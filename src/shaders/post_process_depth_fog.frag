#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sourceColor;
layout(set = 0, binding = 1) uniform sampler2D sceneDepth;

layout(push_constant) uniform PostProcessParameters {
    vec4 color;
    vec4 params;
} pushData;

float linearizeDepth(float depthSample, float nearPlane, float farPlane) {
    return (nearPlane * farPlane) / (farPlane + depthSample * (nearPlane - farPlane));
}

void main() {
    vec4 sampledColor = texture(sourceColor, uv);
    float nearPlane = max(pushData.params.x, 0.001);
    float farPlane = max(pushData.params.y, nearPlane + 0.001);
    float depthSample = texture(sceneDepth, uv).r;
    float linearDepth = linearizeDepth(depthSample, nearPlane, farPlane);
    float fogFactor = clamp((linearDepth - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);
    vec3 foggedColor = mix(sampledColor.rgb, pushData.color.rgb, fogFactor);
    outColor = vec4(foggedColor, sampledColor.a);
}