#version 410 core
#include "lib/uniforms.glsl"

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vDirection;

void main() {
    // The dome is drawn centred on the eye so it never clips the far plane and
    // never moves relative to the camera, which is what makes it read as sky
    // rather than as an enormous object in the world.
    vec3 world = uCameraPosition + aPosition;
    vDirection = aPosition;
    vec4 clip = uViewProjection * vec4(world, 1.0);
    // Pin to the far plane so every piece of scene geometry wins the depth test.
    gl_Position = clip.xyww;
}
