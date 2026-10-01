#version 410 core

in vec3 vNormal;
in vec2 vTexCoord;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

out vec4 fragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(normal, lightDirection), 0.12);
    vec4 baseColor = texture(uBaseColorTexture, vTexCoord) * uBaseColorFactor;
    fragColor = vec4(baseColor.rgb * diffuse, baseColor.a);
}
