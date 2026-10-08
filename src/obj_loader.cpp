#include "mgv/obj_loader.hpp"

#include <algorithm>
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

struct MeshBuilder final {
    std::optional<std::string> material_name;
    MeshData mesh;
    std::vector<bool> has_explicit_normal;
    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> vertex_cache;
};

struct ParsedObj final {
    std::vector<MeshBuilder> primitives;
    std::vector<std::string> material_libraries;
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

[[nodiscard]] std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
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
    VertexKey key{
        .position = resolve_index(
            parse_index(token.substr(0, first_slash), source, line), position_count, "position", source, line),
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

void finish_normals(MeshBuilder& builder) {
    for (std::size_t index = 0; index < builder.mesh.indices.size(); index += 3) {
        const auto a = builder.mesh.indices[index];
        const auto b = builder.mesh.indices[index + 1];
        const auto c = builder.mesh.indices[index + 2];
        const auto face_normal = cross(
            subtract(builder.mesh.vertices[b].position, builder.mesh.vertices[a].position),
            subtract(builder.mesh.vertices[c].position, builder.mesh.vertices[a].position));
        for (const auto vertex_index : {a, b, c}) {
            if (!builder.has_explicit_normal[vertex_index]) {
                builder.mesh.vertices[vertex_index].normal += face_normal;
            }
        }
    }
    for (std::size_t index = 0; index < builder.mesh.vertices.size(); ++index) {
        if (!builder.has_explicit_normal[index]) {
            builder.mesh.vertices[index].normal = normalized(builder.mesh.vertices[index].normal);
        }
    }
}

[[nodiscard]] ParsedObj parse_obj(
    std::istream& input,
    std::string_view source_name,
    bool split_materials) {
    std::vector<Vec3> positions;
    std::vector<Vec2> tex_coords;
    std::vector<Vec3> normals;
    ParsedObj parsed;
    parsed.primitives.emplace_back();

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
        } else if (command == "mtllib" && split_materials) {
            std::string library;
            bool found{};
            while (line >> library) {
                parsed.material_libraries.push_back(std::move(library));
                found = true;
            }
            if (!found) {
                fail(source_name, line_number, "mtllib requires a path");
            }
        } else if (command == "usemtl" && split_materials) {
            std::string name;
            std::getline(line, name);
            name = trim(std::move(name));
            if (name.empty()) {
                fail(source_name, line_number, "usemtl requires a material name");
            }
            auto& current = parsed.primitives.back();
            if (current.mesh.empty()) {
                current.material_name = std::move(name);
            } else if (current.material_name != name) {
                parsed.primitives.emplace_back();
                parsed.primitives.back().material_name = std::move(name);
            }
        } else if (command == "f") {
            auto& builder = parsed.primitives.back();
            std::vector<std::uint32_t> face;
            std::string token;
            while (line >> token) {
                const auto key = parse_key(
                    token, positions.size(), tex_coords.size(), normals.size(), source_name, line_number);
                const auto existing = builder.vertex_cache.find(key);
                if (existing != builder.vertex_cache.end()) {
                    face.push_back(existing->second);
                    continue;
                }
                if (builder.mesh.vertices.size() >= std::numeric_limits<std::uint32_t>::max()) {
                    fail(source_name, line_number, "mesh exceeds 32-bit index capacity");
                }
                const auto index = static_cast<std::uint32_t>(builder.mesh.vertices.size());
                builder.mesh.vertices.push_back({
                    .position = positions[key.position],
                    .normal = key.normal ? normals[*key.normal] : Vec3{},
                    .tex_coord = key.tex_coord ? tex_coords[*key.tex_coord] : Vec2{},
                });
                builder.has_explicit_normal.push_back(key.normal.has_value());
                builder.vertex_cache.emplace(key, index);
                face.push_back(index);
            }
            if (face.size() < 3) {
                fail(source_name, line_number, "face requires at least three vertices");
            }
            for (std::size_t index = 1; index + 1 < face.size(); ++index) {
                builder.mesh.indices.insert(
                    builder.mesh.indices.end(), {face[0], face[index], face[index + 1]});
            }
        }
    }
    if (input.bad()) {
        throw std::runtime_error{"Unable to read OBJ: " + std::string{source_name}};
    }
    std::erase_if(parsed.primitives, [](const MeshBuilder& value) { return value.mesh.empty(); });
    if (parsed.primitives.empty()) {
        throw std::runtime_error{"OBJ contains no renderable faces: " + std::string{source_name}};
    }
    for (auto& builder : parsed.primitives) {
        finish_normals(builder);
    }
    return parsed;
}

[[nodiscard]] std::vector<ImportedMaterial> load_material_library(
    const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"Unable to open MTL: " + path.string()};
    }
    std::vector<ImportedMaterial> materials;
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
        if (command == "newmtl") {
            std::string name;
            std::getline(line, name);
            name = trim(std::move(name));
            if (name.empty()) {
                fail(path.string(), line_number, "newmtl requires a material name");
            }
            // Built field by field: naming members in a designated
            // initialiser and leaving the rest out trips -Wmissing-field-
            // initializers every time the struct gains one.
            ImportedMaterial material;
            material.name = std::move(name);
            materials.push_back(std::move(material));
            continue;
        }
        if (materials.empty()) {
            continue;
        }
        auto& material = materials.back();
        if (command == "Kd") {
            if (!(line >> material.diffuse_color.x >> material.diffuse_color.y >> material.diffuse_color.z)) {
                fail(path.string(), line_number, "Kd requires three numbers");
            }
        } else if (command == "d") {
            if (!(line >> material.opacity) || material.opacity < 0.0F || material.opacity > 1.0F) {
                fail(path.string(), line_number, "d requires a number between zero and one");
            }
        } else if (command == "Tr") {
            float transparency{};
            if (!(line >> transparency) || transparency < 0.0F || transparency > 1.0F) {
                fail(path.string(), line_number, "Tr requires a number between zero and one");
            }
            material.opacity = 1.0F - transparency;
        } else if (command == "map_Kd") {
            std::string texture_path;
            std::getline(line, texture_path);
            texture_path = trim(std::move(texture_path));
            if (texture_path.empty()) {
                fail(path.string(), line_number, "map_Kd requires a path");
            }
            material.diffuse_texture = (path.parent_path() / texture_path).lexically_normal();
        }
    }
    if (input.bad()) {
        throw std::runtime_error{"Unable to read MTL: " + path.string()};
    }
    return materials;
}

} // namespace

struct ObjLoader::Impl final {
    [[nodiscard]] MeshData parse(std::istream& input, std::string_view source_name) const {
        auto parsed = parse_obj(input, source_name, false);
        return std::move(parsed.primitives.front().mesh);
    }

    [[nodiscard]] ImportedModel load_model(const std::filesystem::path& path) const {
        std::ifstream input{path};
        if (!input) {
            throw std::runtime_error{"Unable to open OBJ: " + path.string()};
        }
        auto parsed = parse_obj(input, path.string(), true);
        ImportedModel model;
        std::unordered_map<std::string, std::size_t> material_indices;
        for (const auto& library : parsed.material_libraries) {
            auto materials = load_material_library((path.parent_path() / library).lexically_normal());
            for (auto& material : materials) {
                if (material_indices.contains(material.name)) {
                    throw std::runtime_error{"Duplicate MTL material: " + material.name};
                }
                material_indices.emplace(material.name, model.materials.size());
                model.materials.push_back(std::move(material));
            }
        }
        model.primitives.reserve(parsed.primitives.size());
        for (auto& primitive : parsed.primitives) {
            std::optional<std::size_t> material_index;
            if (primitive.material_name) {
                const auto found = material_indices.find(*primitive.material_name);
                if (found == material_indices.end()) {
                    throw std::runtime_error{"OBJ references unknown material: " + *primitive.material_name};
                }
                material_index = found->second;
            }
            model.primitives.push_back({
                .mesh = std::move(primitive.mesh),
                .material_index = material_index,
            });
        }
        return model;
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

ImportedModel ObjLoader::load_model(const std::filesystem::path& path) const {
    return impl_->load_model(path);
}

MeshData ObjLoader::parse(std::istream& input, std::string_view source_name) const {
    return impl_->parse(input, source_name);
}

} // namespace mgv
