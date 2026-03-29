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
        glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f};
        PointLight pointLights[MAX_LIGHTS];
        int numLights;
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