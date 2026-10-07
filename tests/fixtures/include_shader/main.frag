#version 410 core
#include "helper.glsl"
#include "outer.glsl"
out vec4 fragColor;
void main() { fragColor = vec4(included_helper() * kNested); }
