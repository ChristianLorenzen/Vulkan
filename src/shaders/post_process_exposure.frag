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
    float multiplier = exp2(pushData.params.x);
    outColor = vec4(sampledColor.rgb * multiplier, sampledColor.a);
}