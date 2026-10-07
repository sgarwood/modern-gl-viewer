#version 410 core
#include "lib/uniforms.glsl"
#include "lib/sky.glsl"
#include "lib/noise.glsl"

in vec3 vDirection;
out vec4 fragColor;

void main() {
    vec3 direction = normalize(vDirection);
    vec3 sun = normalize(uSunDirection);

    vec3 radiance = sky_radiance(direction, sun, uTurbidity, uGroundAlbedo);
    radiance += sun_disc(direction, sun, uSunColor, uSunIlluminance);

    // A narrow band of aerial haze right at the horizon, thickening with
    // turbidity. Kept tight: widening it washes the whole lower sky out.
    float horizon = exp(-abs(direction.y) * 55.0);
    vec3 haze = mix(radiance, vec3(luminance(radiance)) * vec3(1.02, 1.01, 1.0), 0.35);
    radiance = mix(radiance, haze, horizon * clamp(uTurbidity / 10.0, 0.08, 0.6));

    fragColor = vec4(max(radiance, vec3(0.0)), 1.0);
}
