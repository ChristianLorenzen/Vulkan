#version 450

// layout(binding = 1) uniform sampler2D texSampler;

// layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 transform; // stores proj * view * model
    mat4 normalMatrix;
} push;

// This is not available to the fragment shader as when we 
// initialized the ubo, we only made it available to the vertex stage.
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec3 directionToLight;
} ubo;

void main() {
    //outColor = texture(texSampler, fragTexCoord);
    outColor = vec4(fragColor, 1.0);
}