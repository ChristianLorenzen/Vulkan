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
    float contrast = max(pushData.params.y, 0.0);
    vec3 adjusted = ((sampledColor.rgb - 0.5) * contrast) + 0.5;
    outColor = vec4(clamp(adjusted, 0.0, 1.0), sampledColor.a);
}