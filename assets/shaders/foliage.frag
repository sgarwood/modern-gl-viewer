#version 410 core
#include "lib/uniforms.glsl"
#include "lib/brdf.glsl"
#include "lib/shadow.glsl"
#include "lib/sky.glsl"
#include "lib/fog.glsl"
#include "lib/noise.glsl"

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vMaterial;

out vec4 fragColor;

// Set per species by the material instance: the same for every tree of a
// kind, so it costs one uniform rather than a vertex attribute repeated
// across a hundred thousand vertices.
uniform vec4 uBarkColor;
uniform vec4 uLeafShadeColor;
uniform vec4 uLeafSunColor;
/// x: how much light a leaf lets through; y: how open the crown is. A pine
/// needle transmits far less than a broad leaf, and a conifer crown is dense
/// enough that very little gets inside it.
uniform vec4 uFoliageResponse;

/// Value noise that does not favour any axis.
///
/// Sampling a 2D field with the third axis folded into it smears the result
/// into diagonal streaks, which on a crown of leaves reads as combed hair.
/// Averaging three planar projections costs little and has no such grain.
float foliage_noise(vec3 position, float scale) {
    return (fbm(position.xz * scale, 2) +
            fbm(position.xy * scale * 1.13 + 19.7, 2) +
            fbm(position.zy * scale * 0.87 - 7.3, 2)) * (1.0 / 3.0);
}

void main() {
    vec3 geometric_normal = normalize(vWorldNormal);
    vec3 view = normalize(uCameraPosition - vWorldPosition);
    float canopy = clamp(vMaterial.x, 0.0, 1.0);

    // Break the smooth lobes up so they read as masses of leaves rather than
    // as spheres. Three projections, so the perturbation survives the
    // silhouette being seen from any angle.
    float clump = foliage_noise(vWorldPosition, 1.9);
    float leaf = foliage_noise(vWorldPosition, 9.0);

    // Bark is the one surface that does want a grain: fissures run up a
    // trunk, so this is sampled with a long vertical wavelength and a short
    // one around the circumference.
    float fissure = fbm(vec2(vWorldPosition.x + vWorldPosition.z,
                             vWorldPosition.y * 0.16) * 24.0, 2);

    vec3 normal = normalize(
        geometric_normal +
        (vec3(leaf, clump, leaf * clump) - 0.5) * 1.7 * canopy +
        vec3(fissure - 0.5, 0.0, fissure - 0.5) * 0.55 * (1.0 - canopy));

    vec3 bark = uBarkColor.rgb * mix(0.62, 1.30, fissure);
    vec3 leaves = mix(uLeafShadeColor.rgb, uLeafSunColor.rgb, clump);
    // Outer leaves catch more light and are younger, so they are lighter.
    leaves *= mix(0.72, 1.22, leaf);

    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    surface.albedo = mix(bark, leaves, canopy);
    surface.roughness = mix(0.92, 0.62, canopy);
    surface.metallic = 0.0;
    // Deep self-shadowing inside a canopy; the shadow map cannot resolve it.
    float openness = max(uFoliageResponse.y, 0.05);
    surface.occlusion = mix(0.85, mix(0.30 * openness, 0.95, clump), canopy);

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, geometric_normal, sun, 2.2);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    // Leaves are thin: light drives straight through them. This is most of
    // what makes a backlit tree look alive.
    float through = pow(clamp(dot(-view, sun), 0.0, 1.0), 2.2);
    radiance += leaves * vec3(1.45, 1.70, 0.62) * through * canopy * visibility *
                uSunIlluminance * kInversePi * 0.30 * max(uFoliageResponse.x, 0.0);

    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
