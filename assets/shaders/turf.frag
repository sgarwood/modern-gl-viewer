#version 410 core
#include "lib/uniforms.glsl"
#include "lib/brdf.glsl"
#include "lib/shadow.glsl"
#include "lib/sky.glsl"
#include "lib/fog.glsl"
#include "lib/noise.glsl"

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vSurfaceClass;   // x: putting surface, y: mown approach

out vec4 fragColor;

// Must match Engine::Impl::max_wet_trail_points.
const int kMaxTrailPoints = 32;
uniform int uTrailCount;
uniform vec3 uTrailPositions[kMaxTrailPoints];

// The direction the mower ran, in the ground plane. Stripes lie across it.
const vec2 kMowAxis = vec2(0.0, 1.0);
const float kGreenStripeWidth = 1.35;    // metres
const float kApproachStripeWidth = 2.6;
const float kBladeLeanRadians = 0.125;
const float kDetailFrequency = 6.0;      // cycles per metre

/// The world-space size of one pixel on this surface. Any feature smaller
/// than this has to be faded out, or it turns into crawling static as the
/// camera moves. It grows with distance and, much faster, with grazing angle.
float pixel_footprint() {
    return max(
        length(vec2(dFdx(vWorldPosition.x), dFdx(vWorldPosition.z))),
        length(vec2(dFdy(vWorldPosition.x), dFdy(vWorldPosition.z))));
}

/// How much of a feature of the given wavelength survives filtering.
float resolve(float footprint, float wavelength) {
    return clamp(1.0 - 2.0 * footprint / max(wavelength, 1e-4), 0.0, 1.0);
}

/// A stripe is not a change of colour: it is a change in which way the grass
/// is lying. Tilting the shading normal towards or away from the mower's
/// travel reproduces the light and dark bands, and makes them swap over as
/// the camera or the sun moves, exactly as they do on a mown surface.
vec3 apply_mower_lay(vec3 normal, float along, float width, float strength) {
    if (strength <= 0.0) {
        return normal;
    }
    float band = along / width;
    // A smooth square wave: the lay flips between bands, but the transition
    // is spread over the width of the mower's wheel rather than a hard edge.
    // A softened sine: flat through the middle of each band with a short
    // transition at the wheel line, rather than a hard square edge.
    float lean = sin(band * 2.0 * kPi);
    lean = sign(lean) * smoothstep(0.0, 0.55, abs(lean));

    vec3 lay_axis = normalize(vec3(kMowAxis.x, 0.0, kMowAxis.y));
    return normalize(normal + lay_axis * tan(kBladeLeanRadians * lean * strength));
}

/// How dry the turf is here, given where the ball has rolled. A ball rolling
/// through dew leaves a darker, drier track behind it.
float trail_dryness(vec3 world_position) {
    const float kTrackRadius = 0.11;
    float dryness = 0.0;
    for (int i = 0; i < uTrailCount && i < kMaxTrailPoints; ++i) {
        float distance_to_track = length(world_position.xz - uTrailPositions[i].xz);
        dryness = max(dryness, 1.0 - smoothstep(0.0, kTrackRadius, distance_to_track));
    }
    return dryness;
}

void main() {
    vec3 geometric_normal = normalize(vWorldNormal);
    vec3 view = normalize(uCameraPosition - vWorldPosition);

    float green = clamp(vSurfaceClass.x, 0.0, 1.0);
    float approach = clamp(vSurfaceClass.y, 0.0, 1.0);
    float mown = max(green, approach);
    float rough = 1.0 - mown;

    float footprint = pixel_footprint();

    // --- colour ------------------------------------------------------------
    float patchiness = fbm(vWorldPosition.xz * 0.21, 3);
    float fine = fbm(vWorldPosition.xz * kDetailFrequency, 2);

    const vec3 kGreenAlbedo = vec3(0.077, 0.152, 0.053);
    const vec3 kApproachAlbedo = vec3(0.094, 0.171, 0.058);
    const vec3 kRoughAlbedo = vec3(0.083, 0.133, 0.050);

    vec3 albedo = mix(kRoughAlbedo, kApproachAlbedo, approach);
    albedo = mix(albedo, kGreenAlbedo, green);
    // Broad patchiness: strong in the rough, barely there on a tended green.
    albedo *= mix(1.0, mix(0.84, 1.18, patchiness), mix(1.0, 0.18, green));

    // Wet turf is darker and glossier; water fills the gaps between blades.
    float dryness = trail_dryness(vWorldPosition);
    float wetness = clamp(uSurfaceWetness, 0.0, 1.0) * (1.0 - dryness);
    albedo *= mix(1.0, 0.66, wetness);

    // --- normals -----------------------------------------------------------
    float detail_resolved = resolve(footprint, 1.0 / kDetailFrequency);
    const float kGradientStep = 0.035;
    float height_x = fbm((vWorldPosition.xz + vec2(kGradientStep, 0.0)) * kDetailFrequency, 2) - fine;
    float height_z = fbm((vWorldPosition.xz + vec2(0.0, kGradientStep)) * kDetailFrequency, 2) - fine;
    vec3 normal = normalize(geometric_normal +
                            vec3(-height_x, 0.0, -height_z) * mix(4.5, 1.6, mown) * detail_resolved);

    // Stripes are metres wide, so they survive to far more of the frame than
    // the blade detail does and need their own filtering scale.
    float along = dot(vWorldPosition.xz, normalize(kMowAxis));
    float stripe_width = mix(kApproachStripeWidth, kGreenStripeWidth, green);
    float stripes_resolved = resolve(footprint, stripe_width * 2.0);
    normal = apply_mower_lay(normal, along, stripe_width, mown * stripes_resolved);

    // --- shading -----------------------------------------------------------
    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    surface.albedo = albedo * mix(1.0, mix(0.93, 1.07, fine), detail_resolved);
    surface.roughness = mix(mix(0.90, 0.80, approach), 0.70, green);
    surface.roughness = mix(surface.roughness, 0.38, wetness);
    surface.metallic = 0.0;
    // Blades shade each other near the ground, and the rough is deeper.
    surface.occlusion = mix(0.60, 0.86, mown);

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, geometric_normal, sun, 2.6);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    // Light passing through the blades when the sun is low and behind them.
    // Without it, backlit turf reads as dead flat.
    float transmission = pow(clamp(dot(-view, sun), 0.0, 1.0), 3.0);
    float grazing = 1.0 - clamp(sun.y, 0.0, 1.0);
    radiance += albedo * vec3(1.30, 1.55, 0.52) * transmission * grazing *
                uSunIlluminance * kInversePi * 0.22 * mix(0.5, 1.0, mown);

    // --- atmosphere --------------------------------------------------------
    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
