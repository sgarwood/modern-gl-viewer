// The standard uniform block every mgv shader may draw from.
//
// The backend caches the location of each of these at link time and uploads
// only the ones a program actually declares, so a shader is free to include
// this module and use a single value from it.

uniform mat4 uModel;
uniform mat4 uNormalMatrix;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uViewProjection;
uniform vec3 uCameraPosition;
uniform float uTime;

uniform vec3 uSunDirection;      // Points from the surface towards the sun.
uniform vec3 uSunColor;
uniform float uSunIlluminance;   // lux
uniform vec3 uSkyZenithColor;
uniform vec3 uSkyHorizonColor;
uniform vec3 uGroundAlbedo;
uniform float uSkyIlluminance;   // lux
uniform float uTurbidity;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uWindSpeed;        // metres per second
uniform float uWindDirection;    // radians, clockwise from -Z
uniform float uSurfaceWetness;   // 0 dry, 1 saturated
uniform vec2 uViewportSize;

const float kPi = 3.14159265359;
const float kInversePi = 0.31830988618;
