#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main() {
    vec3 color = vec3(0.6, 0.7, 0.8);
    
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    float specularStrength = 0.1;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * color;
    
    // Distance fog
    float distance = length(viewPos - FragPos);
    float fogStart = 500.0;
    float fogEnd = 3000.0;
    float fogDensity = 0.0003;
    
    // Exponential fog
    float fogFactor = exp(-fogDensity * distance);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // Fog color (sky blue with slight haze)
    vec3 fogColor = vec3(0.5, 0.7, 1.0);
    
    // Mix scene color with fog
    result = mix(fogColor, result, fogFactor);
    
    FragColor = vec4(result, 1.0);
}
