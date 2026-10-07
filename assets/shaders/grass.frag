#version 410 core
#include "lib/uniforms.glsl"
#include "lib/brdf.glsl"
#include "lib/shadow.glsl"
#include "lib/sky.glsl"
#include "lib/fog.glsl"
#include "lib/noise.glsl"

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vAlong;   // x height fraction, y across, z variation

out vec4 fragColor;

void main() {
    float along = clamp(vAlong.x, 0.0, 1.0);
    float variation = vAlong.z;

    vec3 normal = normalize(vWorldNormal);
    vec3 view = normalize(uCameraPosition - vWorldPosition);
    // The strip is one sided; show the lit face whichever way it is turned.
    if (dot(normal, view) < 0.0) {
        normal = -normal;
    }

    // Blades are darker and bluer at the base, where they are shaded by their
    // neighbours and have less chlorophyll, and lighter towards the tip.
    const vec3 kBaseColor = vec3(0.030, 0.063, 0.022);
    const vec3 kTipColor = vec3(0.118, 0.186, 0.058);
    vec3 albedo = mix(kBaseColor, kTipColor, along * along);
    albedo *= mix(0.72, 1.28, variation);
    albedo *= mix(1.0, 0.68, clamp(uSurfaceWetness, 0.0, 1.0));

    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    surface.albedo = albedo;
    surface.roughness = mix(0.82, 0.48, along);
    surface.metallic = 0.0;
    // The canopy swallows light near the ground. This vertical gradient is
    // what gives a field of blades depth instead of looking like bristles.
    surface.occlusion = mix(0.16, 1.0, along * along);

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, normal, sun, 2.0);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    // A blade is thin enough to light up from behind. This is the single
    // strongest cue that the grass is made of separate leaves.
    float through = pow(clamp(dot(-view, sun), 0.0, 1.0), 2.0);
    radiance += albedo * vec3(1.55, 1.85, 0.70) * through * visibility *
                uSunIlluminance * kInversePi * 0.45 * along;

    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
