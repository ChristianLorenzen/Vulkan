// FayeLighting.glsl - Shared scene-lighting declarations and lighting math.
//
// Include this in any lit FRAGMENT shader. It pulls in FayeGlobal.glsl (camera
// block) and declares the scene lighting UBO at set 0, binding 2, mirroring the
// C++ `SceneLightingUBO` in Renderer/Frame/FrameContext.hpp. The array sizes
// below MUST match MAX_DIRECTIONAL_LIGHTS (4) and MAX_POINT_LIGHTS (10) there.
//
// Vertex shaders should include FayeGlobal.glsl directly instead — the lighting
// block is fragment-stage only.

#ifndef FAYE_LIGHTING_GLSL
#define FAYE_LIGHTING_GLSL

#include "FayeGlobal.glsl"

struct GpuPointLight {
    vec4 position;   // xyz = world position, w unused
    vec4 color;      // rgb = colour, w = intensity
};

struct GpuDirectionalLight {
    vec4 direction;  // xyz = normalised world direction the light travels toward
    vec4 color;      // rgb = colour, w = intensity
};

layout(set = 0, binding = 2) uniform SceneLighting {
    vec4 ambientColor;                        // rgb = colour, a = intensity
    GpuDirectionalLight directionalLights[4]; // MAX_DIRECTIONAL_LIGHTS
    GpuPointLight       pointLights[10];      // MAX_POINT_LIGHTS
    int numDirectionalLights;
    int numPointLights;
    int _pad0;
    int _pad1;
} lights;

// Ambient contribution — use to seed the diffuse accumulator.
vec3 fayeAmbient() { return lights.ambientColor.rgb * lights.ambientColor.a; }

// Blinn-Phong accumulation for one point light. specColor lets callers fold in
// their own specular tint (pass vec3(1.0) for an untinted highlight).
void fayeAccumulatePointLight(
    GpuPointLight light, vec3 fragPos, vec3 N, vec3 V,
    vec3 specColor, float specPower,
    inout vec3 diffuse, inout vec3 specular)
{
    vec3  toLight     = light.position.xyz - fragPos;
    float attenuation = 1.0 / max(dot(toLight, toLight), 1e-4); // inverse square (guarded)
    vec3  L           = normalize(toLight);
    vec3  radiance    = light.color.rgb * light.color.w * attenuation;

    diffuse += radiance * max(dot(N, L), 0.0);

    vec3  H     = normalize(L + V);
    float blinn = pow(clamp(dot(N, H), 0.0, 1.0), specPower);
    specular += radiance * blinn * specColor;
}

// Blinn-Phong accumulation for one directional light. `direction` is the way
// the light travels, so L (toward the light) is its negation; no attenuation.
void fayeAccumulateDirectionalLight(
    GpuDirectionalLight light, vec3 N, vec3 V,
    vec3 specColor, float specPower,
    inout vec3 diffuse, inout vec3 specular)
{
    vec3 L        = normalize(-light.direction.xyz);
    vec3 radiance = light.color.rgb * light.color.w;

    diffuse += radiance * max(dot(N, L), 0.0);

    vec3  H     = normalize(L + V);
    float blinn = pow(clamp(dot(N, H), 0.0, 1.0), specPower);
    specular += radiance * blinn * specColor;
}

#endif // FAYE_LIGHTING_GLSL
