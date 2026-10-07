#version 410 core
#include "lib/uniforms.glsl"
#include "lib/brdf.glsl"
#include "lib/shadow.glsl"
#include "lib/sky.glsl"
#include "lib/fog.glsl"
#include "lib/noise.glsl"

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vObjectPosition;

out vec4 fragColor;

const float kBallRadius = 0.021335;   // metres, the regulation minimum
const float kDimpleScale = 23.0;
const float kDimpleDepth = 0.17;

/// Perturbs the normal with a field of dimples laid out over the sphere.
///
/// A real ball has a few hundred dimples; what matters visually is that the
/// highlight breaks into a cluster of small speculars instead of one mirror
/// spot, and that the terminator softens.
vec3 dimpled_normal(vec3 normal, vec3 object_position) {
    vec3 unit = normalize(object_position / kBallRadius);

    // Spherical coordinates give an even-enough packing without needing a
    // geodesic layout, and the seam at the pole is below a pixel at any
    // distance the ball is actually seen from.
    float u = atan(unit.z, unit.x) * kInversePi * 0.5 + 0.5;
    float v = acos(clamp(unit.y, -1.0, 1.0)) * kInversePi;
    vec2 grid = vec2(u, v) * kDimpleScale;

    vec2 cell = floor(grid) + step(0.5, fract(vec2(0.0, grid.y))) * vec2(0.5, 0.0);
    vec2 local = fract(grid) - 0.5;
    float radial = length(local) * 2.0;
    float depth = (1.0 - smoothstep(0.55, 1.0, radial));

    // Slope of the dimple bowl, pushed back into the tangent frame.
    vec3 tangent = normalize(cross(vec3(0.0, 1.0, 0.0), unit) + vec3(1e-4));
    vec3 bitangent = cross(unit, tangent);
    vec2 slope = local * depth * kDimpleDepth * 4.0;
    return normalize(normal + (tangent * slope.x + bitangent * slope.y));
}

void main() {
    vec3 view = normalize(uCameraPosition - vWorldPosition);
    vec3 geometric_normal = normalize(vWorldNormal);
    vec3 normal = dimpled_normal(geometric_normal, vObjectPosition);

    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    // Urethane cover: near-white, but never pure white, and scuffed in use.
    surface.albedo = vec3(0.86, 0.86, 0.835);
    surface.roughness = 0.21;
    surface.metallic = 0.0;
    surface.occlusion = 1.0;

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, geometric_normal, sun, 2.0);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
