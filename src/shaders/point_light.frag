#version 450

layout (location =0) in vec2 fragOffset;
layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outMotion;

// This billboard shader reads only push constants; no global UBO needed.

layout(push_constant) uniform Push {
    vec4 position;
    vec4 color;
    float radius;
} push;

void main() {
    float dis = sqrt(dot(fragOffset, fragOffset));
    if (dis >= 1.0) {
        discard;
    }
    outColor = vec4(push.color.xyz, 1.0);
    outMotion = vec2(0.0);
}