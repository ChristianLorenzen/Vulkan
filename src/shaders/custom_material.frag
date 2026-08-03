#version 450
#extension GL_GOOGLE_include_directive:require
// custom_material.frag - Example fragment shader demonstrating the FayeShader
// macro system.  Properties declared with FAYE_PROPERTIES_BEGIN/END are
// reflected by ShaderReflection at load time and can be overridden at runtime
// via MaterialPropertyBlock.
#extension GL_EXT_nonuniform_qualifier:require

#include "FayeShader.glsl"
#include "FayeLighting.glsl"

// ---- Declare custom material properties via FayeShader macros --------------
// The C++ ShaderReflection system walks these members from the compiled SPIRV.
FAYE_PROPERTIES_BEGIN
FAYE_FLOAT(metallic,0.)
FAYE_FLOAT(roughness,.5)
FAYE_VEC4(baseColor,vec4(1.))
FAYE_VEC3(emissive,vec3(0.))
FAYE_PROPERTIES_END

// ---- Standard descriptors --------------------------------------------------
// GlobalUbo (set 0, b0) and SceneLighting (set 0, b2) come from FayeLighting.glsl.

// Material parameter UBO — now at binding 0 (textures moved to bindless set 2).
layout(set=1,binding=0)uniform MaterialParams{
    vec4 baseColorFactor;
    vec4 surfaceFactors;// x=metallic, y=roughness, z=normalScale, w=occlusion
    vec4 specularShininess;
    vec4 emissiveFactors;
    vec4 alphaModeCutoff;
}materialParams;

// Bindless texture array (set 2). Indexed by slot indices in push constants.
layout(set=2,binding=0)uniform sampler2D allTextures[];

layout(push_constant)uniform Push{
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
    uvec2 _vertexPad;// padding for vertexBufferAddress (vertex shader only)
    uint albedoSlot;
    uint normalSlot;
    uint metallicSlot;
    uint roughnessSlot;
    uint aoSlot;
}push;

// ---- Fragment inputs --------------------------------------------------------
layout(location=0)in vec3 fragColor;
layout(location=1)in vec3 fragPosWorld;
layout(location=2)in vec3 fragNormalWorld;
layout(location=3)in vec4 fragCurrentClip;
layout(location=4)in vec4 fragPriorClip;
layout(location=5)in vec2 fragTexCoord;
layout(location=6)in vec3 fragTangentWorld;
layout(location=7)in vec3 fragBitangentWorld;

layout(location=0)out vec4 outColor;
layout(location=1)out vec2 outMotion;

void main()
{
    // Sample textures via bindless array (indexed by push constant slots).
    vec3 albedo=texture(allTextures[nonuniformEXT(push.albedoSlot)],fragTexCoord).rgb
    *materialParams.baseColorFactor.rgb
    *fragColor;
    float metallic=clamp(texture(allTextures[nonuniformEXT(push.metallicSlot)],fragTexCoord).r*materialParams.surfaceFactors.x,0.,1.);
    float roughness=clamp(texture(allTextures[nonuniformEXT(push.roughnessSlot)],fragTexCoord).r*materialParams.surfaceFactors.y,.04,1.);
    float occlusion=clamp(mix(1.,texture(allTextures[nonuniformEXT(push.aoSlot)],fragTexCoord).r,materialParams.surfaceFactors.w),0.,1.);
    
    // Normal mapping.
    vec3 surfaceNormal=normalize(fragNormalWorld);
    if(length(fragTangentWorld)>.001&&length(fragBitangentWorld)>.001)
    {
        vec3 tangentNormal=texture(allTextures[nonuniformEXT(push.normalSlot)],fragTexCoord).xyz*2.-1.;
        tangentNormal.xy*=materialParams.surfaceFactors.z;
        mat3 tbn=mat3(normalize(fragTangentWorld),
        normalize(fragBitangentWorld),
        normalize(fragNormalWorld));
        surfaceNormal=normalize(tbn*tangentNormal);
    }
    
    // Lighting (same Blinn-Phong as built-in PBR shader, via FayeLighting.glsl).
    vec3 cameraPosWorld=fayeCameraPosWorld();
    vec3 viewDir=normalize(cameraPosWorld-fragPosWorld);
    
    vec3 diffuseLight=vec3(0.);
    vec3 specularLight=vec3(0.);
    
    for(int i=0;i<lights.numPointLights;i++)
    {
        fayeAccumulatePointLight(lights.pointLights[i],fragPosWorld,surfaceNormal,viewDir,
        albedo,metallic,roughness,diffuseLight,specularLight);
    }
    for(int i=0;i<lights.numDirectionalLights;i++)
    {
        fayeAccumulateDirectionalLight(lights.directionalLights[i],surfaceNormal,viewDir,
        albedo,metallic,roughness,diffuseLight,specularLight);
    }
    
    //vec3 ambient=fayeAmbient()*albedo*(1.-metallic)*occlusion;
    vec3 ambient=fayeIBL(surfaceNormal,viewDir,albedo,metallic,roughness,occlusion);
    vec3 emissive=materialParams.emissiveFactors.rgb*materialParams.emissiveFactors.a;
    
    outColor=vec4(ambient+diffuseLight+specularLight+emissive,
    materialParams.baseColorFactor.a);
    
    // Motion vector output.
    vec2 currentNDC=fragCurrentClip.xy/fragCurrentClip.w;
    vec2 priorNDC=fragPriorClip.xy/fragPriorClip.w;
    outMotion=(currentNDC-priorNDC)*.5;
}
