#include "mgv/shader_loader.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mgv {
namespace {

[[nodiscard]] std::string read_source(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Unable to open shader: " + path.string()};
    }

    std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (source.empty()) {
        throw std::runtime_error{"Shader source is empty: " + path.string()};
    }
    if (!input.eof() && input.fail()) {
        throw std::runtime_error{"Unable to read shader: " + path.string()};
    }
    return source;
}

[[nodiscard]] std::string_view trimmed(std::string_view value) {
    constexpr std::string_view whitespace{" \t\r\f\v"};
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

/// Returns the quoted path of an `#include "..."` directive, or nothing when
/// the line is not one.
[[nodiscard]] std::optional<std::string> include_target(std::string_view line) {
    constexpr std::string_view directive{"#include"};
    const auto content = trimmed(line);
    if (!content.starts_with(directive)) {
        return std::nullopt;
    }
    const auto argument = trimmed(content.substr(directive.size()));
    if (argument.size() < 2 || argument.front() != '"' || argument.back() != '"') {
        throw std::runtime_error{
            "Shader include expects a quoted path: " + std::string{content}};
    }
    return std::string{argument.substr(1, argument.size() - 2)};
}

class SourceExpander final {
public:
    /// Expands `path`, substituting every `#include` with the contents of the
    /// named file resolved relative to the including file's directory.
    ///
    /// A module already pulled in is skipped rather than repeated, so shared
    /// modules need no header guards. `#line` directives are emitted so the
    /// driver's compile errors still name the file the code came from.
    [[nodiscard]] std::string expand(const std::filesystem::path& path) {
        const auto canonical = weakly_canonical(path);
        if (std::ranges::find(open_, canonical) != open_.end()) {
            throw std::runtime_error{"Circular shader include: " + path.string()};
        }
        if (!expanded_.insert(canonical.string()).second) {
            return {};
        }

        open_.push_back(canonical);
        const auto source = read_source(path);
        const auto index = source_index(canonical);

        std::ostringstream output;
        std::istringstream input{source};
        std::string line;
        int number{1};
        bool emit_line_directive{false};
        while (std::getline(input, line)) {
            const auto target = include_target(line);
            if (!target) {
                if (emit_line_directive) {
                    output << "#line " << number << ' ' << index << '\n';
                    emit_line_directive = false;
                }
                output << line << '\n';
                ++number;
                continue;
            }

            const auto resolved = path.parent_path() / *target;
            if (!std::filesystem::exists(resolved)) {
                throw std::runtime_error{
                    "Unable to open shader include \"" + *target + "\" from " + path.string()};
            }
            const auto included = expand(resolved);
            if (!included.empty()) {
                output << "#line 1 " << source_index(weakly_canonical(resolved)) << '\n'
                       << included;
            }
            emit_line_directive = true;
            ++number;
        }

        open_.pop_back();
        return output.str();
    }

private:
    [[nodiscard]] static std::filesystem::path weakly_canonical(const std::filesystem::path& path) {
        std::error_code error;
        auto result = std::filesystem::weakly_canonical(path, error);
        return error ? path.lexically_normal() : result;
    }

    /// GLSL `#line` takes a numeric source index rather than a filename, so
    /// every file visited is assigned one in order of first appearance.
    [[nodiscard]] int source_index(const std::filesystem::path& path) {
        const auto existing = std::ranges::find(indices_, path.string());
        if (existing != indices_.end()) {
            return static_cast<int>(std::distance(indices_.begin(), existing));
        }
        indices_.push_back(path.string());
        return static_cast<int>(indices_.size() - 1);
    }

    std::unordered_set<std::string> expanded_;
    std::vector<std::filesystem::path> open_;
    std::vector<std::string> indices_;
};

} // namespace

ShaderSources ShaderLoader::load(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path) {
    return {
        .vertex = SourceExpander{}.expand(vertex_path),
        .fragment = SourceExpander{}.expand(fragment_path),
        .vertex_name = vertex_path.string(),
        .fragment_name = fragment_path.string(),
    };
}

} // namespace mgv
