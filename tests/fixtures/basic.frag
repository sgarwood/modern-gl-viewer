#version 410 core
uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;
out vec4 fragColor;
void main() { fragColor = texture(uBaseColorTexture, vec2(0.0)) * uBaseColorFactor; }
