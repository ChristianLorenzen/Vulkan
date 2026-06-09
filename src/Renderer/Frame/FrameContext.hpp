#pragma once

#include <vulkan/vulkan.h>

#include "Scene/Camera/Camera.hpp"

namespace Faye
{
#define MAX_LIGHTS 10

    struct PointLight
    {
        glm::vec4 position{};
        glm::vec4 color{};
    };

    struct GlobalUBO
    {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::mat4 priorViewProjection{1.f};
        glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f};
        PointLight pointLights[MAX_LIGHTS];
        int numLights;
        float time = 0.0f;
        // Seconds elapsed since the previous frame. Used by water.vert to
        // re-evaluate Gerstner displacement at the prior frame's time so
        // motion vectors account for wave movement.
        float deltaTime = 0.0f;
        float _pad1 = 0.0f;
        float _pad2 = 0.0f;
        // Padding so inverseProjection sits on a 16-byte boundary (byte 624),
        // matching the std140 offset GLSL assigns to a mat4 member.
        float _pad3 = 0.0f;
        float _pad4 = 0.0f;
        float _pad5 = 0.0f;
        // Appended at the end so existing shader block declarations (which
        // omit it) keep identical member offsets. Used by water.frag to
        // convert prepass NDC depth to view-space distance exactly,
        // independent of the projection convention.
        glm::mat4 inverseProjection{1.f};
    };

    struct FrameContext
    {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        const Camera &camera;
        VkDescriptorSet globalDescriptorSet;
    };

    using FrameInfo = FrameContext;
}