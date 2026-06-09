#version 450
// water.frag -- PBR-lite water surface shading.
//
// Descriptor layout (fixed by MaterialCache):
//   set=0 binding=0 : GlobalUBO (ubo.time, camera matrices, lights)
//   set=0 binding=1 : prepassDepth -- opaque-only depth from the depth prepass
//                     (separate from scene depth, no water self-contamination)
//   set=1 binding=0 : normalMap1 (Albedo slot)   -> waternormal1.jpg
//   set=1 binding=1 : normalMap2 (Normal slot)   -> waternormal2.jpg
//   set=1 binding=2 : foamMap   (Metallic slot)  -> waterfoam1.jpg
//   set=1 binding=5 : MaterialParams UBO         -> editor-adjustable properties
//                       baseColorFactor.rgb -> shallow water tint
//                       surfaceFactors.z    -> ripple normal scale
//                       specularShininess.w -> specular power

#include "FayeShader.glsl"

FAYE_SAMPLER2D(normalMap1, 1, 0)
FAYE_SAMPLER2D(normalMap2, 1, 1)
FAYE_SAMPLER2D(foamMap,    1, 2)

// Opaque-only depth from the depth prepass (set=0 binding=1).
layout(set = 0, binding = 1) uniform sampler2D prepassDepth;

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
    int   numLights;
    float time;
    float deltaTime;   // seconds since previous frame
    float _pad1;
    float _pad2;
    float _pad3;
    float _pad4;
    float _pad5;
    mat4  inverseProjection;   // exact NDC-depth -> view-space unprojection
} ubo;

// Material parameters -- bound at set=1 binding=5, updated by the editor.
//   baseColorFactor.rgb : shallow water colour tint
//   surfaceFactors.z    : ripple normal intensity  (editor: "Normal Scale")
//   specularShininess.w : Blinn-Phong specular power (editor: "Shininess")
layout(set = 1, binding = 5) uniform MaterialParams {
    vec4 baseColorFactor;
    vec4 surfaceFactors;
    vec4 specularShininess;
    vec4 emissiveFactors;
    vec4 alphaModeCutoff;
} materialParams;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
} push;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec4 fragCurrentClip;
layout(location = 4) in vec4 fragPriorClip;
layout(location = 5) in vec2 fragTexCoord;
layout(location = 6) in vec3 fragTangentWorld;
layout(location = 7) in vec3 fragBitangentWorld;
// Raw Gerstner wave height above the rest plane (world metres), from water.vert.
layout(location = 8) in float fragFoamCrest;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outMotion;

vec3 unpackNormal(vec3 packed)
{
    return normalize(packed * 2.0 - 1.0);
}

// Reoriented Normal Mapping -- preserves both normals without whitening.
vec3 blendNormalsRNM(vec3 n1, vec3 n2)
{
    n1 += vec3(0.0, 0.0, 1.0);
    n2  = vec3(-n2.x, -n2.y, n2.z);
    return normalize(n1 * dot(n1, n2) - n2 * n1.z);
}

// Converts [0,1] NDC depth to positive view-space distance in metres by
// unprojecting through the camera's actual inverse projection matrix.
// Convention-independent: works for any perspective projection because the
// z/w rows of a projection matrix do not depend on NDC x/y.
float linearizeDepth(float d)
{
    vec4 viewPos = ubo.inverseProjection * vec4(0.0, 0.0, d, 1.0);
    return abs(viewPos.z / viewPos.w);
}

void main()
{
    float t = ubo.time;

    // Editor-driven properties
    float normalScale  = clamp(materialParams.surfaceFactors.z, 0.0, 2.0);
    float specPower    = max(materialParams.specularShininess.w, 1.0);
    vec3  shallowTint  = materialParams.baseColorFactor.rgb;

    // ---- Scrolling normal maps -----------------------------------------------
    vec2 uv1 = fragTexCoord * 8.0 + vec2( 0.06,  0.04) * t;
    vec2 uv2 = fragTexCoord * 5.0 + vec2(-0.03,  0.07) * t;

    vec3 n1 = unpackNormal(SAMPLE(normalMap1, uv1).xyz);
    vec3 n2 = unpackNormal(SAMPLE(normalMap2, uv2).xyz);
    n1.xy *= normalScale;
    n2.xy *= normalScale;
    vec3 rippleNormal = blendNormalsRNM(normalize(n1), normalize(n2));

    // ---- World-space surface normal -----------------------------------------
    vec3 surfaceNormal = fragNormalWorld;
    if (dot(fragTangentWorld,   fragTangentWorld)   > 0.001 &&
        dot(fragBitangentWorld, fragBitangentWorld) > 0.001)
    {
        mat3 tbn = mat3(normalize(fragTangentWorld),
                        normalize(fragBitangentWorld),
                        normalize(fragNormalWorld));
        surfaceNormal = normalize(tbn * rippleNormal);
    }

    // ---- Fresnel (Schlick, water F0 ~= 2%) -----------------------------------
    vec3  camPos  = ubo.inverseView[3].xyz;
    vec3  viewDir = normalize(camPos - fragPosWorld);
    float NdotV   = clamp(dot(surfaceNormal, viewDir), 0.0, 1.0);
    float F0      = 0.02;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // ---- Water colour -------------------------------------------------------
    // Shallow tint from editor (base colour), deep tint is fixed.
    vec4 shallowCol = vec4(shallowTint, 0.80);
    vec4 deepCol    = vec4(0.01, 0.10, 0.25, 1.00);
    vec4 waterColor = mix(shallowCol, deepCol, fresnel);

    // ---- Lighting -----------------------------------------------------------
    vec3 diffuseLight  = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);
    for (int i = 0; i < ubo.numLights; i++)
    {
        vec3  toLight     = ubo.pointLights[i].position.xyz - fragPosWorld;
        float attenuation = 1.0 / max(dot(toLight, toLight), 0.0001);
        vec3  dirToLight  = normalize(toLight);
        vec3  lightContrib = ubo.pointLights[i].color.xyz
                           * ubo.pointLights[i].color.w
                           * attenuation;
        diffuseLight  += lightContrib * max(dot(surfaceNormal, dirToLight), 0.0);
        vec3  halfVec   = normalize(dirToLight + viewDir);
        specularLight  += lightContrib
                        * pow(clamp(dot(surfaceNormal, halfVec), 0.0, 1.0), specPower);
    }

    // ---- Foam masks ----------------------------------------------------------
    // fragFoamCrest = raw wave height (metres) above the rest plane.
    float h = fragFoamCrest;

    // Bell-curve mask:
    //   - Pre-crest fringe (h 0.05-0.24): subtle brightening of water surface
    //     to bridge the gap between plain water and visible foam.
    //   - Main foam (h 0.22-0.64): rises with a smooth onset, falls at high
    //     crests so the tip thins slightly (more natural whitecap look).
    float fringe   = smoothstep(0.05, 0.24, h);
    float foamRise = smoothstep(0.22, 0.38, h);
    float foamFall = 1.0 - smoothstep(0.44, 0.68, h);
    float crestFoamMask = foamRise * foamFall;

    // ---- Contact foam (depth prepass intersection) ---------------------------
    // Compare LINEAR view-space distances (metres), not raw NDC depth: NDC
    // depth is non-linear and bunches near 1.0 with distance, which made any
    // geometry behind the water register as "contact". gl_FragCoord-based UVs
    // match the prepass framebuffer exactly (same extent, no Y-flip concerns).
    vec2  screenUV = gl_FragCoord.xy / vec2(textureSize(prepassDepth, 0));
    float opaqueD  = texture(prepassDepth, screenUV).r;

    float contactFoamMask = 0.0;
    if (opaqueD < 1.0)   // 1.0 = clear value = nothing behind this pixel
    {
        float opaqueDist   = linearizeDepth(opaqueD);
        float waterDist    = linearizeDepth(gl_FragCoord.z);
        float gap          = opaqueDist - waterDist;   // metres of water above geometry
        float contactWidth = 0.35;                     // metres
        float contactMask  = 1.0 - clamp(gap / contactWidth, 0.0, 1.0);
        contactFoamMask    = contactMask * contactMask; // squared = tighter ring
    }

    float foamAmount = max(crestFoamMask, contactFoamMask);

    // ---- Foam texture breakup ------------------------------------------------
    // Self-normalising breakup: the fine sample is compared against a coarse
    // sample of the SAME texture (a proxy for its local average), so the
    // result is independent of the texture's absolute brightness. A
    // nearly-white foam texture saturated the previous fixed thresholds and
    // turned every masked fragment into full foam. This is also mip-stable:
    // under heavy minification both samples converge to the mean, the ratio
    // approaches 1, and the breakup fades out instead of exploding.
    vec2  foamUV   = fragTexCoord * 12.0 + vec2(0.02, 0.01) * t;
    vec2  foamUV2  = fragTexCoord *  3.0 - vec2(0.011, 0.017) * t;
    vec4  foamSamp = SAMPLE(foamMap, foamUV);
    float foamFine   = foamSamp.r;
    float foamCoarse = SAMPLE(foamMap, foamUV2).r;

    float ratio   = foamFine / max(foamCoarse, 0.05);
    float breakup = smoothstep(1.0, 1.25, ratio);

    // Coverage is anchored to the geometric mask (foamAmount): the texture
    // only modulates between a soft base and full foam, so foam density is
    // driven by wave height / contact distance -- never by the texture's
    // overall brightness or the viewing distance.
    float foamBlend = foamAmount * mix(0.3, 1.0, breakup);

    // ---- Composite ------------------------------------------------------------
    // 1. Water lit by diffuse light, specular scaled by fresnel reflectance.
    // 2. Fringe zone: slightly lighten the water approaching the crest.
    // 3. Foam zone: lit, texture-tinted foam; foam is rough so it suppresses
    //    the specular highlight.
    vec3 foamAlbedo  = mix(vec3(0.9), foamSamp.rgb * 1.1, 0.5);
    vec3 litWater    = waterColor.rgb * diffuseLight;
    vec3 litFoam     = foamAlbedo * diffuseLight;
    specularLight   *= fresnel;
    vec3 fringeColor = mix(litWater, litFoam, fringe * 0.18);
    vec3 combined    = mix(fringeColor + specularLight, litFoam, foamBlend);
    float alpha      = mix(waterColor.a, 1.0, foamBlend);

    outColor = vec4(combined, alpha);

    // ---- Debug visualisation --------------------------------------------------
    // Set to a non-zero mode and rebuild the shader to inspect a single term.
    // All modes must look STABLE as the camera moves toward/away from the water:
    //   1: contact foam mask (red)   -- fixed-width ring hugging intersections
    //   2: crest foam mask (green)   -- bands on wave crests
    //   3: foam texture breakup      -- greyscale; patchy detail, fades at range
    //   4: linearised water depth    -- 5 m bands, must stay fixed in world space
    //   5: linearised prepass depth  -- 5 m bands on geometry seen through water
#define WATER_DEBUG 0
#if WATER_DEBUG == 1
    outColor = vec4(contactFoamMask, 0.0, 0.0, 1.0);
#elif WATER_DEBUG == 2
    outColor = vec4(0.0, crestFoamMask, 0.0, 1.0);
#elif WATER_DEBUG == 3
    outColor = vec4(vec3(breakup), 1.0);
#elif WATER_DEBUG == 4
    outColor = vec4(vec3(fract(linearizeDepth(gl_FragCoord.z) / 5.0)), 1.0);
#elif WATER_DEBUG == 5
    outColor = (opaqueD < 1.0)
                   ? vec4(vec3(fract(linearizeDepth(opaqueD) / 5.0)), 1.0)
                   : vec4(0.0, 0.0, 1.0, 1.0);   // blue = nothing behind
#endif

    vec2 currentNDC = fragCurrentClip.xy / fragCurrentClip.w;
    vec2 priorNDC   = fragPriorClip.xy   / fragPriorClip.w;
    outMotion       = (currentNDC - priorNDC) * 0.5;
}
