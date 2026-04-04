#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sourceColor;

layout(push_constant) uniform PostProcessParameters {
    vec4 color;
    vec4 params;
} pushData;

void main() {
    vec4 sampledColor = texture(sourceColor, uv);
    float intensity = clamp(pushData.params.z, 0.0, 1.0);
    float softness = clamp(pushData.params.w, 0.05, 1.0);
    vec2 centeredUv = uv * 2.0 - 1.0;
    float distanceFromCenter = length(centeredUv);
    float vignette = 1.0 - smoothstep(max(0.0, 1.0 - softness), 1.0, distanceFromCenter) * intensity;
    outColor = vec4(sampledColor.rgb * vignette, sampledColor.a);
}