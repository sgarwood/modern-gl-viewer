#version 410 core
#include "lib/uniforms.glsl"
#include "lib/noise.glsl"

layout(location = 0) in vec3 aPosition;      // unit blade: x across, y 0..1 up
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;      // x across, y along
layout(location = 3) in vec4 aInstancePosition;    // xyz base, w yaw
layout(location = 4) in vec4 aInstanceParameters;  // height, width, variation, lean

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vAlong;        // x height fraction, y across, z variation

mat3 rotate_y(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(vec3(c, 0.0, -s), vec3(0.0, 1.0, 0.0), vec3(s, 0.0, c));
}

mat3 rotate_x(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, c, s), vec3(0.0, -s, c));
}

void main() {
    vec3 base = aInstancePosition.xyz;
    float yaw = aInstancePosition.w;
    float height = aInstanceParameters.x;
    float width = aInstanceParameters.y;
    float variation = aInstanceParameters.z;
    float lean = aInstanceParameters.w;

    float along = aTexCoord.y;

    // --- wind ---------------------------------------------------------------
    // Direction and strength come from a slowly drifting noise field rather
    // than a single sine, so the field ripples in gusts instead of pulsing in
    // unison. The time offset grows with height, which lags the tip behind
    // the base and is what makes a blade look like it is being bent rather
    // than rotated.
    vec2 wind_direction = vec2(sin(uWindDirection), -cos(uWindDirection));
    float gust = value_noise(base.xz * 0.11 + uWindDirection + uTime * 0.19 * max(uWindSpeed, 0.4));
    float turbulence = value_noise(
        base.xz * 0.9 + (uTime + along * along * 0.22) * 0.8 * max(uWindSpeed, 0.4));
    float strength = (0.35 + 0.65 * gust) * (0.5 + 0.5 * turbulence) * uWindSpeed;

    // --- bend ---------------------------------------------------------------
    // The blade curves rather than hinging: displacement grows with the
    // square of the height, so the base stays planted.
    float curve = along * along;
    float bend = lean + strength * 0.26;

    vec3 local = vec3(aPosition.x * width, along * height, 0.0);
    mat3 orientation = rotate_y(yaw) * rotate_x(-bend * curve / max(along, 1e-3) * along);
    vec3 oriented = orientation * local;

    // Push the tip downwind on top of the bend, so gusts travel across the
    // field as visible waves.
    oriented.xz += wind_direction * strength * 0.5 * height * curve;
    // Bending shortens the blade; without this the field appears to grow in
    // the wind.
    oriented.y -= height * curve * bend * bend * 0.35;

    vec3 world = base + oriented;

    // --- normal -------------------------------------------------------------
    // Fanning the normal across the blade's width makes a flat strip read as
    // a rounded leaf and keeps a field of them from flashing all at once.
    mat3 fan = rotate_y(mix(-0.55, 0.55, aTexCoord.x));
    vec3 normal = orientation * fan * vec3(0.0, 0.0, 1.0);

    vWorldPosition = world;
    vWorldNormal = normal;
    vAlong = vec3(along, aTexCoord.x, variation);

    vec4 clip = uViewProjection * vec4(world, 1.0);

    // --- thickening ---------------------------------------------------------
    // A blade seen edge-on is thinner than a pixel and drops out, leaving
    // holes in the field. Widening it in clip space to a minimum of about one
    // pixel keeps coverage without making face-on blades fat.
    float minimum_width = 1.4 * clip.w / max(uViewportSize.x, 1.0);
    vec3 side = orientation * vec3(1.0, 0.0, 0.0);
    vec4 side_clip = uViewProjection * vec4(side, 0.0);
    float projected = length(side_clip.xy) * width * 0.5;
    if (projected < minimum_width && projected > 0.0) {
        float extra = (minimum_width - projected) * sign(aPosition.x);
        clip.xy += normalize(side_clip.xy + 1e-6) * extra;
    }

    gl_Position = clip;
}
