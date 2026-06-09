#version 450
// depth_prepass.vert -- Minimal vertex shader for the opaque depth prepass.
// Outputs only gl_Position; no fragment outputs needed.
// Uses the same push-constant layout as shader.vert so the same pipeline
// layout can be reused.

struct PointLight { vec4 position; vec4 color; };

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 priorViewProjection;
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int   numLights;
    float time;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent;

void main()
{
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(inPosition, 1.0);
}
