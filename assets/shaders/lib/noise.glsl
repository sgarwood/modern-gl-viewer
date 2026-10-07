// Cheap hash-based value noise. Deterministic across drivers because it only
// uses integer bit operations on the float bit pattern rather than the
// sin(dot(...)) trick, whose precision varies between implementations.

float hash12(vec2 p) {
    uvec2 q = floatBitsToUint(p);
    q = 1103515245u * ((q >> 1u) ^ (q.yx));
    uint n = 1103515245u * ((q.x) ^ (q.y >> 3u));
    n = n ^ (n >> 16u);
    return float(n) * (1.0 / float(0xffffffffu));
}

vec2 hash22(vec2 p) {
    return vec2(hash12(p), hash12(p + vec2(37.1, 17.7)));
}

float value_noise(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    // Quintic interpolant: continuous first and second derivatives, so lighting
    // derived from the noise has no visible cell seams.
    vec2 w = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float a = hash12(cell);
    float b = hash12(cell + vec2(1.0, 0.0));
    float c = hash12(cell + vec2(0.0, 1.0));
    float d = hash12(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, w.x), mix(c, d, w.x), w.y);
}

float fbm(vec2 p, int octaves) {
    float sum = 0.0;
    float amplitude = 0.5;
    float normalization = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amplitude * value_noise(p);
        normalization += amplitude;
        p *= 2.03;
        amplitude *= 0.5;
    }
    return sum / max(normalization, 1e-5);
}
