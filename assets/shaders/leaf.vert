#version 410 core
#include "lib/uniforms.glsl"

layout(location = 0) in vec3 aPosition;              // unit leaf, flat, +Z
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;              // y runs stem to tip
layout(location = 3) in vec4 aInstancePosition;      // xyz, w yaw
layout(location = 4) in vec4 aInstanceParameters;    // size, tilt, colour, depth

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vLeaf;   // x along, y colour variation, z depth in the pile

void main() {
    float size = max(aInstanceParameters.x, 1e-5);
    float tilt = aInstanceParameters.y;
    float yaw = aInstancePosition.w;
    float along = aTexCoord.y;

    // A fallen leaf is not flat: it dries into a shallow curl, and that curl
    // is most of why a drift catches light in flecks rather than as a sheet.
    vec3 local = aPosition;
    float across = local.x;
    local.y += (across * across * 5.5 + along * along * 0.10) * 0.55;

    float tilt_sine = sin(tilt);
    float tilt_cosine = cos(tilt);
    vec3 tilted = vec3(
        local.x,
        local.y * tilt_cosine - local.z * tilt_sine,
        local.y * tilt_sine + local.z * tilt_cosine);

    float sine = sin(yaw);
    float cosine = cos(yaw);
    vec3 oriented = vec3(
        tilted.x * cosine + tilted.z * sine,
        tilted.y,
        -tilted.x * sine + tilted.z * cosine);

    vec3 world = aInstancePosition.xyz + oriented * size;

    // The curl bends the normal away from straight up, which is what keeps a
    // drift from flashing as one surface when the sun crosses it.
    vec3 curled = normalize(vec3(-across * 11.0 * 0.55, 1.0, -along * 0.2 * 0.55));
    vec3 tilted_normal = vec3(
        curled.x,
        curled.y * tilt_cosine - curled.z * tilt_sine,
        curled.y * tilt_sine + curled.z * tilt_cosine);

    vWorldPosition = world;
    vWorldNormal = vec3(
        tilted_normal.x * cosine + tilted_normal.z * sine,
        tilted_normal.y,
        -tilted_normal.x * sine + tilted_normal.z * cosine);
    vLeaf = vec3(along, aInstanceParameters.z, aInstanceParameters.w);

    gl_Position = uViewProjection * vec4(world, 1.0);
}
