#version 450
#extension GL_GOOGLE_include_directive:require
// custom_material.vert - Example vertex shader using the Faye custom-material pipeline.
// Matches the push-constant and global UBO layout of the built-in PBR shader so it
// is compatible with the existing SimpleRenderSystem pipeline layout.

#extension GL_EXT_buffer_reference:require
#extension GL_EXT_buffer_reference2:require
#extension GL_EXT_scalar_block_layout:require
#extension GL_EXT_shader_explicit_arithmetic_types_int64:require

#include "FayeGlobal.glsl"

layout(buffer_reference,scalar)readonly buffer VertexBuffer{
    vec3 pos;
    vec3 color;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
};

layout(push_constant)uniform Push{
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
    uint64_t vertexBufferAddress;
}push;

layout(location=0)out vec3 fragColor;
layout(location=1)out vec3 fragPosWorld;
layout(location=2)out vec3 fragNormalWorld;
layout(location=3)out vec4 fragCurrentClip;
layout(location=4)out vec4 fragPriorClip;
layout(location=5)out vec2 fragTexCoord;
layout(location=6)out vec3 fragTangentWorld;
layout(location=7)out vec3 fragBitangentWorld;

void main()
{
    // Stride = scalar layout: 3*vec3(12) + vec2(8) + vec4(16) = 60 bytes
    VertexBuffer vtx=VertexBuffer(push.vertexBufferAddress+uint64_t(gl_VertexIndex)*uint64_t(60));
    
    vec4 positionWorld=push.modelMatrix*vec4(vtx.pos,1.);
    vec4 priorPositionWorld=push.priorModelMatrix*vec4(vtx.pos,1.);
    
    fragCurrentClip=ubo.projection*ubo.view*positionWorld;
    fragPriorClip=ubo.priorViewProjection*priorPositionWorld;
    gl_Position=fragCurrentClip;
    
    mat3 normalMatrix=transpose(inverse(mat3(push.modelMatrix)));
    fragNormalWorld=normalize(normalMatrix*vtx.normal);
    vec3 tangentWorld=normalize(mat3(push.modelMatrix)*vtx.tangent.xyz);
    fragTangentWorld=tangentWorld;
    fragBitangentWorld=normalize(cross(fragNormalWorld,tangentWorld)*vtx.tangent.w);
    fragPosWorld=positionWorld.xyz;
    fragTexCoord=vtx.uv;
    fragColor=push.baseColor.xyz;
}
