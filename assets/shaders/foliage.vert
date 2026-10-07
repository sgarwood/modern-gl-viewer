#version 410 core
#include "lib/uniforms.glsl"
#include "lib/noise.glsl"

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vMaterial;   // x: 0 bark, 1 canopy. y: height along the part.

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);

    // Canopy sway. The whole crown leans with the wind and the outer foliage
    // lags behind it, which is what makes a tree read as flexible rather than
    // as a carved object. Bark at the base does not move at all.
    float flexibility = aTexCoord.x * clamp(aPosition.y / 6.0, 0.0, 1.0);
    if (flexibility > 0.0) {
        vec2 wind = vec2(sin(uWindDirection), -cos(uWindDirection));
        float phase = uTime * (0.6 + uWindSpeed * 0.22) + dot(world.xz, vec2(0.07, 0.11));
        float gust = 0.5 + 0.5 * sin(phase * 0.31);
        float sway = sin(phase) * 0.45 + sin(phase * 2.3) * 0.18;
        float amplitude = flexibility * (0.045 + uWindSpeed * 0.022) * mix(0.55, 1.0, gust);
        world.xz += wind * sway * amplitude * uWindSpeed;
        world.y -= abs(sway) * amplitude * uWindSpeed * 0.3;
    }

    vWorldPosition = world.xyz;
    vWorldNormal = mat3(uNormalMatrix) * aNormal;
    vMaterial = aTexCoord;
    gl_Position = uViewProjection * world;
}
