#version 450
#extension GL_GOOGLE_include_directive:require

#include "FayeGlobal.glsl"

layout(location=0)out vec3 vNearPoint;// eye ray at the near plane, world space
layout(location=1)out vec3 vFarPoint;// eye ray at the far plane, world space

// Vulkan NDC depth runs [0, 1]: 0 == near plane, 1 == far plane.
vec3 unprojectToWorld(vec2 ndcXY,float ndcZ)
{
    vec4 viewPos=ubo.inverseProjection*vec4(ndcXY,ndcZ,1.);
    viewPos/=viewPos.w;
    return(ubo.inverseView*vec4(viewPos.xyz,1.)).xyz;
}

void main()
{
    // Fullscreen triangle: vertices at (-1,-1), (3,-1), (-1,3) in NDC.
    vec2 ndc=vec2(float((gl_VertexIndex<<1)&2),
    float(gl_VertexIndex&2))*2.-1.;
    
    vNearPoint=unprojectToWorld(ndc,0.);
    vFarPoint=unprojectToWorld(ndc,1.);
    
    gl_Position=vec4(ndc,1.,1.);
}
