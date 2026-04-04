#version 450

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 priorViewProjection;
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int numLights;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
// layout(location = 2) in vec2 inTexCoord;

// layout(location = 1) out vec2 fragTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec4 fragCurrentClip;
layout(location = 4) out vec4 fragPriorClip;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
} push;

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(inPosition, 1.0);
    vec4 priorPositionWorld = push.priorModelMatrix * vec4(inPosition, 1.0);
    fragCurrentClip = ubo.projection * ubo.view * positionWorld;
    fragPriorClip = ubo.priorViewProjection * priorPositionWorld;
    gl_Position = fragCurrentClip;

    mat3 normalMatrix = transpose(inverse(mat3(push.modelMatrix)));
    fragNormalWorld = normalize(normalMatrix * normal);
    fragPosWorld = positionWorld.xyz;
    fragColor = push.baseColor.xyz; // Start with ambient term
}