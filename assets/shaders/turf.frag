#version 410 core
#include "lib/uniforms.glsl"
#include "lib/brdf.glsl"
#include "lib/shadow.glsl"
#include "lib/sky.glsl"
#include "lib/fog.glsl"
#include "lib/noise.glsl"

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vSurface;   // x: height of cut in metres, y: mow signal in [-1, 1]

out vec4 fragColor;

// Must match Engine::Impl::max_wet_trail_points.
const int kMaxTrailPoints = 32;
uniform int uTrailCount;
uniform vec3 uTrailPositions[kMaxTrailPoints];

// A green and a fairway are mown across each other. That difference in
// direction separates them at a glance far more than any difference in hue.
const vec2 kFairwayMowAxis = vec2(0.0, 1.0);
const vec2 kGreenMowAxis = vec2(1.0, 0.0);
const float kFairwayStripeWidth = 2.75;   // metres
const float kGreenStripeWidth = 1.15;
const float kBladeLeanRadians = 0.145;

// Reference cut heights, in metres, for normalising the cut into a 0..1
// "shagginess" that the rest of the shader is written against.
const float kShavedCut = 0.0032;
const float kRoughCut = 0.062;

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
/// is lying. Tilting the shading normal across each band reproduces the light
/// and dark bands, and makes them swap over as the camera or the sun moves,
/// exactly as they do on a mown surface.
vec3 apply_mower_lay(vec3 normal, vec2 axis, float width, float strength) {
    if (strength <= 0.0) {
        return normal;
    }
    float along = dot(vWorldPosition.xz, axis);
    // A softened square wave: flat through the middle of each band with a
    // short transition at the wheel line.
    float lean = sin(along / width * 2.0 * kPi);
    lean = sign(lean) * smoothstep(0.0, 0.55, abs(lean));

    vec3 lay = normalize(vec3(axis.x, 0.0, axis.y));
    return normalize(normal + lay * tan(kBladeLeanRadians * lean * strength));
}

/// How dry the turf is here, given where the ball has rolled. A ball rolling
/// through dew leaves a darker, drier track behind it.
float trail_dryness() {
    const float kTrackRadius = 0.11;
    float dryness = 0.0;
    for (int i = 0; i < uTrailCount && i < kMaxTrailPoints; ++i) {
        float distance_to_track = length(vWorldPosition.xz - uTrailPositions[i].xz);
        dryness = max(dryness, 1.0 - smoothstep(0.0, kTrackRadius, distance_to_track));
    }
    return dryness;
}

void main() {
    vec3 geometric_normal = normalize(vWorldNormal);
    vec3 view = normalize(uCameraPosition - vWorldPosition);

    float cut = max(vSurface.x, 0.0);
    float mow = clamp(vSurface.y, -1.0, 1.0);
    // 0 for a shaved putting surface, 1 for deep rough. Square-rooted because
    // the visual difference between 3 mm and 13 mm of grass is far larger
    // than between 50 mm and 60 mm.
    float shag = sqrt(clamp((cut - kShavedCut) / (kRoughCut - kShavedCut), 0.0, 1.0));
    float green_cut = max(-mow, 0.0);
    float fairway_cut = max(mow, 0.0);

    float footprint = pixel_footprint();

    // --- colour ------------------------------------------------------------
    // Long grass clumps; short grass cannot. The clump scale grows with the
    // cut, so rough breaks into visible tussocks while a green stays even.
    float clump_scale = mix(1.6, 0.42, shag);
    float clump = fbm(vWorldPosition.xz * clump_scale, 3);
    float fine = fbm(vWorldPosition.xz * mix(9.0, 4.0, shag), 2);

    // Bentgrass on a green is cut so short that it reads bright and slightly
    // yellow; rough is deeper, bluer and much darker, because most of what
    // the eye sees between the blades is shadow.
    const vec3 kShavedAlbedo = vec3(0.128, 0.205, 0.062);
    const vec3 kRoughAlbedo = vec3(0.043, 0.082, 0.038);
    vec3 albedo = mix(kShavedAlbedo, kRoughAlbedo, shag);

    // Patchiness scales with the cut for the same reason.
    albedo *= mix(1.0, mix(0.70, 1.34, clump), shag);

    // Wet turf is darker and glossier; water fills the gaps between blades.
    float wetness = clamp(uSurfaceWetness, 0.0, 1.0) * (1.0 - trail_dryness());
    albedo *= mix(1.0, 0.66, wetness);

    // --- normals -----------------------------------------------------------
    // Relief scales with the cut too: a green is a plane, rough is lumpy.
    float detail_frequency = mix(9.0, 4.0, shag);
    float detail_resolved = resolve(footprint, 1.0 / detail_frequency);
    const float kGradientStep = 0.035;
    float height_x = fbm((vWorldPosition.xz + vec2(kGradientStep, 0.0)) * detail_frequency, 2) - fine;
    float height_z = fbm((vWorldPosition.xz + vec2(0.0, kGradientStep)) * detail_frequency, 2) - fine;
    vec3 normal = normalize(geometric_normal +
                            vec3(-height_x, 0.0, -height_z) * mix(0.7, 7.5, shag * shag) *
                            detail_resolved);

    // Stripes are metres wide, so they survive to far more of the frame than
    // the blade detail does and need their own filtering scale. The green and
    // the fairway each get their own axis and width.
    normal = apply_mower_lay(
        normal, normalize(kGreenMowAxis), kGreenStripeWidth,
        green_cut * resolve(footprint, kGreenStripeWidth * 2.0));
    normal = apply_mower_lay(
        normal, normalize(kFairwayMowAxis), kFairwayStripeWidth,
        fairway_cut * resolve(footprint, kFairwayStripeWidth * 2.0));

    // --- shading -----------------------------------------------------------
    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    surface.albedo = albedo * mix(1.0, mix(0.90, 1.12, fine), detail_resolved);
    // A shaved green has a real sheen along the grain; rough has none.
    surface.roughness = mix(mix(0.58, 0.96, shag), 0.36, wetness);
    surface.metallic = 0.0;
    // The dominant cue. Blades shade each other, and deep rough is mostly
    // shadow between stems: this is why rough looks dark from any angle and
    // in any light, where a tint only works under one.
    surface.occlusion = mix(0.95, 0.34, shag) * mix(1.0, mix(0.72, 1.0, clump), shag);

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, geometric_normal, sun, 2.6);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    // Light passing through the blades when the sun is low and behind them.
    // Long grass transmits far more of it, which is why rough catches fire at
    // sunset while a green stays flat.
    float transmission = pow(clamp(dot(-view, sun), 0.0, 1.0), 3.0);
    float grazing = 1.0 - clamp(sun.y, 0.0, 1.0);
    radiance += albedo * vec3(1.30, 1.55, 0.52) * transmission * grazing * visibility *
                uSunIlluminance * kInversePi * mix(0.10, 0.52, shag);

    // --- atmosphere --------------------------------------------------------
    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
