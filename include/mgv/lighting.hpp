#pragma once

#include "mgv/mesh.hpp"

namespace mgv {

/// A single directional light, expressed in linear radiometric units.
///
/// `direction` points *from* the surface *towards* the light, matching the
/// convention used by the lighting shaders, so a sun overhead is `{0, 1, 0}`.
struct DirectionalLight final {
    // A late-morning sun: 50 degrees above the horizon, bearing 140 degrees.
    Vec3 direction{0.4131759F, 0.7660444F, 0.4924039F};
    Vec3 color{1.0F, 0.95F, 0.86F};
    float illuminance{90'000.0F};

    friend bool operator==(const DirectionalLight&, const DirectionalLight&) = default;
};

/// Sky and atmosphere parameters shared by every shader in a frame.
///
/// Radiances are linear and absolute; the tone mapper applies `exposure` at the
/// end of the frame, so materials never pre-scale their own output.
struct Environment final {
    DirectionalLight sun;
    Vec3 sky_zenith_color{0.18F, 0.32F, 0.62F};
    Vec3 sky_horizon_color{0.62F, 0.70F, 0.80F};
    Vec3 ground_albedo{0.12F, 0.14F, 0.09F};
    float sky_illuminance{28'000.0F};
    float turbidity{3.0F};
    float fog_density{0.0045F};
    float fog_height_falloff{0.12F};
    float exposure{1.0F};
    float wind_speed{0.0F};
    float wind_direction_radians{0.0F};
    float surface_wetness{0.0F};

    friend bool operator==(const Environment&, const Environment&) = default;
};

/// Normalizes `direction`, returning the default sun direction when the input
/// has no usable length.
[[nodiscard]] Vec3 normalized_light_direction(Vec3 direction) noexcept;

/// Builds a sun direction from compass bearing and elevation, both in degrees.
///
/// `azimuth_degrees` is measured clockwise from -Z (north), matching the
/// meteorological convention already used by the weather commands.
[[nodiscard]] Vec3 sun_direction_from_angles(
    float azimuth_degrees,
    float elevation_degrees) noexcept;

} // namespace mgv
