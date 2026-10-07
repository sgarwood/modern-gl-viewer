// Directional shadow lookup.
//
// The map is bound as a depth texture with hardware comparison enabled, so a
// single texture() call already returns a bilinearly filtered occlusion
// fraction. The rotated Poisson tap pattern on top of that widens the
// penumbra to something closer to a real sun, whose disc subtends about half
// a degree, without the cost of a true blocker search.

uniform mat4 uSunViewProjection;
uniform sampler2DShadow uShadowMap;
uniform float uShadowTexelSize;

const vec2 kPoissonTaps[8] = vec2[8](
    vec2(-0.3277, -0.4056), vec2( 0.7307, -0.2271),
    vec2(-0.6974,  0.3285), vec2( 0.2630,  0.7612),
    vec2( 0.1894, -0.8307), vec2(-0.8755, -0.2518),
    vec2( 0.5718,  0.5072), vec2(-0.1294,  0.1124));

/// Fraction of the sun reaching a point: 1 fully lit, 0 fully occluded.
float sun_visibility(vec3 world_position, vec3 normal, vec3 sun_direction, float softness) {
    // Offset along the normal before projecting. A depth bias alone has to
    // grow with the surface's slope to the light, and at grazing angles that
    // bias is large enough to detach contact shadows; moving along the normal
    // instead costs nothing where the surface faces the light.
    float slope = clamp(1.0 - dot(normal, sun_direction), 0.0, 1.0);
    vec3 offset = world_position + normal * (0.03 + 0.28 * slope);

    vec4 light_clip = uSunViewProjection * vec4(offset, 1.0);
    if (light_clip.w <= 0.0) {
        return 1.0;
    }
    vec3 coordinate = light_clip.xyz / light_clip.w * 0.5 + 0.5;
    if (coordinate.z > 1.0) {
        return 1.0;
    }

    // Rotate the tap pattern per pixel so that undersampling shows up as
    // noise, which the eye tolerates, rather than as banding, which it does
    // not.
    float angle = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
    vec2 rotation = vec2(cos(angle), sin(angle));

    float radius = uShadowTexelSize * softness;
    float visibility = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 tap = vec2(
            kPoissonTaps[i].x * rotation.x - kPoissonTaps[i].y * rotation.y,
            kPoissonTaps[i].x * rotation.y + kPoissonTaps[i].y * rotation.x);
        visibility += texture(uShadowMap, vec3(coordinate.xy + tap * radius, coordinate.z));
    }
    visibility *= 0.125;

    // Fade the whole effect out at the edge of the covered region instead of
    // letting it stop at a hard line.
    vec2 edge = abs(coordinate.xy - 0.5) * 2.0;
    float inside = 1.0 - smoothstep(0.86, 1.0, max(edge.x, edge.y));
    return mix(1.0, visibility, inside);
}
