#include "mgv/shader_loader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

namespace {

class TemporaryFile final {
public:
    TemporaryFile(std::string_view name, std::string_view contents)
        : path_{std::filesystem::temp_directory_path() / name} {
        std::ofstream output{path_};
        output << contents;
    }

    ~TemporaryFile() { std::filesystem::remove(path_); }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("shader loader reads custom source files at runtime") {
    const TemporaryFile vertex{"mgv-test.vert", "#version 410 core\nvoid main() {}\n"};
    const TemporaryFile fragment{"mgv-test.frag", "#version 410 core\nout vec4 color;\n"};

    const auto sources = mgv::ShaderLoader::load(vertex.path(), fragment.path());

    CHECK(sources.vertex.find("void main") != std::string::npos);
    CHECK(sources.fragment.find("out vec4") != std::string::npos);
    CHECK(sources.vertex_name == vertex.path().string());
    CHECK(sources.fragment_name == fragment.path().string());
    CHECK(sources.language == mgv::ShaderSourceLanguage::glsl);
}

TEST_CASE("shader loader rejects an empty source file") {
    const TemporaryFile vertex{"mgv-empty.vert", ""};
    const TemporaryFile fragment{"mgv-valid.frag", "void main() {}"};

    CHECK_THROWS_WITH(
        mgv::ShaderLoader::load(vertex.path(), fragment.path()),
        Catch::Matchers::ContainsSubstring("empty"));
}

TEST_CASE("shader loader identifies a missing source file") {
    const TemporaryFile fragment{"mgv-present.frag", "void main() {}"};
    const auto missing = std::filesystem::temp_directory_path() / "mgv-does-not-exist.vert";

    CHECK_THROWS_WITH(
        mgv::ShaderLoader::load(missing, fragment.path()),
        Catch::Matchers::ContainsSubstring(missing.string()));
}
