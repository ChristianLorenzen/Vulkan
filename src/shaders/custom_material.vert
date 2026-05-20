#version 450
// custom_material.vert - Example vertex shader using the Faye custom-material pipeline.
// Matches the push-constant and global UBO layout of the built-in PBR shader so it
// is compatible with the existing SimpleRenderSystem pipeline layout.

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
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec4 fragCurrentClip;
layout(location = 4) out vec4 fragPriorClip;
layout(location = 5) out vec2 fragTexCoord;
layout(location = 6) out vec3 fragTangentWorld;
layout(location = 7) out vec3 fragBitangentWorld;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
} push;

void main()
{
    vec4 positionWorld      = push.modelMatrix      * vec4(inPosition, 1.0);
    vec4 priorPositionWorld = push.priorModelMatrix * vec4(inPosition, 1.0);

    fragCurrentClip = ubo.projection * ubo.view * positionWorld;
    fragPriorClip   = ubo.priorViewProjection * priorPositionWorld;
    gl_Position     = fragCurrentClip;

    mat3 normalMatrix     = transpose(inverse(mat3(push.modelMatrix)));
    fragNormalWorld       = normalize(normalMatrix * normal);
    vec3 tangentWorld     = normalize(mat3(push.modelMatrix) * inTangent.xyz);
    fragTangentWorld      = tangentWorld;
    fragBitangentWorld    = normalize(cross(fragNormalWorld, tangentWorld) * inTangent.w);
    fragPosWorld          = positionWorld.xyz;
    fragTexCoord          = uv;
    fragColor             = push.baseColor.xyz;
}
