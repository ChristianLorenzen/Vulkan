#pragma once

#include <Camera/Camera.hpp>
#include <Vulkan/Vulkan.hpp>

namespace Faye {
    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        Camera &camera;
        VkDescriptorSet globalDescriptorSet;
    };
}