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

/// The blade's spine, as a quadratic Bezier in its own plane: upright at the
/// base, leaning by `lean` at the tip.
///
/// A curve rather than a rotation, because a rotated blade pivots about its
/// root and a real one bends along its length. Having the spine in closed
/// form also gives the tangent exactly, which is what the normal needs; the
/// alternative is differencing two sample points and hoping.
vec3 blade_spine(float t, float height, float lean) {
    vec3 base = vec3(0.0);
    vec3 control = vec3(0.0, height * 0.52, height * sin(lean) * 0.14);
    vec3 tip = vec3(0.0, height * cos(lean), height * sin(lean));
    float u = 1.0 - t;
    return u * u * base + 2.0 * u * t * control + t * t * tip;
}

vec3 blade_tangent(float t, float height, float lean) {
    vec3 base = vec3(0.0);
    vec3 control = vec3(0.0, height * 0.52, height * sin(lean) * 0.14);
    vec3 tip = vec3(0.0, height * cos(lean), height * sin(lean));
    return normalize(2.0 * (1.0 - t) * (control - base) + 2.0 * t * (tip - control));
}

mat3 rotate_y(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(vec3(c, 0.0, -s), vec3(0.0, 1.0, 0.0), vec3(s, 0.0, c));
}

void main() {
    vec3 base = aInstancePosition.xyz;
    float height = aInstanceParameters.x;
    float width = aInstanceParameters.y;
    float variation = aInstanceParameters.z;
    float rest_lean = aInstanceParameters.w;

    float along = aTexCoord.y;

    // --- wind ---------------------------------------------------------------
    // Direction and strength come from a slowly drifting noise field rather
    // than a single sine, so the field ripples in gusts instead of pulsing in
    // unison. The time offset grows with height, which lags the tip behind
    // the base.
    float gust = value_noise(base.xz * 0.11 + uWindDirection + uTime * 0.19 * max(uWindSpeed, 0.4));
    float turbulence = value_noise(
        base.xz * 0.9 + (uTime + along * along * 0.22) * 0.8 * max(uWindSpeed, 0.4));
    float strength = (0.35 + 0.65 * gust) * (0.5 + 0.5 * turbulence) * uWindSpeed;

    // The harder it blows, the less a blade's own habit matters and the more
    // the whole field lies the same way.
    float alignment = clamp(strength * 0.17, 0.0, 0.62);
    float facing = mix(aInstancePosition.w, uWindDirection, alignment);
    float lean = clamp(rest_lean + strength * 0.22, 0.0, 1.15);

    // --- shape --------------------------------------------------------------
    vec3 spine = blade_spine(along, height, lean);
    vec3 tangent = blade_tangent(along, height, lean);
    // The blade is flat across its width, so its side is the plane's X axis
    // and its face is perpendicular to both.
    vec3 side = vec3(1.0, 0.0, 0.0);
    vec3 face = normalize(cross(side, tangent));

    vec3 local = spine + side * (aPosition.x * width);

    // Fanning the normal across the width makes a flat strip read as a
    // rounded leaf and keeps a field of them from flashing all at once.
    mat3 fan = rotate_y(mix(-0.55, 0.55, aTexCoord.x));
    mat3 orientation = rotate_y(facing);

    vec3 world = base + orientation * local;
    vec3 normal = orientation * fan * face;

    vWorldPosition = world;
    vWorldNormal = normal;
    vAlong = vec3(along, aTexCoord.x, variation);

    vec4 clip = uViewProjection * vec4(world, 1.0);

    // --- thickening ---------------------------------------------------------
    // A blade seen edge-on is thinner than a pixel and drops out, leaving
    // holes in the field. Widening it in clip space to a minimum of about one
    // pixel keeps coverage without making face-on blades fat.
    float minimum_width = 1.4 * clip.w / max(uViewportSize.x, 1.0);
    vec4 side_clip = uViewProjection * vec4(orientation * side, 0.0);
    float projected = length(side_clip.xy) * width * 0.5;
    if (projected < minimum_width && projected > 0.0) {
        float extra = (minimum_width - projected) * sign(aPosition.x);
        clip.xy += normalize(side_clip.xy + 1e-6) * extra;
    }

    gl_Position = clip;
}
