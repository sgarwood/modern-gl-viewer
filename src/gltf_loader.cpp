#include "mgv/gltf_loader.hpp"

// nlohmann/json is vendored for the hardware adapters. Using it here keeps
// it where it belongs: inside an adapter's implementation, never in a header
// and never in the domain types this produces.
#include "mgv/hardware/json.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mgv {
namespace {

using Json = nlohmann::json;

constexpr std::uint32_t glb_magic = 0x46546C67;  // "glTF"
constexpr std::uint32_t glb_json_chunk = 0x4E4F534A;
constexpr std::uint32_t glb_binary_chunk = 0x004E4942;

// glTF component type codes.
constexpr int component_byte = 5120;
constexpr int component_unsigned_byte = 5121;
constexpr int component_short = 5122;
constexpr int component_unsigned_short = 5123;
constexpr int component_unsigned_int = 5125;
constexpr int component_float = 5126;

[[nodiscard]] std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) {
        throw std::runtime_error{"Unable to open glTF: " + path.string()};
    }
    const auto size = static_cast<std::streamsize>(input.tellg());
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(std::max(size, std::streamsize{0})));
    if (!bytes.empty() &&
        !input.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error{"Unable to read glTF: " + path.string()};
    }
    return bytes;
}

[[nodiscard]] std::vector<std::byte> decode_base64(std::string_view text) {
    static constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::byte> out;
    out.reserve(text.size() * 3 / 4);
    std::uint32_t accumulator{};
    int bits{};
    for (const auto character : text) {
        if (character == '=') {
            break;
        }
        const auto index = alphabet.find(character);
        if (index == std::string_view::npos) {
            continue;  // whitespace and line breaks are legal in a data URI
        }
        accumulator = (accumulator << 6) | static_cast<std::uint32_t>(index);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::byte>((accumulator >> bits) & 0xFFU));
        }
    }
    return out;
}

[[nodiscard]] std::size_t component_size(int component_type) {
    switch (component_type) {
    case component_byte:
    case component_unsigned_byte:
        return 1;
    case component_short:
    case component_unsigned_short:
        return 2;
    case component_unsigned_int:
    case component_float:
        return 4;
    default:
        throw std::runtime_error{
            "Unsupported glTF component type " + std::to_string(component_type)};
    }
}

[[nodiscard]] std::size_t component_count(std::string_view type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT4") return 16;
    throw std::runtime_error{"Unsupported glTF accessor type " + std::string{type}};
}

/// Everything needed to walk a document's binary data.
class Document final {
public:
    Document(Json json, std::vector<std::byte> binary_chunk, std::filesystem::path directory)
        // Parentheses, not braces. A braced initialiser with a single
        // nlohmann::json inside it selects the initializer_list constructor
        // in preference to the move constructor, which wraps the document in
        // a one-element array. Every lookup then silently finds nothing.
        : json_(std::move(json)),
          binary_chunk_{std::move(binary_chunk)},
          directory_{std::move(directory)} {
        resolve_buffers();
    }

    [[nodiscard]] const Json& json() const noexcept { return json_; }

    /// Reads an accessor as floats, widening whatever it was stored as.
    ///
    /// Normalized integer attributes are scaled back to their unit range, as
    /// the specification requires; skipping that step is how a weight stored
    /// as a byte becomes 255 instead of 1.
    [[nodiscard]] std::vector<float> read_floats(std::size_t accessor_index) const {
        return read<float>(accessor_index, true);
    }

    /// Reads an accessor as unsigned integers, for indices and joints.
    [[nodiscard]] std::vector<std::uint32_t> read_integers(std::size_t accessor_index) const {
        return read<std::uint32_t>(accessor_index, false);
    }

    [[nodiscard]] std::size_t accessor_count(std::size_t accessor_index) const {
        return accessor(accessor_index).at("count").get<std::size_t>();
    }

    [[nodiscard]] std::size_t accessor_components(std::size_t accessor_index) const {
        return component_count(accessor(accessor_index).at("type").get<std::string>());
    }

private:
    [[nodiscard]] const Json& accessor(std::size_t index) const {
        const auto& accessors = json_.at("accessors");
        if (index >= accessors.size()) {
            throw std::runtime_error{"glTF accessor index out of range"};
        }
        return accessors[index];
    }

    void resolve_buffers() {
        const auto buffers = json_.find("buffers");
        if (buffers == json_.end()) {
            return;
        }
        for (const auto& buffer : *buffers) {
            const auto uri = buffer.find("uri");
            if (uri == buffer.end()) {
                // No URI means the glTF-Binary chunk, which only the first
                // buffer may claim.
                buffers_.push_back(binary_chunk_);
                continue;
            }
            const auto text = uri->get<std::string>();
            constexpr std::string_view data_prefix = "data:";
            if (text.starts_with(data_prefix)) {
                const auto comma = text.find(',');
                if (comma == std::string::npos) {
                    throw std::runtime_error{"Malformed glTF data URI"};
                }
                buffers_.push_back(decode_base64(std::string_view{text}.substr(comma + 1)));
                continue;
            }
            buffers_.push_back(read_file(directory_ / text));
        }
    }

    template <typename Target>
    [[nodiscard]] std::vector<Target> read(std::size_t accessor_index, bool scale_normalized) const {
        const auto& entry = accessor(accessor_index);
        const auto count = entry.at("count").get<std::size_t>();
        const auto components = component_count(entry.at("type").get<std::string>());
        const auto component_type = entry.at("componentType").get<int>();
        const auto element_size = component_size(component_type);
        const auto normalized =
            scale_normalized && entry.value("normalized", false);

        std::vector<Target> out(count * components, Target{});

        const auto view_index = entry.find("bufferView");
        if (view_index == entry.end()) {
            // An accessor with no view reads as zeroes, which is what the
            // specification says and what a sparse accessor's base is.
            return out;
        }
        const auto& view = json_.at("bufferViews").at(view_index->get<std::size_t>());
        const auto buffer_index = view.at("buffer").get<std::size_t>();
        if (buffer_index >= buffers_.size()) {
            throw std::runtime_error{"glTF buffer index out of range"};
        }
        const auto& buffer = buffers_[buffer_index];

        const auto view_offset = view.value("byteOffset", std::size_t{0});
        const auto accessor_offset = entry.value("byteOffset", std::size_t{0});
        const auto packed = element_size * components;
        const auto stride = view.value("byteStride", packed);
        const auto begin = view_offset + accessor_offset;

        if (count > 0 && begin + (count - 1) * stride + packed > buffer.size()) {
            throw std::runtime_error{"glTF accessor reads past the end of its buffer"};
        }

        for (std::size_t element = 0; element < count; ++element) {
            const auto* source = buffer.data() + begin + element * stride;
            for (std::size_t component = 0; component < components; ++component) {
                const auto* at = source + component * element_size;
                auto& target = out[element * components + component];
                switch (component_type) {
                case component_float: {
                    float value{};
                    std::memcpy(&value, at, sizeof(value));
                    target = static_cast<Target>(value);
                    break;
                }
                case component_unsigned_byte: {
                    std::uint8_t value{};
                    std::memcpy(&value, at, sizeof(value));
                    target = normalized
                        ? static_cast<Target>(static_cast<float>(value) / 255.0F)
                        : static_cast<Target>(value);
                    break;
                }
                case component_byte: {
                    std::int8_t value{};
                    std::memcpy(&value, at, sizeof(value));
                    target = normalized
                        ? static_cast<Target>(std::max(static_cast<float>(value) / 127.0F, -1.0F))
                        : static_cast<Target>(value);
                    break;
                }
                case component_unsigned_short: {
                    std::uint16_t value{};
                    std::memcpy(&value, at, sizeof(value));
                    target = normalized
                        ? static_cast<Target>(static_cast<float>(value) / 65535.0F)
                        : static_cast<Target>(value);
                    break;
                }
                case component_short: {
                    std::int16_t value{};
                    std::memcpy(&value, at, sizeof(value));
                    target = normalized
                        ? static_cast<Target>(std::max(static_cast<float>(value) / 32767.0F, -1.0F))
                        : static_cast<Target>(value);
                    break;
                }
                case component_unsigned_int: {
                    std::uint32_t value{};
                    std::memcpy(&value, at, sizeof(value));
                    target = static_cast<Target>(value);
                    break;
                }
                default:
                    throw std::runtime_error{"Unsupported glTF component type"};
                }
            }
        }
        return out;
    }

    Json json_;
    std::vector<std::byte> binary_chunk_;
    std::filesystem::path directory_;
    std::vector<std::vector<std::byte>> buffers_;
};

[[nodiscard]] Document open_document(const std::filesystem::path& path) {
    auto bytes = read_file(path);
    const auto directory = path.parent_path();

    if (bytes.size() >= 12) {
        std::uint32_t magic{};
        std::memcpy(&magic, bytes.data(), sizeof(magic));
        if (magic == glb_magic) {
            std::uint32_t total{};
            std::memcpy(&total, bytes.data() + 8, sizeof(total));
            Json json;
            std::vector<std::byte> binary;
            std::size_t offset = 12;
            while (offset + 8 <= std::min<std::size_t>(total, bytes.size())) {
                std::uint32_t length{};
                std::uint32_t type{};
                std::memcpy(&length, bytes.data() + offset, sizeof(length));
                std::memcpy(&type, bytes.data() + offset + 4, sizeof(type));
                const auto* chunk = bytes.data() + offset + 8;
                if (offset + 8 + length > bytes.size()) {
                    throw std::runtime_error{"glTF-Binary chunk runs past the end of the file"};
                }
                if (type == glb_json_chunk) {
                    json = Json::parse(
                        reinterpret_cast<const char*>(chunk),
                        reinterpret_cast<const char*>(chunk) + length);
                } else if (type == glb_binary_chunk) {
                    binary.assign(chunk, chunk + length);
                }
                offset += 8 + length;
                offset += (4 - offset % 4) % 4;
            }
            if (json.is_null()) {
                throw std::runtime_error{"glTF-Binary file has no JSON chunk"};
            }
            return Document{std::move(json), std::move(binary), directory};
        }
    }

    return Document{
        Json::parse(
            reinterpret_cast<const char*>(bytes.data()),
            reinterpret_cast<const char*>(bytes.data()) + bytes.size()),
        {},
        directory};
}

[[nodiscard]] std::vector<ImportedMaterial> read_materials(const Json& json) {
    std::vector<ImportedMaterial> materials;
    const auto entries = json.find("materials");
    if (entries == json.end()) {
        return materials;
    }
    materials.reserve(entries->size());
    for (const auto& entry : *entries) {
        ImportedMaterial material;
        material.name = entry.value("name", std::string{});
        const auto pbr = entry.find("pbrMetallicRoughness");
        if (pbr != entry.end()) {
            const auto factor = pbr->find("baseColorFactor");
            if (factor != pbr->end() && factor->size() >= 4) {
                material.diffuse_color = {
                    (*factor)[0].get<float>(),
                    (*factor)[1].get<float>(),
                    (*factor)[2].get<float>(),
                };
                material.opacity = (*factor)[3].get<float>();
            }
        }
        materials.push_back(std::move(material));
    }
    return materials;
}

} // namespace

ImportedModel GltfLoader::load(const std::filesystem::path& path) const {
    const auto document = open_document(path);
    const auto& json = document.json();

    ImportedModel model;
    model.materials = read_materials(json);

    const auto meshes = json.find("meshes");
    if (meshes == json.end()) {
        throw std::runtime_error{"glTF contains no meshes: " + path.string()};
    }

    for (const auto& mesh : *meshes) {
        for (const auto& primitive : mesh.at("primitives")) {
            // Only triangles. glTF's default mode is 4, which is triangles.
            if (primitive.value("mode", 4) != 4) {
                continue;
            }
            const auto& attributes = primitive.at("attributes");
            const auto position = attributes.find("POSITION");
            if (position == attributes.end()) {
                continue;
            }

            const auto positions = document.read_floats(position->get<std::size_t>());
            const auto count = document.accessor_count(position->get<std::size_t>());

            const auto read_optional = [&](const char* name) {
                const auto found = attributes.find(name);
                return found == attributes.end()
                    ? std::vector<float>{}
                    : document.read_floats(found->get<std::size_t>());
            };
            const auto normals = read_optional("NORMAL");
            const auto uvs = read_optional("TEXCOORD_0");

            ImportedPrimitive imported;
            imported.mesh.vertices.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                Vertex vertex;
                vertex.position = {
                    positions[index * 3], positions[index * 3 + 1], positions[index * 3 + 2]};
                if (normals.size() >= (index + 1) * 3) {
                    vertex.normal = {
                        normals[index * 3], normals[index * 3 + 1], normals[index * 3 + 2]};
                }
                if (uvs.size() >= (index + 1) * 2) {
                    vertex.tex_coord = {uvs[index * 2], uvs[index * 2 + 1]};
                }
                imported.mesh.vertices.push_back(vertex);
            }

            const auto joints = attributes.find("JOINTS_0");
            const auto weights = attributes.find("WEIGHTS_0");
            if (joints != attributes.end() && weights != attributes.end()) {
                const auto joint_values = document.read_integers(joints->get<std::size_t>());
                const auto weight_values = document.read_floats(weights->get<std::size_t>());
                imported.mesh.skinning.reserve(count);
                for (std::size_t index = 0; index < count; ++index) {
                    SkinningVertex influence;
                    for (std::size_t slot = 0; slot < 4; ++slot) {
                        const auto at = index * 4 + slot;
                        if (at < joint_values.size()) {
                            influence.joints[slot] =
                                static_cast<std::uint16_t>(joint_values[at]);
                        }
                        influence.weights[slot] =
                            at < weight_values.size() ? weight_values[at] : 0.0F;
                    }
                    // glTF requires the weights to sum to one, and exporters
                    // do not always deliver it. Normalising here means the
                    // shader never has to, and a mesh that was exported a
                    // percent light does not come out a percent shrunken.
                    const auto total = influence.weights[0] + influence.weights[1] +
                                       influence.weights[2] + influence.weights[3];
                    if (total > 1.0e-6F) {
                        for (auto& weight : influence.weights) {
                            weight /= total;
                        }
                    } else {
                        influence.weights = {1.0F, 0.0F, 0.0F, 0.0F};
                    }
                    imported.mesh.skinning.push_back(influence);
                }
            }

            const auto indices = primitive.find("indices");
            if (indices != primitive.end()) {
                for (const auto index : document.read_integers(indices->get<std::size_t>())) {
                    imported.mesh.indices.push_back(index);
                }
            } else {
                imported.mesh.indices.resize(count);
                for (std::size_t index = 0; index < count; ++index) {
                    imported.mesh.indices[index] = static_cast<std::uint32_t>(index);
                }
            }

            const auto material = primitive.find("material");
            if (material != primitive.end()) {
                imported.material_index = material->get<std::size_t>();
            }
            model.primitives.push_back(std::move(imported));
        }
    }

    const auto skins = json.find("skins");
    if (skins != json.end() && !skins->empty()) {
        const auto& skin = (*skins)[0];
        ImportedSkin imported;
        const auto& nodes = json.at("nodes");
        for (const auto& joint : skin.at("joints")) {
            const auto node_index = joint.get<std::size_t>();
            const auto& node = nodes.at(node_index);
            // A joint with no name is still a joint; name it after its node
            // so the binding has something unambiguous to match on.
            imported.joint_names.push_back(
                node.value("name", "joint_" + std::to_string(node_index)));
        }

        const auto matrices = skin.find("inverseBindMatrices");
        if (matrices != skin.end()) {
            const auto values = document.read_floats(matrices->get<std::size_t>());
            const auto joints = imported.joint_names.size();
            imported.inverse_bind_matrices.reserve(joints);
            for (std::size_t joint = 0; joint < joints; ++joint) {
                Mat4 matrix{};
                for (std::size_t element = 0; element < 16; ++element) {
                    const auto at = joint * 16 + element;
                    matrix[element] = at < values.size() ? values[at] : 0.0F;
                }
                imported.inverse_bind_matrices.push_back(matrix);
            }
        } else {
            // Absent inverse bind matrices mean identity, per the spec.
            imported.inverse_bind_matrices.assign(
                imported.joint_names.size(), identity_matrix);
        }
        model.skin = std::move(imported);
    }

    if (model.primitives.empty()) {
        throw std::runtime_error{"glTF contains no triangle geometry: " + path.string()};
    }
    return model;
}

} // namespace mgv
