#version 450
#extension GL_EXT_nonuniform_qualifier:require
#extension GL_GOOGLE_include_directive:require

layout(location=0)in vec3 vNearPoint;
layout(location=1)in vec3 vFarPoint;

layout(location=0)out vec4 outColor;
layout(location=1)out vec2 outMotion;

layout(set=0,binding=3)uniform sampler2D skyboxSampler;

void main(){
    vec3 d=normalize(vFarPoint-vNearPoint);
    vec2 uv=vec2(atan(d.z,d.x)/6.283185+.5,acos(clamp(d.y,-1.,1.))/3.141593);
    vec3 sky=texture(skyboxSampler,uv).rgb;
    outColor=vec4(sky,1.);
}