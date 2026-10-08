#include "mgv/lighting.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>

namespace mgv {
namespace {

constexpr Vec3 default_direction{0.4131759F, 0.7660444F, 0.4924039F};
constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0F;

} // namespace

Vec3 normalized_light_direction(Vec3 direction) noexcept {
    const auto length = std::sqrt(
        direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (!(length > 1.0e-6F)) {
        return default_direction;
    }
    return {direction.x / length, direction.y / length, direction.z / length};
}

Vec3 sun_direction_from_angles(float azimuth_degrees, float elevation_degrees) noexcept {
    const auto azimuth = azimuth_degrees * degrees_to_radians;
    const auto elevation = elevation_degrees * degrees_to_radians;
    const auto horizontal = std::cos(elevation);
    return normalized_light_direction({
        horizontal * std::sin(azimuth),
        std::sin(elevation),
        -horizontal * std::cos(azimuth),
    });
}

namespace {

[[nodiscard]] Vec3 cross(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

[[nodiscard]] float dot(const Vec3& lhs, const Vec3& rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vec3 normalized(const Vec3& value) noexcept {
    const auto magnitude = std::sqrt(dot(value, value));
    if (!(magnitude > 1.0e-6F)) {
        return {0.0F, 1.0F, 0.0F};
    }
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

[[nodiscard]] Vec3 add(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] Vec3 subtract(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] Vec3 scaled(const Vec3& value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] float length(const Vec3& value) noexcept {
    return std::sqrt(dot(value, value));
}

[[nodiscard]] std::array<Vec3, 8> frustum_slice_corners(
    const Camera& camera,
    float aspect_ratio,
    float near_distance,
    float far_distance) {
    const auto forward = normalized(subtract(camera.target(), camera.position()));
    const auto right = normalized(cross(forward, camera.up()));
    const auto up = cross(right, forward);
    const auto half_fov = camera.vertical_field_of_view_degrees() *
                          std::numbers::pi_v<float> / 360.0F;
    const auto tangent = std::tan(half_fov);
    const auto near_height = tangent * near_distance;
    const auto near_width = near_height * aspect_ratio;
    const auto far_height = tangent * far_distance;
    const auto far_width = far_height * aspect_ratio;
    const auto near_center = add(camera.position(), scaled(forward, near_distance));
    const auto far_center = add(camera.position(), scaled(forward, far_distance));
    const auto corner = [&](Vec3 center, float width, float height, float x, float y) {
        return add(add(center, scaled(right, width * x)), scaled(up, height * y));
    };
    return {
        corner(near_center, near_width, near_height, -1.0F, -1.0F),
        corner(near_center, near_width, near_height, 1.0F, -1.0F),
        corner(near_center, near_width, near_height, 1.0F, 1.0F),
        corner(near_center, near_width, near_height, -1.0F, 1.0F),
        corner(far_center, far_width, far_height, -1.0F, -1.0F),
        corner(far_center, far_width, far_height, 1.0F, -1.0F),
        corner(far_center, far_width, far_height, 1.0F, 1.0F),
        corner(far_center, far_width, far_height, -1.0F, 1.0F),
    };
}

} // namespace

Mat4 directional_light_view_projection(
    Vec3 light_direction,
    const ShadowVolume& volume,
    ClipSpaceConvention convention) {
    const auto radius = std::max(volume.radius, 1.0e-3F);
    const auto resolution = std::max(volume.resolution, 1);

    // The light looks along -forward, from caster_distance behind the centre.
    const auto forward = normalized_light_direction(light_direction);
    // Any axis not parallel to the light will do for the up reference; +Y is
    // the natural choice except when the sun is directly overhead.
    const Vec3 reference = std::abs(forward.y) > 0.999F
        ? Vec3{0.0F, 0.0F, 1.0F}
        : Vec3{0.0F, 1.0F, 0.0F};
    const auto right = normalized(cross(reference, forward));
    const auto up = cross(forward, right);

    // Snap the centre to whole texels along the light's own axes. A fraction
    // of a texel of drift is enough to make every shadow edge crawl.
    const auto texel = 2.0F * radius / static_cast<float>(resolution);
    const auto snap = [texel](float value) { return std::floor(value / texel) * texel; };
    const auto centre_right = snap(dot(volume.centre, right));
    const auto centre_up = snap(dot(volume.centre, up));
    const auto centre_forward = dot(volume.centre, forward);

    // The light sits caster_distance along +forward, which points towards the
    // sun, and looks back down -forward at the volume.
    const auto eye_forward = centre_forward + volume.caster_distance;

    // Rows are the light's basis vectors with the translation expressed in
    // that basis. The third row is +forward, not -forward: a look-at view
    // matrix's third row is the negated viewing direction, and the light
    // views along -forward. Getting that sign wrong still produces a matrix
    // that passes every containment check while ordering depth backwards, so
    // that nothing ever occludes anything.
    Mat4 view{};
    view[0] = right.x;   view[4] = right.y;   view[8] = right.z;    view[12] = -centre_right;
    view[1] = up.x;      view[5] = up.y;      view[9] = up.z;       view[13] = -centre_up;
    view[2] = forward.x; view[6] = forward.y; view[10] = forward.z; view[14] = -eye_forward;
    view[3] = 0.0F;      view[7] = 0.0F;      view[11] = 0.0F;      view[15] = 1.0F;

    const auto near_plane = 0.0F;
    const auto far_plane = volume.caster_distance + radius * 4.0F;

    Mat4 projection{};
    projection[0] = 1.0F / radius;
    projection[5] = (convention.invert_y ? -1.0F : 1.0F) / radius;
    projection[15] = 1.0F;
    if (convention.depth_range == ClipDepthRange::zero_to_one) {
        projection[10] = -1.0F / (far_plane - near_plane);
        projection[14] = -near_plane / (far_plane - near_plane);
    } else {
        projection[10] = -2.0F / (far_plane - near_plane);
        projection[14] = -(far_plane + near_plane) / (far_plane - near_plane);
    }

    Mat4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result[column * 4 + row] +=
                    projection[inner * 4 + row] * view[column * 4 + inner];
            }
        }
    }
    return result;
}

std::vector<ShadowCascade> directional_light_cascades(
    const Camera& camera,
    float aspect_ratio,
    Vec3 light_direction,
    const ShadowVolume& settings,
    ClipSpaceConvention convention) {
    if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0F ||
        settings.cascade_count == 0 || settings.cascade_count > maximum_shadow_cascades ||
        !std::isfinite(settings.maximum_distance) ||
        settings.maximum_distance <= camera.near_plane() ||
        !std::isfinite(settings.split_lambda) || settings.split_lambda < 0.0F ||
        settings.split_lambda > 1.0F || settings.resolution <= 0 ||
        !std::isfinite(settings.caster_distance) || settings.caster_distance <= 0.0F) {
        throw std::invalid_argument{"Invalid directional shadow cascade settings"};
    }

    const auto near_distance = camera.near_plane();
    const auto far_distance = std::min(camera.far_plane(), settings.maximum_distance);
    std::vector<ShadowCascade> cascades;
    cascades.reserve(settings.cascade_count);
    auto previous_split = near_distance;
    for (std::uint32_t index = 0; index < settings.cascade_count; ++index) {
        const auto ratio = static_cast<float>(index + 1U) /
                           static_cast<float>(settings.cascade_count);
        const auto logarithmic = near_distance *
                                 std::pow(far_distance / near_distance, ratio);
        const auto uniform = near_distance + (far_distance - near_distance) * ratio;
        auto split = settings.split_lambda * logarithmic +
                     (1.0F - settings.split_lambda) * uniform;
        if (index + 1U == settings.cascade_count) {
            split = far_distance;
        }

        const auto corners = frustum_slice_corners(
            camera, aspect_ratio, previous_split, split);
        Vec3 centre{};
        for (const auto& corner : corners) {
            centre = add(centre, corner);
        }
        centre = scaled(centre, 1.0F / static_cast<float>(corners.size()));
        auto radius = 0.0F;
        for (const auto& corner : corners) {
            radius = std::max(radius, length(subtract(corner, centre)));
        }
        // Quantizing the extent avoids tiny floating-point changes resizing
        // the texel grid as the camera moves.
        radius = std::ceil(radius * 16.0F) / 16.0F;
        auto volume = settings;
        volume.centre = centre;
        volume.radius = radius;
        cascades.push_back({
            .view_projection = directional_light_view_projection(
                light_direction, volume, convention),
            .split_depth = split,
        });
        previous_split = split;
    }
    return cascades;
}

} // namespace mgv
