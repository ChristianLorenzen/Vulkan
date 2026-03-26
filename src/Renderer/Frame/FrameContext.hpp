#pragma once

#include <vulkan/vulkan.h>

#include "Scene/Camera/Camera.hpp"

namespace Faye {
    struct FrameContext {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        const Camera &camera;
        VkDescriptorSet globalDescriptorSet;
    };

    using FrameInfo = FrameContext;
}