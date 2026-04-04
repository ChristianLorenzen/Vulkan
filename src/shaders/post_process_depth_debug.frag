#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneDepth;

void main() {
    float depthSample = texture(sceneDepth, uv).r;
    float depthVisual = 1.0 - clamp(depthSample, 0.0, 1.0);
    outColor = vec4(vec3(depthVisual), 1.0);
}