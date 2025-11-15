#version 430 core

layout(location = 0) in vec2 aPos;

out vec3 nearPoint;
out vec3 farPoint;

uniform mat4 invView;
uniform mat4 invProj;

vec3 UnprojectPoint(float x, float y, float z) {
    vec4 clipSpace = vec4(x, y, z, 1.0);
    vec4 eyeSpace = invProj * clipSpace;
    eyeSpace /= eyeSpace.w;
    vec4 worldSpace = invView * eyeSpace;
    return worldSpace.xyz;
}

void main() {
    nearPoint = UnprojectPoint(aPos.x, aPos.y, -1.0);
    farPoint = UnprojectPoint(aPos.x, aPos.y, 1.0);
    gl_Position = vec4(aPos, 0.0, 1.0);
}
