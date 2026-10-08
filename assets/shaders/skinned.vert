#version 410 core
#include "lib/uniforms.glsl"

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 5) in uvec4 aJoints;
layout(location = 6) in vec4 aWeights;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vTexCoord;

// Must not exceed the backend's reported max_skinning_joints, which is
// derived from the vertex stage's uniform budget.
const int kMaxJoints = 64;
uniform mat4 uJoints[kMaxJoints];
uniform int uJointCount;

void main() {
    // Linear blend skinning: the weighted sum of the vertex transformed by
    // each influencing joint. The weights are normalised at import, so there
    // is nothing to renormalise here.
    //
    // Falls back to the bind pose when there is no palette, so a skinned
    // mesh drawn before its animation is bound appears in its rest position
    // rather than collapsing to the origin.
    mat4 skin = mat4(0.0);
    float total = 0.0;
    for (int influence = 0; influence < 4; ++influence) {
        int joint = int(aJoints[influence]);
        float weight = aWeights[influence];
        if (weight <= 0.0 || joint >= uJointCount || joint >= kMaxJoints) {
            continue;
        }
        skin += uJoints[joint] * weight;
        total += weight;
    }
    if (total <= 0.0) {
        skin = mat4(1.0);
    }

    vec4 skinned = skin * vec4(aPosition, 1.0);
    vec4 world = uModel * skinned;

    // The joint matrices rotate and scale as well as translate, so normals
    // have to go through them too. Using the rigid normal matrix alone
    // leaves a character lit as though it never moved.
    mat3 skin_normal = mat3(skin);
    vWorldPosition = world.xyz;
    vWorldNormal = mat3(uNormalMatrix) * (skin_normal * aNormal);
    vTexCoord = aTexCoord;

    gl_Position = uViewProjection * world;
}
