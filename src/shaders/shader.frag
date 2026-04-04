#version 450

// layout(binding = 1) uniform sampler2D texSampler;

// layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec4 fragCurrentClip;
layout(location = 4) in vec4 fragPriorClip;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outMotion;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
} push;

// This is not available to the fragment shader as when we 
// initialized the ubo, we only made it available to the vertex stage.
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

void main() {
    vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);
    vec3 surfaceNormal = normalize(fragNormalWorld);

    vec3 cameraPosWorld = ubo.inverseView[3].xyz; // Extract camera position from inverse view matrix
    vec3 viewDir = normalize(cameraPosWorld - fragPosWorld);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];
        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight); // Inverse square law
        
        directionToLight = normalize(directionToLight);
    
        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0.0);
        vec3 intensity = light.color.xyz * light.color.w * attenuation;

        diffuseLight += intensity * cosAngIncidence;

        vec3 halfAngle = normalize(directionToLight + viewDir);
        float blinnTerm = dot(surfaceNormal, halfAngle);
        blinnTerm = clamp(blinnTerm, 0.0, 1.0);
        blinnTerm = pow(blinnTerm, 64.0); // Broader default highlight for simple colored materials
        specularLight += intensity * blinnTerm;
    }

    outColor = vec4(diffuseLight * fragColor + specularLight, 1.0);

    if (fragCurrentClip.w <= 0.0 || fragPriorClip.w <= 0.0) {
        outMotion = vec2(0.0);
        return;
    }

    vec2 currentNdc = fragCurrentClip.xy / fragCurrentClip.w;
    vec2 priorNdc = fragPriorClip.xy / fragPriorClip.w;
    vec2 currentUv = currentNdc * 0.5 + 0.5;
    vec2 priorUv = priorNdc * 0.5 + 0.5;
    outMotion = priorUv - currentUv;
}