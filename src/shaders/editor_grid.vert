#version 450

// Editor reference grid -- vertex stage.
//
// Draws a single oversized triangle covering the whole viewport (vkCmdDraw with
// 3 vertices, no vertex buffer). Rather than tessellating an actual grid mesh,
// we hand the fragment stage the two endpoints of the eye ray through this
// pixel, in world space; the fragment shader intersects that ray with the
// ground plane analytically. The grid is therefore truly infinite and costs one
// triangle regardless of how far the camera can see.
//
// Both endpoints are unprojected from the SAME NDC coordinate that we emit as
// gl_Position. That self-consistency means we never have to reason about
// whether the projection matrix carries a Y flip: whatever convention
// ubo.projection uses, ubo.inverseProjection undoes it.

#include "FayeGlobal.glsl"

layout(location = 0) out vec3 vNearPoint; // eye ray at the near plane, world space
layout(location = 1) out vec3 vFarPoint;  // eye ray at the far plane, world space

// Vulkan NDC depth runs [0, 1]: 0 == near plane, 1 == far plane.
vec3 unprojectToWorld(vec2 ndcXY, float ndcZ)
{
    vec4 viewPos = ubo.inverseProjection * vec4(ndcXY, ndcZ, 1.0);
    viewPos /= viewPos.w;
    return (ubo.inverseView * vec4(viewPos.xyz, 1.0)).xyz;
}

void main()
{
    // Fullscreen triangle: vertices at (-1,-1), (3,-1), (-1,3) in NDC.
    vec2 ndc = vec2(float((gl_VertexIndex << 1) & 2),
                    float(gl_VertexIndex & 2)) * 2.0 - 1.0;

    vNearPoint = unprojectToWorld(ndc, 0.0);
    vFarPoint = unprojectToWorld(ndc, 1.0);

    gl_Position = vec4(ndc, 0.0, 1.0);
}
