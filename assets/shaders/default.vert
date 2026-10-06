#version 410 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
uniform mat4 uMvp;
out vec3 vPosition;
out vec3 vNormal;
out vec2 vTexCoord;
void main() {
    vPosition = aPosition;
    vNormal = aNormal;
    vTexCoord = aTexCoord;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
