#version 450
// custom_material.frag - Example fragment shader demonstrating the FayeShader
// macro system.  Properties declared with FAYE_PROPERTIES_BEGIN/END are
// reflected by ShaderReflection at load time and can be overridden at runtime
// via MaterialPropertyBlock.
//
// Current limitation: fayeProps is a zero-initialised global struct (GLSL
// struct members have no initialisers).  Until per-material UBO allocation is
// wired up in MaterialCache, actual parameter values come from the standard
// MaterialParams UBO at set=1, binding=5.  The macro declarations here are
// the authoritative source for reflection and future runtime binding.
// TODO: Allocate a per-material GPU buffer for fayeProps once
//       MaterialTemplate/MaterialTemplateRegistry drives pipeline creation.

#include "FayeShader.glsl"

// ---- Declare custom material properties via FayeShader macros --------------
// The C++ ShaderReflection system walks these members from the compiled SPIRV.
FAYE_PROPERTIES_BEGIN
    FAYE_FLOAT(metallic,  0.0)
    FAYE_FLOAT(roughness, 0.5)
    FAYE_VEC4(baseColor,  vec4(1.0))
    FAYE_VEC3(emissive,   vec3(0.0))
FAYE_PROPERTIES_END

// ---- Sampler declarations via FayeShader macros ----------------------------
// Compatible with the descriptor set layout written by MaterialCache.
FAYE_SAMPLER2D(albedoMap,    1, 0)
FAYE_SAMPLER2D(normalMap,    1, 1)
FAYE_SAMPLER2D(metallicMap,  1, 2)
FAYE_SAMPLER2D(roughnessMap, 1, 3)
FAYE_SAMPLER2D(aoMap,        1, 4)

// ---- Standard descriptors --------------------------------------------------
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

// Standard material parameters (provides real GPU-backed values until
// per-material FayeProperties UBO allocation is implemented).
layout(set = 1, binding = 5) uniform MaterialParams {
    vec4 baseColorFactor;
    vec4 surfaceFactors;   // x=metallic, y=roughness, z=normalScale, w=occlusion
    vec4 specularShininess;
    vec4 emissiveFactors;
    vec4 alphaModeCutoff;
} materialParams;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
} push;

// ---- Fragment inputs --------------------------------------------------------
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

void main()
{
    // Sample textures using FayeShader macros.
    vec3 albedo   = SAMPLE(albedoMap,   fragTexCoord).rgb
                  * materialParams.baseColorFactor.rgb
                  * fragColor;
    float metallic  = clamp(SAMPLE(metallicMap,  fragTexCoord).r * materialParams.surfaceFactors.x, 0.0, 1.0);
    float roughness = clamp(SAMPLE(roughnessMap, fragTexCoord).r * materialParams.surfaceFactors.y, 0.04, 1.0);
    float occlusion = clamp(mix(1.0, SAMPLE(aoMap, fragTexCoord).r, materialParams.surfaceFactors.w), 0.0, 1.0);

    // Normal mapping.
    vec3 surfaceNormal = normalize(fragNormalWorld);
    if (length(fragTangentWorld) > 0.001 && length(fragBitangentWorld) > 0.001)
    {
        vec3 tangentNormal = SAMPLE(normalMap, fragTexCoord).xyz * 2.0 - 1.0;
        tangentNormal.xy  *= materialParams.surfaceFactors.z;
        mat3 tbn           = mat3(normalize(fragTangentWorld),
                                  normalize(fragBitangentWorld),
                                  normalize(fragNormalWorld));
        surfaceNormal = normalize(tbn * tangentNormal);
    }

    // Lighting (same Blinn-Phong as built-in PBR shader).
    vec3 cameraPosWorld = ubo.inverseView[3].xyz;
    vec3 viewDir        = normalize(cameraPosWorld - fragPosWorld);
    vec3 specularColor  = mix(materialParams.specularShininess.rgb, albedo, metallic);
    float specularPower = mix(max(materialParams.specularShininess.w, 1.0), 4.0, roughness);

    vec3 diffuseLight  = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);

    for (int i = 0; i < ubo.numLights; i++)
    {
        PointLight light       = ubo.pointLights[i];
        vec3  dirToLight       = light.position.xyz - fragPosWorld;
        float attenuation      = 1.0 / dot(dirToLight, dirToLight);
        dirToLight             = normalize(dirToLight);
        float cosAngIncidence  = max(dot(surfaceNormal, dirToLight), 0.0);
        vec3  intensity        = light.color.xyz * light.color.w * attenuation;

        diffuseLight  += intensity * cosAngIncidence;

        vec3  halfAngle = normalize(dirToLight + viewDir);
        float blinnTerm = pow(clamp(dot(surfaceNormal, halfAngle), 0.0, 1.0), specularPower);
        specularLight  += intensity * blinnTerm * specularColor;
    }

    vec3 diffuseColor = albedo * (1.0 - metallic);
    vec3 emissive     = materialParams.emissiveFactors.rgb * materialParams.emissiveFactors.a;

    outColor  = vec4((diffuseLight * diffuseColor + specularLight) * occlusion + emissive,
                     materialParams.baseColorFactor.a);

    // Motion vector output.
    vec2 currentNDC = fragCurrentClip.xy / fragCurrentClip.w;
    vec2 priorNDC   = fragPriorClip.xy   / fragPriorClip.w;
    outMotion       = (currentNDC - priorNDC) * 0.5;
}
