#include "mgv/obj_loader.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mgv {
namespace {

struct VertexKey final {
    std::size_t position{};
    std::optional<std::size_t> tex_coord;
    std::optional<std::size_t> normal;

    friend bool operator==(const VertexKey&, const VertexKey&) = default;
};

struct VertexKeyHash final {
    [[nodiscard]] std::size_t operator()(const VertexKey& key) const noexcept {
        constexpr auto mix = std::size_t{0x9e3779b9};
        auto hash = key.position;
        hash ^= key.tex_coord.value_or(std::numeric_limits<std::size_t>::max()) + mix + (hash << 6U) + (hash >> 2U);
        hash ^= key.normal.value_or(std::numeric_limits<std::size_t>::max()) + mix + (hash << 6U) + (hash >> 2U);
        return hash;
    }
};

[[nodiscard]] Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Vec3& operator+=(Vec3& lhs, const Vec3& rhs) {
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    return lhs;
}

[[nodiscard]] Vec3 normalized(const Vec3& value) {
    const auto length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= std::numeric_limits<float>::epsilon()) {
        return {0.0F, 0.0F, 1.0F};
    }
    return {value.x / length, value.y / length, value.z / length};
}

[[noreturn]] void fail(std::string_view source, std::size_t line, const std::string& message) {
    throw std::runtime_error{std::string{source} + ":" + std::to_string(line) + ": " + message};
}

[[nodiscard]] int parse_index(std::string_view text, std::string_view source, std::size_t line) {
    if (text.empty()) {
        fail(source, line, "missing OBJ index");
    }
    int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
        fail(source, line, "invalid OBJ index '" + std::string{text} + "'");
    }
    return value;
}

[[nodiscard]] std::size_t resolve_index(
    int index,
    std::size_t count,
    std::string_view kind,
    std::string_view source,
    std::size_t line) {
    const auto resolved = index > 0
        ? static_cast<long long>(index - 1)
        : static_cast<long long>(count) + static_cast<long long>(index);
    if (resolved < 0 || resolved >= static_cast<long long>(count)) {
        fail(source, line, std::string{kind} + " index is out of range");
    }
    return static_cast<std::size_t>(resolved);
}

[[nodiscard]] VertexKey parse_key(
    std::string_view token,
    std::size_t position_count,
    std::size_t tex_coord_count,
    std::size_t normal_count,
    std::string_view source,
    std::size_t line) {
    const auto first_slash = token.find('/');
    const auto second_slash = first_slash == std::string_view::npos
        ? std::string_view::npos
        : token.find('/', first_slash + 1);

    const auto position_text = token.substr(0, first_slash);
    VertexKey key{
        .position = resolve_index(
            parse_index(position_text, source, line), position_count, "position", source, line),
        .tex_coord = std::nullopt,
        .normal = std::nullopt,
    };

    if (first_slash != std::string_view::npos) {
        const auto tex_begin = first_slash + 1;
        const auto tex_end = second_slash == std::string_view::npos ? token.size() : second_slash;
        const auto tex_text = token.substr(tex_begin, tex_end - tex_begin);
        if (!tex_text.empty()) {
            key.tex_coord = resolve_index(
                parse_index(tex_text, source, line), tex_coord_count, "texture coordinate", source, line);
        }
    }

    if (second_slash != std::string_view::npos) {
        const auto normal_text = token.substr(second_slash + 1);
        if (!normal_text.empty()) {
            key.normal = resolve_index(
                parse_index(normal_text, source, line), normal_count, "normal", source, line);
        }
    }
    return key;
}

} // namespace

struct ObjLoader::Impl final {
    [[nodiscard]] MeshData parse(std::istream& input, std::string_view source_name) const {
        std::vector<Vec3> positions;
        std::vector<Vec2> tex_coords;
        std::vector<Vec3> normals;
        MeshData mesh;
        std::vector<bool> has_explicit_normal;
        std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> vertex_cache;

        std::string line_text;
        std::size_t line_number{};
        while (std::getline(input, line_text)) {
            ++line_number;
            const auto comment = line_text.find('#');
            if (comment != std::string::npos) {
                line_text.erase(comment);
            }

            std::istringstream line{line_text};
            std::string command;
            line >> command;
            if (command.empty()) {
                continue;
            }

            if (command == "v") {
                Vec3 value;
                if (!(line >> value.x >> value.y >> value.z)) {
                    fail(source_name, line_number, "position requires three numbers");
                }
                positions.push_back(value);
            } else if (command == "vt") {
                Vec2 value;
                if (!(line >> value.x >> value.y)) {
                    fail(source_name, line_number, "texture coordinate requires two numbers");
                }
                tex_coords.push_back(value);
            } else if (command == "vn") {
                Vec3 value;
                if (!(line >> value.x >> value.y >> value.z)) {
                    fail(source_name, line_number, "normal requires three numbers");
                }
                normals.push_back(normalized(value));
            } else if (command == "f") {
                std::vector<std::uint32_t> face;
                std::string token;
                while (line >> token) {
                    const auto key = parse_key(
                        token, positions.size(), tex_coords.size(), normals.size(), source_name, line_number);
                    const auto existing = vertex_cache.find(key);
                    if (existing != vertex_cache.end()) {
                        face.push_back(existing->second);
                        continue;
                    }

                    if (mesh.vertices.size() >= std::numeric_limits<std::uint32_t>::max()) {
                        fail(source_name, line_number, "mesh exceeds 32-bit index capacity");
                    }
                    const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
                    mesh.vertices.push_back({
                        .position = positions[key.position],
                        .normal = key.normal ? normals[*key.normal] : Vec3{},
                        .tex_coord = key.tex_coord ? tex_coords[*key.tex_coord] : Vec2{},
                    });
                    has_explicit_normal.push_back(key.normal.has_value());
                    vertex_cache.emplace(key, index);
                    face.push_back(index);
                }

                if (face.size() < 3) {
                    fail(source_name, line_number, "face requires at least three vertices");
                }
                for (std::size_t index = 1; index + 1 < face.size(); ++index) {
                    mesh.indices.insert(mesh.indices.end(), {face[0], face[index], face[index + 1]});
                }
            }
        }

        if (input.bad()) {
            throw std::runtime_error{"Unable to read OBJ: " + std::string{source_name}};
        }
        if (mesh.empty()) {
            throw std::runtime_error{"OBJ contains no renderable faces: " + std::string{source_name}};
        }

        for (std::size_t index = 0; index < mesh.indices.size(); index += 3) {
            const auto a = mesh.indices[index];
            const auto b = mesh.indices[index + 1];
            const auto c = mesh.indices[index + 2];
            const auto face_normal = cross(
                subtract(mesh.vertices[b].position, mesh.vertices[a].position),
                subtract(mesh.vertices[c].position, mesh.vertices[a].position));
            for (const auto vertex_index : {a, b, c}) {
                if (!has_explicit_normal[vertex_index]) {
                    mesh.vertices[vertex_index].normal += face_normal;
                }
            }
        }
        for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
            if (!has_explicit_normal[index]) {
                mesh.vertices[index].normal = normalized(mesh.vertices[index].normal);
            }
        }
        return mesh;
    }
};

ObjLoader::ObjLoader() : impl_{std::make_unique<Impl>()} {}
ObjLoader::~ObjLoader() = default;
ObjLoader::ObjLoader(ObjLoader&&) noexcept = default;
ObjLoader& ObjLoader::operator=(ObjLoader&&) noexcept = default;

MeshData ObjLoader::load(const std::filesystem::path& path) const {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"Unable to open OBJ: " + path.string()};
    }
    return impl_->parse(input, path.string());
}

MeshData ObjLoader::parse(std::istream& input, std::string_view source_name) const {
    return impl_->parse(input, source_name);
}

} // namespace mgv
