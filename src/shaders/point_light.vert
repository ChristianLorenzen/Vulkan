#version 450
#extension GL_GOOGLE_include_directive:require
#include "FayeGlobal.glsl"

const vec2 OFFSETS[6]=vec2[](
    vec2(-1.,-1.),
    vec2(-1.,1.),
    vec2(1.,-1.),
    vec2(1.,-1.),
    vec2(-1.,1.),
    vec2(1.,1.)
);

layout(location=0)out vec2 fragOffset;

layout(push_constant)uniform Push{
    vec4 position;
    vec4 color;
    float radius;
}push;

void main(){
    fragOffset=OFFSETS[gl_VertexIndex];
    vec3 cameraRightWorld=vec3(ubo.view[0][0],ubo.view[1][0],ubo.view[2][0]);
    vec3 cameraUpWorld=vec3(ubo.view[0][1],ubo.view[1][1],ubo.view[2][1]);
    
    vec3 positionWorld=push.position.xyz
    +push.radius*fragOffset.x*cameraRightWorld
    +push.radius*fragOffset.y*cameraUpWorld;
    
    gl_Position=ubo.projection*ubo.view*vec4(positionWorld,1.);
}