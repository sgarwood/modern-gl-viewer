// Colour space helpers shared by the sky and surface shaders.

vec3 xyz_to_linear_srgb(vec3 xyz) {
    return vec3(
        dot(vec3( 3.2404542, -1.5371385, -0.4985314), xyz),
        dot(vec3(-0.9692660,  1.8760108,  0.0415560), xyz),
        dot(vec3( 0.0556434, -0.2040259,  1.0572252), xyz));
}

/// Converts a CIE xyY colour, as the Preetham model produces, to linear sRGB.
/// Luminance is carried through unchanged, so the result stays in cd/m^2.
vec3 xyY_to_linear_srgb(vec3 xyY) {
    float y = max(xyY.y, 1e-4);
    vec3 xyz = vec3(xyY.x, xyY.y, 1.0 - xyY.x - xyY.y) * (xyY.z / y);
    return max(xyz_to_linear_srgb(xyz), vec3(0.0));
}

float luminance(vec3 linear) {
    return dot(linear, vec3(0.2126, 0.7152, 0.0722));
}
