// Preetham's analytic daylight model.
//
// A. J. Preetham, P. Shirley, B. Smits, "A Practical Analytic Model for
// Daylight", SIGGRAPH 1999. The distribution coefficients and the zenith
// polynomials below are the ones tabulated in that paper.
//
// Chosen over a raymarched or LUT-based model because it is a single closed
// form evaluation per pixel with no precomputation, which keeps a full-screen
// sky affordable even on a software rasteriser, and because an OpenGL 4.1 core
// profile has no compute shaders to build scattering LUTs with.

#include "color.glsl"

struct SkyCoefficients {
    vec3 a;
    vec3 b;
    vec3 c;
    vec3 d;
    vec3 e;
    vec3 zenith;  // xyY: chromaticity and luminance at the zenith
};

/// Builds the Perez distribution coefficients and zenith colour for a given
/// turbidity and sun elevation. Each component is x, y, Y in turn.
SkyCoefficients sky_coefficients(float turbidity, float sun_elevation_cosine) {
    float t = clamp(turbidity, 1.7, 10.0);
    // The solar zenith angle, which the zenith polynomials are expressed in.
    float theta_sun = acos(clamp(sun_elevation_cosine, 0.0, 1.0));
    float theta2 = theta_sun * theta_sun;
    float theta3 = theta2 * theta_sun;
    float t2 = t * t;

    SkyCoefficients sky;
    sky.a = vec3(-0.0193 * t - 0.2592, -0.0167 * t - 0.2608,  0.1787 * t - 1.4630);
    sky.b = vec3(-0.0665 * t + 0.0008, -0.0950 * t + 0.0092, -0.3554 * t + 0.4275);
    sky.c = vec3(-0.0004 * t + 0.2125, -0.0079 * t + 0.2102, -0.0227 * t + 5.3251);
    sky.d = vec3(-0.0641 * t - 0.8989, -0.0441 * t - 1.6537,  0.1206 * t - 2.5771);
    sky.e = vec3(-0.0033 * t + 0.0452, -0.0109 * t + 0.0529, -0.0670 * t + 0.3703);

    float chi = (4.0 / 9.0 - t / 120.0) * (kPi - 2.0 * theta_sun);
    float zenith_luminance = (4.0453 * t - 4.9710) * tan(chi) - 0.2155 * t + 2.4192;

    float zenith_x =
        ( 0.00165 * theta3 - 0.00375 * theta2 + 0.00209 * theta_sun + 0.0)       * t2 +
        (-0.02903 * theta3 + 0.06377 * theta2 - 0.03202 * theta_sun + 0.00394)   * t +
        ( 0.11693 * theta3 - 0.21196 * theta2 + 0.06052 * theta_sun + 0.25886);
    float zenith_y =
        ( 0.00275 * theta3 - 0.00610 * theta2 + 0.00317 * theta_sun + 0.0)       * t2 +
        (-0.04214 * theta3 + 0.08970 * theta2 - 0.04153 * theta_sun + 0.00516)   * t +
        ( 0.15346 * theta3 - 0.26756 * theta2 + 0.06670 * theta_sun + 0.26688);

    // The paper gives zenith luminance in kcd/m^2; the renderer works in cd/m^2.
    sky.zenith = vec3(zenith_x, zenith_y, max(zenith_luminance, 0.0) * 1000.0);
    return sky;
}

vec3 perez(SkyCoefficients sky, float cos_theta, float gamma, float cos_gamma) {
    // The 0.01 guards the grazing case where cos_theta reaches zero.
    return (1.0 + sky.a * exp(sky.b / (cos_theta + 0.01))) *
           (1.0 + sky.c * exp(sky.d * gamma) + sky.e * cos_gamma * cos_gamma);
}

/// Sky radiance, in cd/m^2, looking along `direction`.
///
/// The model is only defined above the horizon. Below it the ray is folded
/// back and shaded as ground lit by the same sky, so a camera tilted down at
/// the ball sees distant terrain haze rather than a hard black band.
vec3 sky_radiance(vec3 direction, vec3 sun_direction, float turbidity, vec3 ground_albedo) {
    SkyCoefficients sky = sky_coefficients(turbidity, sun_direction.y);

    float below = smoothstep(0.0, 0.035, -direction.y);
    vec3 sampled = normalize(vec3(direction.x, abs(direction.y) + 0.001, direction.z));

    float cos_theta = clamp(sampled.y, 0.0, 1.0);
    float cos_gamma = clamp(dot(sampled, sun_direction), -1.0, 1.0);
    float gamma = acos(cos_gamma);

    // The distribution is relative: dividing by its value at the zenith, where
    // theta is zero and gamma equals the solar zenith angle, normalizes it so
    // the zenith colour comes out exactly as tabulated.
    float cos_theta_sun = clamp(sun_direction.y, 0.0, 1.0);
    vec3 relative = perez(sky, cos_theta, gamma, cos_gamma) /
                    max(perez(sky, 1.0, acos(cos_theta_sun), cos_theta_sun), vec3(1e-4));

    vec3 radiance = xyY_to_linear_srgb(sky.zenith * relative);

    // The ground beyond the modelled terrain: its own diffuse response to the
    // sky, veiled by the full depth of atmosphere between here and there. That
    // veil is most of what is actually seen, which is why a distant hillside
    // reads as a pale wash rather than as grass.
    vec3 horizon = xyY_to_linear_srgb(
        sky.zenith * perez(sky, 0.02, gamma, cos_gamma) /
        max(perez(sky, 1.0, acos(cos_theta_sun), cos_theta_sun), vec3(1e-4)));
    vec3 ground = mix(ground_albedo * luminance(horizon) * 1.6, horizon, 0.72);
    return mix(radiance, ground, below);
}

/// The sun's own disc. Its angular radius is about 0.266 degrees; the limb is
/// softened by a pixel's worth of angle so the edge does not alias.
vec3 sun_disc(vec3 direction, vec3 sun_direction, vec3 sun_color, float sun_illuminance) {
    const float kAngularRadius = 0.00465;
    float cos_angle = dot(direction, sun_direction);
    float edge = fwidth(cos_angle) + 1e-6;
    float disc = smoothstep(cos(kAngularRadius) - edge, cos(kAngularRadius) + edge, cos_angle);

    // Radiance of the disc from its illuminance and solid angle, so changing
    // the sun's strength moves the disc and the lit ground together.
    float solid_angle = 2.0 * kPi * (1.0 - cos(kAngularRadius));
    return sun_color * (sun_illuminance / max(solid_angle, 1e-6)) * disc;
}
