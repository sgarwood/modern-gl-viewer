// Height-falloff aerial perspective.
//
// The optical depth along a ray through an exponentially thinning medium has a
// closed form, so distant geometry picks up the sky's own colour without any
// marching. This is what reads as atmosphere and sells the scale of a hole.

/// Fraction of the surface's own radiance that survives the path to the eye.
float fog_transmittance(vec3 surface_position, vec3 eye_position, float density, float falloff) {
    vec3 ray = surface_position - eye_position;
    float distance_along = length(ray);
    if (distance_along < 1e-4 || density <= 0.0) {
        return 1.0;
    }

    float rise = ray.y;
    float base = exp(-falloff * eye_position.y);
    // The integral of density * exp(-falloff * height) along the ray. The
    // vertical term degenerates when the ray is level, so it is expanded to
    // its limit there instead of dividing by zero.
    float vertical = abs(rise) > 1e-3
        ? (1.0 - exp(-falloff * rise)) / (falloff * rise)
        : 1.0;
    float optical_depth = density * distance_along * base * vertical;
    return exp(-max(optical_depth, 0.0));
}

/// Blends a surface towards the radiance of the sky behind it.
vec3 apply_fog(vec3 surface_radiance, vec3 in_scattered_radiance, float transmittance) {
    return mix(in_scattered_radiance, surface_radiance, transmittance);
}
