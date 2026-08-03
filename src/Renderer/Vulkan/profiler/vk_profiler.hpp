#pragma once

#include <stack>
#include <vector>
#include <string>
#include <string_view>
#include "Renderer/Vulkan/vk_device.hpp"
#include <vulkan/vulkan.h>

#define VK_PROFILER_MAX_SCOPES 64

namespace Faye::Profiler
{
    struct ScopeData {
        std::string name;
        uint32_t startQueryIndex;
        uint32_t depth;
    };

    struct FrameData {
        uint32_t queryIndex = 0;
        std::stack<uint32_t> scopeStack;
        std::vector<ScopeData> scopeData;
        VkQueryPool queryPool = VK_NULL_HANDLE;
    };

    struct ResolvedScope {
        std::string name;
        uint32_t depth;
        double milliseconds;
    };


    class VkProfiler
    {
        public:
            VkProfiler(VulkanDevice &device);
            ~VkProfiler();

            void calibrateClock(VkPhysicalDevice physicalDevice, VkDevice device);
            std::vector<FrameData> getFrames() const { return frames; }
            void createQueryPools(VkDevice device, uint32_t frames_in_flight);
            void fetchFrameDuration(VkDevice device);

            void beginFrame(VkCommandBuffer cmd, uint32_t frameIndex);
            void endFrame(VkCommandBuffer cmd, uint32_t frameIndex);

            void beginScope(VkCommandBuffer cmd, std::string_view scopeName);
            void endScope(VkCommandBuffer cmd);

            const std::vector<ResolvedScope>& getScopeResults() const { return resolvedScopes;};

            private:
                VulkanDevice &device;
                // both pools and frames have count FRAMES_IN_FLIGHT
                //std::vector<VkQueryPool> queryPools;
                std::vector<FrameData> frames;
                uint32_t frameIndex = 0;

                float timestampPeriod = 1.0f;
                uint32_t timestampValidBits = 64;
                std::vector<ResolvedScope> resolvedScopes;
    };

    class ScopedZone {
        
        public:
            ScopedZone(VkProfiler& profiler, VkCommandBuffer cmd, std::string_view scopeName)
                : profiler(profiler), cmd(cmd) {
                profiler.beginScope(cmd, scopeName);
            }

            ~ScopedZone() {
                profiler.endScope(cmd);
            }

            ScopedZone(const ScopedZone&) = delete;
            ScopedZone& operator=(const ScopedZone&) = delete;

        private:
            VkProfiler& profiler;
            VkCommandBuffer cmd;
    };

}