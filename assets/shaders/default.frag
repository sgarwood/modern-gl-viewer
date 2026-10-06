#version 410 core
in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

out vec4 fragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(vec3(0.5, 0.8, 0.3));
    float diffuse = max(dot(normal, lightDirection), 0.15);
    
    // Exact equation of the green
    float green_y = -0.05 * vPosition.z + 0.1 * sin(0.4 * vPosition.z) * cos(0.4 * vPosition.x);
    float h = vPosition.y - green_y;
    
    if (h > 0.02) {
        // It's the golf ball!
        fragColor = vec4(vec3(1.0) * diffuse, 1.0);
        return;
    }
    
    // Base Grass Color (lush green)
    vec3 grassColor1 = vec3(0.18, 0.45, 0.15);
    vec3 grassColor2 = vec3(0.22, 0.52, 0.18);
    
    // Procedural mower pattern
    float pattern = mod(floor(vPosition.x * 1.5) + floor(vPosition.z * 1.5), 2.0);
    vec3 baseGrass = mix(grassColor1, grassColor2, pattern);
    
    // Add contour lines like OS map
    // Evaluate gradient (fwidth) to make constant-thickness lines
    float elevation = vPosition.y;
    float contourSpacing = 0.1; // 10cm contours
    float lineThickness = 1.5; // pixel width
    
    float v = elevation / contourSpacing;
    float dv = fwidth(v);
    float f = abs(fract(v) - 0.5); // distance to mid point
    float d = f / dv; // distance in pixels to contour
    
    vec3 finalColor = baseGrass * diffuse;
    
    // Draw line where d < lineThickness
    if (d < lineThickness) {
        float alpha = 1.0 - (d / lineThickness);
        finalColor = mix(finalColor, vec3(1.0, 1.0, 1.0), alpha * 0.7);
    }
    
    fragColor = vec4(finalColor, 1.0);
}
