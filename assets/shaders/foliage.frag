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

void main() {
    vec3 geometric_normal = normalize(vWorldNormal);
    vec3 view = normalize(uCameraPosition - vWorldPosition);
    float canopy = clamp(vMaterial.x, 0.0, 1.0);

    // Break the smooth lobes up so they read as masses of leaves rather than
    // as spheres. The perturbation is three-dimensional, so it survives the
    // silhouette being seen from any angle.
    float clump = fbm(vWorldPosition.xz * 2.3 + vWorldPosition.y * 0.7, 3);
    float leaf = fbm(vWorldPosition.xz * 11.0 - vWorldPosition.y * 4.0, 2);
    vec3 normal = normalize(geometric_normal +
                            (vec3(leaf, clump, leaf * clump) - 0.5) * 1.5 * canopy);

    vec3 bark = vec3(0.058, 0.044, 0.034) * mix(0.75, 1.25, clump);
    vec3 leaves = mix(vec3(0.052, 0.112, 0.034), vec3(0.096, 0.158, 0.047), clump);
    // Outer leaves catch more light and are younger, so they are lighter.
    leaves *= mix(0.72, 1.22, leaf);

    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    surface.albedo = mix(bark, leaves, canopy);
    surface.roughness = mix(0.88, 0.62, canopy);
    surface.metallic = 0.0;
    // Deep self-shadowing inside a canopy; the shadow map cannot resolve it.
    surface.occlusion = mix(0.85, mix(0.30, 0.95, clump), canopy);

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, geometric_normal, sun, 2.2);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    // Leaves are thin: light drives straight through them. This is most of
    // what makes a backlit tree look alive.
    float through = pow(clamp(dot(-view, sun), 0.0, 1.0), 2.2);
    radiance += leaves * vec3(1.45, 1.70, 0.62) * through * canopy * visibility *
                uSunIlluminance * kInversePi * 0.30;

    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
