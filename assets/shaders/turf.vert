#version 410 core
#include "lib/uniforms.glsl"

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vSurface;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vWorldNormal = mat3(uNormalMatrix) * aNormal;
    vSurface = aTexCoord;
    gl_Position = uViewProjection * world;
}
