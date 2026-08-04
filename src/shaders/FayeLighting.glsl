// FayeLighting.glsl - Shared scene-lighting declarations and lighting math.
//
// Include this in any lit FRAGMENT shader. It pulls in FayeGlobal.glsl (camera
// block) and declares the scene lighting UBO at set 0, binding 2, mirroring the
// C++ `SceneLightingUBO` in Renderer/Frame/FrameContext.hpp. The array sizes
// below MUST match MAX_DIRECTIONAL_LIGHTS (4) and MAX_POINT_LIGHTS (10) there.
//
// Vertex shaders should include FayeGlobal.glsl directly instead — the lighting
// block is fragment-stage only.
#extension GL_GOOGLE_include_directive:require
#ifndef FAYE_LIGHTING_GLSL
#define FAYE_LIGHTING_GLSL

#include "FayeGlobal.glsl"

struct GpuPointLight{
    vec4 position;// xyz = world position, w unused
    vec4 color;// rgb = colour, w = intensity
};

struct GpuDirectionalLight{
    vec4 direction;// xyz = normalised world direction the light travels toward
    vec4 color;// rgb = colour, w = intensity
};

layout(set=0,binding=2)uniform SceneLighting{
    vec4 ambientColor;// rgb = colour, a = intensity
    GpuDirectionalLight directionalLights[4];// MAX_DIRECTIONAL_LIGHTS
    GpuPointLight pointLights[10];// MAX_POINT_LIGHTS
    int numDirectionalLights;
    int numPointLights;
    int _pad0;
    int _pad1;
    vec4 skyParams;// x = skybox rotation, y = skybox intensity, z = prefiltered mip count, w = unused
}lights;

layout(set=0,binding=4)uniform samplerCube irradianceCube;
layout(set=0,binding=5)uniform samplerCube prefilteredCube;

// Ambient contribution — use to seed the diffuse accumulator.
vec3 fayeAmbient(){return lights.ambientColor.rgb*lights.ambientColor.a;}

vec3 fayeRotateY(vec3 v,float a){
    float s=sin(a),c=cos(a);
    return vec3(c*v.x+s*v.z,v.y,-s*v.x+c*v.z);
}

vec3 fayeFresnelSchlickRoughness(float cosTheta,vec3 F0,float roughness)
{
    return F0+(max(vec3(1.-roughness),F0)-F0)*pow(1.-cosTheta,5.);
}

vec2 fayeEnvBRDFApprox(float NdotV,float roughness)
{
    const vec4 c0=vec4(-1.,-.0275,-.572,.022);
    const vec4 c1=vec4(1.,.0425,1.04,-.04);
    vec4 r=roughness*c0+c1;
    float a004=min(r.x*r.x,exp2(-9.28*NdotV))*r.x+r.y;
    return vec2(-1.04,1.04)*a004+r.zw;
}

vec3 fayeIBL(vec3 N,vec3 V,vec3 albedo,float metallic,float roughness,
float occlusion)
{
    float NoV=max(dot(N,V),1e-4);
    vec3 R=reflect(-V,N);
    vec3 F0=mix(vec3(.04),albedo,metallic);
    
    vec3 F=fayeFresnelSchlickRoughness(NoV,F0,roughness);
    vec3 kD=(1.-F)*(1.-metallic);
    
    vec3 nr=fayeRotateY(N,lights.skyParams.x);
    vec3 rr=fayeRotateY(R,lights.skyParams.x);
    
    // Irradiance is already cosine-convolved AND divided by PI in the bake, so
    // no further PI division here -- doing it twice is the classic "IBL is 3x
    // too dark" bug.
    vec3 diffuse=texture(irradianceCube,nr).rgb*albedo;
    
    float lod=roughness*max(lights.skyParams.z-1.,0.);
    vec3 prefiltered=textureLod(prefilteredCube,rr,lod).rgb;
    vec2 ab=fayeEnvBRDFApprox(NoV,roughness);
    vec3 specular=prefiltered*(F*ab.x+ab.y);
    
    return(kD*diffuse+specular)*occlusion*lights.skyParams.y;
}

/**
* Blinn-Phong = r = k_a + sum_0_n (l_c * (r_d + r_s))
* where r_d = (k_d*(n.l)) and r_s = (k_s*(n.h))^p
* Cook-Torrance = r = k_a + sum_0_n (l_c * (n . l) * (d * r_d + s * r_s))
* where r_d = k_d and s + d = 1
* rs = (D * G * F) / (4 * (n . l) * (n . v))
* where D, G and F are pluggable values which make up specular reflection.
* D is Normali Distribution Function
* G is the Geometric Attenuation Function
* F is Fresnel Function
* n = normal vector
* l = light direction
* v = view direction
* h = half-angle vector norm(L+V)
*/

float maxdp(vec3 n, vec3 l) {
    return max(dot(n, l), 1e-4);
}

//Cook-Torrance calculations
float calcD(vec3 N,vec3 H,float m){
    float alpha=maxdp(N,H);
    float alpha2=alpha*alpha;
    float m2=max(m*m,1e-4);
    float ex=(alpha2-1)/(m2*alpha2);
    return exp(ex)/(3.14159*m2*(alpha2*alpha2));
}

vec3 calcF(vec3 f0,vec3 V,vec3 H){
    return f0+(1-f0)*pow(1-clamp(maxdp(V,H),0.,1.),5);
}

float calcG(vec3 N,vec3 V,vec3 H,vec3 L){
    float m2=(2*maxdp(N,H)*maxdp(N,V))/maxdp(V,H);
    float m3=(2*maxdp(N,H)*maxdp(N,L))/maxdp(V,H);
    return min(1,min(m2,m3));
}

void brdf(vec3 n,vec3 v,vec3 l,vec3 albedo, float metallic, float roughness, vec3 radiance, inout vec3 diffuse, inout vec3 specular){
    if (dot(n,l)<0.) return;
    
    vec3 h = normalize(v+l);
    
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 f = calcF(f0, v, h);
    
    specular+= radiance*f*(calcD(n,h,roughness)*calcG(n,v,h,l))/(4*maxdp(n,v));
    vec3 kd = (1-f)*(1-metallic);
    diffuse+=radiance*maxdp(n,l)*kd*albedo/3.14159;
}

/**
* Point + Directional lighting calc
*/

void fayeAccumulatePointLight(
    GpuPointLight light,vec3 fragPos,vec3 N,vec3 V,
    vec3 albedo,float metallic,float roughness,
inout vec3 diffuse,inout vec3 specular)
{
    vec3 toLight=light.position.xyz-fragPos;
    float attenuation=1./max(dot(toLight,toLight),1e-4);// inverse square
    vec3 L=normalize(toLight);
    vec3 radiance=light.color.rgb*light.color.w*attenuation;
    
    brdf(N,V,L,albedo,metallic,roughness,radiance,diffuse,specular);
}

void fayeAccumulateDirectionalLight(
    GpuDirectionalLight light,vec3 N,vec3 V,
    vec3 albedo,float metallic,float roughness,
inout vec3 diffuse,inout vec3 specular)
{
    vec3 L=normalize(-light.direction.xyz);
    vec3 radiance=light.color.rgb*light.color.w;
    
    brdf(N,V,L,albedo,metallic,roughness,radiance,diffuse,specular);
}

#endif// FAYE_LIGHTING_GLSL
