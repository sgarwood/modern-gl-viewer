#version 410 core

in vec3 vNormal;
in vec2 vTexCoord;

uniform sampler2D uBaseColorTexture;

out vec4 fragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(normal, lightDirection), 0.12);
    vec3 baseColor = texture(uBaseColorTexture, vTexCoord).rgb;
    fragColor = vec4(baseColor * diffuse, 1.0);
}
