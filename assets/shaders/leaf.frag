#version 410 core
#include "lib/uniforms.glsl"
#include "lib/brdf.glsl"
#include "lib/shadow.glsl"
#include "lib/sky.glsl"
#include "lib/fog.glsl"

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vLeaf;   // x along, y colour variation, z depth in the pile

out vec4 fragColor;

void main() {
    float along = clamp(vLeaf.x, 0.0, 1.0);
    float variation = clamp(vLeaf.y, 0.0, 1.0);
    float depth = clamp(vLeaf.z, 0.0, 1.0);

    vec3 normal = normalize(vWorldNormal);
    vec3 view = normalize(uCameraPosition - vWorldPosition);
    // A leaf has two faces and both are seen in a drift.
    if (dot(normal, view) < 0.0) {
        normal = -normal;
    }

    // Autumn is not one colour. Leaves fall over weeks and dry at their own
    // rate, so a drift runs from the last of the green through ochre and
    // russet to the dark brown of the ones that fell first.
    const vec3 kFading = vec3(0.132, 0.118, 0.028);   // yellowing
    const vec3 kRusset = vec3(0.148, 0.055, 0.016);
    const vec3 kSpent = vec3(0.046, 0.028, 0.015);    // damp and rotting
    vec3 albedo = variation < 0.55
        ? mix(kFading, kRusset, variation / 0.55)
        : mix(kRusset, kSpent, (variation - 0.55) / 0.45);
    // Edges dry and curl first, so a leaf is darker at its tip than its stem.
    albedo *= mix(1.08, 0.80, along);
    albedo *= mix(1.0, 0.72, clamp(uSurfaceWetness, 0.0, 1.0));

    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    surface.albedo = albedo;
    // Dry leaves are matt; wet ones are the glossiest thing on the course.
    surface.roughness = mix(0.74, 0.30, clamp(uSurfaceWetness, 0.0, 1.0));
    surface.metallic = 0.0;
    // Buried leaves sit in their neighbours' shade. This gradient is what
    // gives a drift depth instead of looking like confetti on a lawn.
    surface.occlusion = mix(0.10, 1.0, depth * depth);

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, normal, sun, 1.8);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    // A dead leaf is thin and papery, and glows when the sun is behind it.
    float through = pow(clamp(dot(-view, sun), 0.0, 1.0), 2.4);
    radiance += albedo * vec3(1.70, 1.15, 0.45) * through * visibility *
                uSunIlluminance * kInversePi * 0.55 * depth;

    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
