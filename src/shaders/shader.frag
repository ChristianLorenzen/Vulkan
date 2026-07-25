#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "FayeLighting.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec4 fragCurrentClip;
layout(location = 4) in vec4 fragPriorClip;
layout(location = 5) in vec2 fragTexCoord;
layout(location = 6) in vec3 fragTangentWorld;
layout(location = 7) in vec3 fragBitangentWorld;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outMotion;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
    uvec2 _vertexPad;   // padding for vertexBufferAddress (vertex shader only)
    uint albedoSlot;
    uint normalSlot;
    uint metallicSlot;
    uint roughnessSlot;
    uint aoSlot;
} push;

layout(set = 1, binding = 0) uniform MaterialParams {
    vec4 baseColorFactor;
    vec4 surfaceFactors;
    vec4 specularShininess;
    vec4 emissiveFactors;
    vec4 alphaModeCutoff;
} materialParams;

layout(set = 2, binding = 0) uniform sampler2D allTextures[];

void main() {
    vec3 albedo = texture(allTextures[nonuniformEXT(push.albedoSlot)],   fragTexCoord).rgb
                * materialParams.baseColorFactor.rgb * fragColor;
    float metallic  = clamp(texture(allTextures[nonuniformEXT(push.metallicSlot)],  fragTexCoord).r * materialParams.surfaceFactors.x, 0.0, 1.0);
    float roughness = clamp(texture(allTextures[nonuniformEXT(push.roughnessSlot)], fragTexCoord).r * materialParams.surfaceFactors.y, 0.04, 1.0);
    float occlusion = clamp(mix(1.0, texture(allTextures[nonuniformEXT(push.aoSlot)], fragTexCoord).r, materialParams.surfaceFactors.w), 0.0, 1.0);

    vec3 diffuseLight = fayeAmbient();
    vec3 specularLight = vec3(0.0);
    vec3 surfaceNormal = normalize(fragNormalWorld);

    if (length(fragTangentWorld) > 0.001 && length(fragBitangentWorld) > 0.001) {
        vec3 tangentNormal = texture(allTextures[nonuniformEXT(push.normalSlot)], fragTexCoord).xyz * 2.0 - 1.0;
        tangentNormal.xy *= materialParams.surfaceFactors.z;
        mat3 tbn = mat3(normalize(fragTangentWorld), normalize(fragBitangentWorld), normalize(fragNormalWorld));
        surfaceNormal = normalize(tbn * tangentNormal);
    }

    vec3 cameraPosWorld = fayeCameraPosWorld();
    vec3 viewDir = normalize(cameraPosWorld - fragPosWorld);
    vec3 specularColor = mix(materialParams.specularShininess.rgb, albedo, metallic);
    float specularPower = mix(max(materialParams.specularShininess.w, 1.0), 4.0, roughness);

    for (int i = 0; i < lights.numPointLights; i++) {
        fayeAccumulatePointLight(lights.pointLights[i], fragPosWorld, surfaceNormal, viewDir,
                                 specularColor, specularPower, diffuseLight, specularLight);
    }
    for (int i = 0; i < lights.numDirectionalLights; i++) {
        fayeAccumulateDirectionalLight(lights.directionalLights[i], surfaceNormal, viewDir,
                                       specularColor, specularPower, diffuseLight, specularLight);
    }

    vec3 diffuseColor = albedo * (1.0 - metallic);
    vec3 emissive = materialParams.emissiveFactors.rgb * materialParams.emissiveFactors.a;
    outColor = vec4((diffuseLight * diffuseColor + specularLight) * occlusion + emissive, materialParams.baseColorFactor.a);

    if (materialParams.alphaModeCutoff.x > 0.5 && outColor.a < materialParams.alphaModeCutoff.y) {
        discard;
    }

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