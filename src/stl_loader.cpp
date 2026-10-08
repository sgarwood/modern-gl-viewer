#include "mgv/stl_loader.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>

namespace mgv {
namespace {

constexpr std::size_t header_bytes = 80;
constexpr std::size_t triangle_bytes = 50;   // 12 floats + a 2-byte attribute

[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] Vec3 normalized(Vec3 value, Vec3 fallback) noexcept {
    const auto length =
        std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (!std::isfinite(length) || length <= 1.0e-12F) {
        return fallback;
    }
    return {value.x / length, value.y / length, value.z / length};
}

/// A position, quantised so that corners the exporter meant to be the same
/// actually compare equal.
///
/// STL stores every triangle's corners independently, and a vertex shared
/// by eight faces is written eight times. They usually agree bit for bit,
/// having come from one number, but a file that has been through a unit
/// conversion or a repair tool will have drifted in the last place.
struct Key final {
    std::int64_t x{}, y{}, z{};

    [[nodiscard]] bool operator==(const Key&) const noexcept = default;
};

struct KeyHash final {
    [[nodiscard]] std::size_t operator()(const Key& key) const noexcept {
        const auto mix = [](std::size_t seed, std::int64_t value) {
            return seed ^ (static_cast<std::size_t>(value) + 0x9e3779b97f4a7c15ULL +
                           (seed << 6) + (seed >> 2));
        };
        return mix(mix(mix(0, key.x), key.y), key.z);
    }
};

[[nodiscard]] Key quantise(Vec3 position) noexcept {
    constexpr float scale = 1.0e5F;   // a hundredth of a millimetre at metre scale
    return {
        static_cast<std::int64_t>(std::llround(position.x * scale)),
        static_cast<std::int64_t>(std::llround(position.y * scale)),
        static_cast<std::int64_t>(std::llround(position.z * scale)),
    };
}

} // namespace

MeshData load_binary_stl(const std::filesystem::path& path, float crease_degrees) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw std::runtime_error{"Unable to open STL file: " + path.string()};
    }

    std::vector<char> header(header_bytes);
    std::uint32_t declared{};
    if (!file.read(header.data(), header_bytes) ||
        !file.read(reinterpret_cast<char*>(&declared), sizeof(declared))) {
        throw std::runtime_error{"STL file is too short to hold a header: " + path.string()};
    }

    std::vector<char> body(static_cast<std::size_t>(declared) * triangle_bytes);
    if (!body.empty() && !file.read(body.data(), static_cast<std::streamsize>(body.size()))) {
        throw std::runtime_error{
            "STL file claims more triangles than it contains: " + path.string()};
    }

    // Faces first, with the normal taken from the winding. A stored normal
    // is advisory, is frequently zero, and when it disagrees with the
    // winding it is the winding the renderer will believe.
    struct Face final {
        std::array<Vec3, 3> corner{};
        Vec3 normal{};
    };
    std::vector<Face> faces;
    faces.reserve(declared);
    for (std::uint32_t index = 0; index < declared; ++index) {
        const auto* at = body.data() + static_cast<std::size_t>(index) * triangle_bytes;
        std::array<float, 12> values{};
        std::memcpy(values.data(), at, sizeof(values));
        Face face;
        for (int corner = 0; corner < 3; ++corner) {
            face.corner[static_cast<std::size_t>(corner)] = {
                values[static_cast<std::size_t>(3 + corner * 3)],
                values[static_cast<std::size_t>(4 + corner * 3)],
                values[static_cast<std::size_t>(5 + corner * 3)],
            };
        }
        const auto& c = face.corner;
        const Vec3 edge0{c[1].x - c[0].x, c[1].y - c[0].y, c[1].z - c[0].z};
        const Vec3 edge1{c[2].x - c[0].x, c[2].y - c[0].y, c[2].z - c[0].z};
        face.normal = normalized(cross(edge0, edge1), {values[0], values[1], values[2]});
        faces.push_back(face);
    }

    // Which faces meet at each position.
    std::unordered_map<Key, std::vector<std::size_t>, KeyHash> incident;
    incident.reserve(faces.size() * 2);
    for (std::size_t index = 0; index < faces.size(); ++index) {
        for (const auto& corner : faces[index].corner) {
            incident[quantise(corner)].push_back(index);
        }
    }

    const auto crease = std::cos(crease_degrees * 3.14159265F / 180.0F);
    MeshData mesh;
    mesh.vertices.reserve(faces.size() * 3);
    mesh.indices.reserve(faces.size() * 3);

    // A corner is shared only with the faces it actually lies smoothly
    // against. Emitting one vertex per (position, smoothed normal) keeps a
    // sharp edge sharp while still welding the surfaces either side of it.
    std::unordered_map<Key, std::vector<std::pair<Vec3, std::uint32_t>>, KeyHash> emitted;
    for (const auto& face : faces) {
        for (const auto& corner : face.corner) {
            const auto key = quantise(corner);
            Vec3 sum{};
            for (const auto neighbour : incident[key]) {
                const auto& other = faces[neighbour].normal;
                const auto alignment =
                    other.x * face.normal.x + other.y * face.normal.y + other.z * face.normal.z;
                if (alignment >= crease) {
                    sum = {sum.x + other.x, sum.y + other.y, sum.z + other.z};
                }
            }
            const auto normal = normalized(sum, face.normal);

            auto& bucket = emitted[key];
            const auto found = std::ranges::find_if(bucket, [&](const auto& candidate) {
                const auto& existing = candidate.first;
                return existing.x * normal.x + existing.y * normal.y + existing.z * normal.z >
                       0.9999F;
            });
            if (found != bucket.end()) {
                mesh.indices.push_back(found->second);
                continue;
            }
            const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
            Vertex vertex;
            vertex.position = corner;
            vertex.normal = normal;
            mesh.vertices.push_back(vertex);
            mesh.indices.push_back(index);
            bucket.emplace_back(normal, index);
        }
    }
    return mesh;
}

} // namespace mgv
