#version 450

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec3 directionToLight;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
// layout(location = 2) in vec2 inTexCoord;

// layout(location = 1) out vec2 fragTexCoord;

layout(location = 0) out vec3 fragColor;

layout(push_constant) uniform Push {
    mat4 modelMatrix; // stores proj * view * model
    mat4 normalMatrix;
} push;

const float AMBIENT = 0.02;


void main() {
    gl_Position = ubo.projectionViewMatrix * push.modelMatrix * vec4(inPosition, 1.0);

    vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);

    float lightIntensity = AMBIENT + max(dot(normalWorldSpace, ubo.directionToLight), 0);

    // fragTexCoord = inTexCoord;
    fragColor = lightIntensity * inColor;
}