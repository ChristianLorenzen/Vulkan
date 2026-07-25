// FayeGlobal.glsl - The single authoritative camera/per-frame uniform block.
//
// Mirrors the C++ `GlobalUBO` in Renderer/Frame/FrameContext.hpp exactly. Bound
// at set 0, binding 0 (VK_SHADER_STAGE_ALL_GRAPHICS). Include this in every
// shader that needs camera matrices or frame time. Lighting is separate — see
// FayeLighting.glsl (set 0, binding 2).
//
// std140 offsets (must match the C++ struct):
//   projection 0, view 64, inverseView 128, priorViewProjection 192,
//   time 256, deltaTime 260, _pad1 264, _pad2 268, inverseProjection 272.

#ifndef FAYE_GLOBAL_GLSL
#define FAYE_GLOBAL_GLSL

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4  projection;
    mat4  view;
    mat4  inverseView;
    mat4  priorViewProjection;
    float time;
    float deltaTime;          // seconds since the previous frame
    float _pad1;
    float _pad2;
    mat4  inverseProjection;  // exact NDC-depth -> view-space unprojection
} ubo;

// World-space camera position, extracted from the inverse view matrix.
vec3 fayeCameraPosWorld() { return ubo.inverseView[3].xyz; }

#endif // FAYE_GLOBAL_GLSL
