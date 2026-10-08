#version 410 core
#include "lib/uniforms.glsl"
#include "lib/brdf.glsl"
#include "lib/shadow.glsl"
#include "lib/sky.glsl"
#include "lib/fog.glsl"

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vTexCoord;

out vec4 fragColor;

uniform vec4 uBaseColorFactor;

void main() {
    vec3 normal = normalize(vWorldNormal);
    vec3 view = normalize(uCameraPosition - vWorldPosition);
    if (!gl_FrontFacing) {
        normal = -normal;
    }

    Surface surface;
    surface.position = vWorldPosition;
    surface.normal = normal;
    surface.view = view;
    surface.albedo = uBaseColorFactor.rgb;
    surface.roughness = clamp(uBaseColorFactor.a, 0.04, 1.0);
    surface.metallic = 0.0;
    surface.occlusion = 1.0;

    vec3 sun = normalize(uSunDirection);
    float visibility = sun_visibility(vWorldPosition, normal, sun, 1.6);
    vec3 radiance = direct_lighting(surface, sun, uSunColor, uSunIlluminance) * visibility;
    radiance += hemisphere_ambient(
        surface, uSkyZenithColor, uSkyHorizonColor, uGroundAlbedo, uSkyIlluminance);

    float transmittance = fog_transmittance(
        vWorldPosition, uCameraPosition, uFogDensity, uFogHeightFalloff);
    vec3 in_scatter = sky_radiance(-view, sun, uTurbidity, uGroundAlbedo);
    radiance = apply_fog(radiance, in_scatter, transmittance);

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
