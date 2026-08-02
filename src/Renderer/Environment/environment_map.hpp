#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "Renderer/Vulkan/vk_compute_pipeline.hpp"
#include "Renderer/Vulkan/vk_descriptors.hpp"
#include "Renderer/Vulkan/VkImageResource.hpp"
#include "Renderer/Vulkan/VkSamplerResource.hpp"
#include "Renderer/Material/Material.hpp"


namespace Faye {
    class EnvironmentMap {
    public:
        bool load(VulkanDevice&, const std::string &hdrPath);
        void destroy(VkDevice device);
        bool isValid() const;
        VkDescriptorImageInfo descriptorInfo() const;

    private:
        VkImageResource resource;
        VkSamplerResource samplerResource;
    };
}