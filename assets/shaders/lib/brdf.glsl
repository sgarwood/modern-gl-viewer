// A compact metallic-roughness BRDF: Trowbridge-Reitz (GGX) normal
// distribution, height-correlated Smith visibility, and a Schlick Fresnel,
// matching the formulation used by glTF and by Filament.

struct Surface {
    vec3 position;      // world space
    vec3 normal;        // world space, unit length
    vec3 view;          // world space, from the surface towards the eye
    vec3 albedo;        // linear, already stripped of any metallic tint
    float roughness;    // perceptual
    float metallic;
    float occlusion;
};

float distribution_ggx(float n_dot_h, float alpha) {
    float a2 = alpha * alpha;
    float d = n_dot_h * n_dot_h * (a2 - 1.0) + 1.0;
    return a2 / max(kPi * d * d, 1e-7);
}

float visibility_smith_ggx(float n_dot_v, float n_dot_l, float alpha) {
    float a2 = alpha * alpha;
    float v = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0 - a2) + a2);
    float l = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0 - a2) + a2);
    return 0.5 / max(v + l, 1e-7);
}

vec3 fresnel_schlick(vec3 f0, float v_dot_h) {
    float f = pow(clamp(1.0 - v_dot_h, 0.0, 1.0), 5.0);
    return f0 + (1.0 - f0) * f;
}

/// Outgoing radiance from one directional light, given its illuminance on a
/// surface facing it. Returns zero for a light below the horizon of the
/// surface rather than a negative lobe.
vec3 direct_lighting(Surface surface, vec3 light_direction, vec3 light_color, float illuminance) {
    float n_dot_l = dot(surface.normal, light_direction);
    if (n_dot_l <= 0.0) {
        return vec3(0.0);
    }
    vec3 half_vector = normalize(light_direction + surface.view);
    float n_dot_v = max(dot(surface.normal, surface.view), 1e-4);
    float n_dot_h = clamp(dot(surface.normal, half_vector), 0.0, 1.0);
    float v_dot_h = clamp(dot(surface.view, half_vector), 0.0, 1.0);

    float alpha = max(surface.roughness * surface.roughness, 1e-3);
    vec3 f0 = mix(vec3(0.04), surface.albedo, surface.metallic);

    vec3 fresnel = fresnel_schlick(f0, v_dot_h);
    float distribution = distribution_ggx(n_dot_h, alpha);
    float visibility = visibility_smith_ggx(n_dot_v, n_dot_l, alpha);
    vec3 specular = fresnel * distribution * visibility;

    vec3 diffuse = (1.0 - fresnel) * (1.0 - surface.metallic) * surface.albedo * kInversePi;

    // Illuminance is already the light's contribution on a perpendicular
    // surface, so the cosine term is all that remains.
    return (diffuse + specular) * light_color * illuminance * n_dot_l;
}

/// A two-lobe hemisphere approximation of sky lighting: the surface sees the
/// zenith colour looking up and the ground bounce looking down.
vec3 hemisphere_ambient(
    Surface surface,
    vec3 zenith_color,
    vec3 horizon_color,
    vec3 ground_color,
    float illuminance) {
    float up = surface.normal.y * 0.5 + 0.5;
    vec3 sky = mix(horizon_color, zenith_color, smoothstep(0.5, 1.0, up));
    vec3 incoming = mix(ground_color, sky, smoothstep(0.0, 0.6, up));
    return surface.albedo * incoming * illuminance * kInversePi * surface.occlusion;
}
