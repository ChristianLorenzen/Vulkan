#version 450
#extension GL_EXT_nonuniform_qualifier:require
#extension GL_GOOGLE_include_directive:require

layout(location=0)in vec3 vNearPoint;
layout(location=1)in vec3 vFarPoint;

layout(location=0)out vec4 outColor;
layout(location=1)out vec2 outMotion;

layout(set=0,binding=3)uniform samplerCube skyCube;

layout(push_constant)uniform Push{float rotation;float intensity;}push;

vec3 rotateY(vec3 v,float a){
    float s=sin(a),c=cos(a);
    return vec3(c*v.x+s*v.z,v.y,-s*v.x+c*v.z);
}

void main(){
    vec3 d=normalize(vFarPoint-vNearPoint);
    outColor=vec4(texture(skyCube,rotateY(d,push.rotation)).rgb*push.intensity,1.);
}