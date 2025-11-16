#version 430 core

in vec3 nearPoint;
in vec3 farPoint;

out vec4 FragColor;

uniform sampler3D volumeTexture;     // E-field (Ez component)
uniform sampler3D epsilonTexture;    // Material properties
uniform sampler3D emissionTexture;   // Emission sources

uniform vec3 gridCenter;             // World-space grid center
uniform vec3 gridHalfSize;           // Half size of grid in world units (per-axis, anisotropic)
uniform int gridSize;                // Grid resolution (e.g., 64, 128)
uniform float intensityScale;        // Visualization intensity multiplier
uniform int stepCount;               // Ray-marching steps
uniform bool showEmissionSource;     // Show emission markers
uniform bool showGeometryEdges;      // Show geometry wireframe

// Gradient colors for waveform
uniform vec3 gradientColorLow;       // Color for low intensity (default: dark blue)
uniform vec3 gradientColorHigh;      // Color for high intensity (default: red)

vec3 valueToColor(float value) {
    // Map field strength to smooth gradient from low to high color
    float intensity = abs(value);
    
    // Smooth gradient interpolation
    return mix(gradientColorLow, gradientColorHigh, intensity);
}

bool isEdge(vec3 texCoord) {
    if (!showGeometryEdges) return false;
    
    float eps = texture(epsilonTexture, texCoord).r;
    if (eps <= 1.01) return false; // Not in material
    
    // Check neighbors for edge detection
    float step = 1.0 / float(gridSize);
    float neighbors = 0.0;
    neighbors += texture(epsilonTexture, texCoord + vec3(step, 0, 0)).r;
    neighbors += texture(epsilonTexture, texCoord - vec3(step, 0, 0)).r;
    neighbors += texture(epsilonTexture, texCoord + vec3(0, step, 0)).r;
    neighbors += texture(epsilonTexture, texCoord - vec3(0, step, 0)).r;
    neighbors += texture(epsilonTexture, texCoord + vec3(0, 0, step)).r;
    neighbors += texture(epsilonTexture, texCoord - vec3(0, 0, step)).r;
    
    // If any neighbor is air (epsilon ~= 1), this is an edge
    return neighbors < 6.0 * eps * 0.95;
}

bool intersectBox(vec3 orig, vec3 dir, out float t0, out float t1) {
    vec3 boxMin = gridCenter - gridHalfSize; // Component-wise subtract for anisotropic
    vec3 boxMax = gridCenter + gridHalfSize; // Component-wise add for anisotropic
    
    vec3 invDir = 1.0 / dir;
    vec3 tMin = (boxMin - orig) * invDir;
    vec3 tMax = (boxMax - orig) * invDir;
    
    vec3 t1v = min(tMin, tMax);
    vec3 t2v = max(tMin, tMax);
    
    t0 = max(max(t1v.x, t1v.y), t1v.z);
    t1 = min(min(t2v.x, t2v.y), t2v.z);
    
    return t1 > max(t0, 0.0);
}

void main() {
    vec3 rayDir = normalize(farPoint - nearPoint);
    vec3 rayOrigin = nearPoint;
    
    float t0, t1;
    if (!intersectBox(rayOrigin, rayDir, t0, t1)) {
        discard;
    }
    
    t0 = max(t0, 0.0);
    
    // Ray marching
    int numSteps = stepCount;
    float stepSize = (t1 - t0) / float(numSteps);
    
    vec4 color = vec4(0.0);
    
    for (int i = 0; i < numSteps; i++) {
        float t = t0 + float(i) * stepSize;
        vec3 pos = rayOrigin + rayDir * t;
        
        // Convert world space to texture coordinates [0,1]
        vec3 localPos = pos - gridCenter;
        vec3 texCoord = (localPos / gridHalfSize) * 0.5 + 0.5;
        
        if (texCoord.x < 0.0 || texCoord.x > 1.0 ||
            texCoord.y < 0.0 || texCoord.y > 1.0 ||
            texCoord.z < 0.0 || texCoord.z > 1.0) {
            continue;
        }
        
        float value = texture(volumeTexture, texCoord).r;
        float intensity = abs(value) * intensityScale;
        
        vec3 rgb = valueToColor(value * intensityScale);
        vec4 sampleColor = vec4(rgb, intensity);
        
        // Check if this is an emission source location
        if (showEmissionSource) {
            float emissionVal = abs(texture(emissionTexture, texCoord).r);
            if (emissionVal > 0.01) {
                // Bright yellow marker for emission source
                sampleColor = vec4(1.0, 1.0, 0.0, 0.8);
            }
        }
        
        // Check for material edge and add wireframe
        if (isEdge(texCoord)) {
            sampleColor = vec4(0.5, 0.5, 0.5, 0.5); // Grey wireframe
        }
        
        // Front-to-back compositing
        sampleColor.a *= 0.5; // Alpha for better visibility
        color.rgb += sampleColor.rgb * sampleColor.a * (1.0 - color.a);
        color.a += sampleColor.a * (1.0 - color.a);
        
        if (color.a > 0.95) {
            break;
        }
    }
    
    FragColor = color;
}
