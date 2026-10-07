#version 410 core
#include "helper.glsl"
void main() { gl_Position = vec4(included_helper()); }
