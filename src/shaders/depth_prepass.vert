#version 450
// depth_prepass.vert -- Minimal vertex shader for the opaque depth prepass.
// Outputs only gl_Position; no fragment outputs needed.
// Uses the same push-constant layout as shader.vert so the same pipeline
// layout can be reused.

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "FayeGlobal.glsl"

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    vec3 pos;
    vec3 color;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
};

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
    uint64_t vertexBufferAddress;
} push;

void main()
{
    // Stride = scalar layout: 3*vec3(12) + vec2(8) + vec4(16) = 60 bytes
    VertexBuffer vtx = VertexBuffer(push.vertexBufferAddress + uint64_t(gl_VertexIndex) * uint64_t(60));
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(vtx.pos, 1.0);
}
