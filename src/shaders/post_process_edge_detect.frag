#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sourceColor;
layout(set = 0, binding = 1) uniform sampler2D sceneDepth;

layout(push_constant) uniform PostProcessParameters {
    vec4 color;
    vec4 params;
} pushData;

float sampleDepth(vec2 sampleUv) {
    return texture(sceneDepth, clamp(sampleUv, 0.0, 1.0)).r;
}

void main() {
    vec4 sampledColor = texture(sourceColor, uv);

    vec2 texelSize = 1.0 / vec2(textureSize(sceneDepth, 0));
    float center = sampleDepth(uv);
    float left = sampleDepth(uv + vec2(-texelSize.x, 0.0));
    float right = sampleDepth(uv + vec2(texelSize.x, 0.0));
    float up = sampleDepth(uv + vec2(0.0, -texelSize.y));
    float down = sampleDepth(uv + vec2(0.0, texelSize.y));

    float edgeMagnitude = max(max(abs(center - left), abs(center - right)), max(abs(center - up), abs(center - down)));
    float threshold = max(pushData.params.x, 0.000001);
    float strength = max(pushData.params.y, 0.0);
    float edge = smoothstep(threshold, threshold * max(strength, 1.0), edgeMagnitude * strength);

    vec3 edgeColor = mix(sampledColor.rgb, pushData.color.rgb, edge);
    outColor = vec4(edgeColor, sampledColor.a);
}