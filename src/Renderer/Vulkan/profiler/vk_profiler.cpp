#include "vk_profiler.hpp"
#include "Renderer/Vulkan/vk_swapchain.hpp"


namespace Faye::Profiler {

    VkProfiler::VkProfiler(VulkanDevice &device)
        : device(device) {
        createQueryPools(device.getDevice(), VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);

        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device.getPhysicalDevice(), &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device.getPhysicalDevice(), &familyCount, families.data());

        timestampValidBits = families[device.getGraphicsQueueFamilyIndex()].timestampValidBits;
    }

    VkProfiler::~VkProfiler() {
        for (const auto& frame : frames) {
            if (frame.queryPool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(device.getDevice(), frame.queryPool, nullptr);
            }
        }
    }

    void VkProfiler::createQueryPools(VkDevice device, uint32_t frames_in_flight) {
        
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2 * VK_PROFILER_MAX_SCOPES; // Start and end timestamps

        for (uint32_t i = 0; i < frames_in_flight; ++i) {
            FrameData frameData{};

            VkQueryPool queryPool;
            if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create query pool for frame " + std::to_string(i));
            }
            frameData.queryPool = queryPool;
            frames.push_back(frameData);
        }
    }

    void VkProfiler::fetchFrameDuration(VkDevice device) {
        FrameData &frame = frames[frameIndex];
        if (frame.queryIndex == 0) return;

        const uint32_t numTimestamps = frame.queryIndex;
        std::vector<uint64_t> timestamps(numTimestamps * 2);

        VkResult res = vkGetQueryPoolResults(device, frame.queryPool,
             0, numTimestamps, sizeof(uint64_t) * timestamps.size(),
              timestamps.data(), sizeof(uint64_t) * 2, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    
        if (res != VK_SUCCESS && res != VK_NOT_READY) {
            throw std::runtime_error("Failed to get query pool results");
        }

        const uint64_t mask = (timestampValidBits >= 64) ? ~0ULL : ((1ULL << timestampValidBits) - 1);

        resolvedScopes.clear();
        for (const ScopeData &scope : frame.scopeData) {
            const uint32_t b = scope.startQueryIndex;
            const uint32_t e = b + 1;

            if (timestamps[b * 2 + 1] == 0 || timestamps[e * 2 + 1] == 0) continue;

            const uint64_t begin = timestamps[b*2] & mask;
            const uint64_t end = timestamps[e*2] & mask;

            const double ms = static_cast<double>(end - begin) * timestampPeriod / 1'000'000.0;
            resolvedScopes.push_back({scope.name, scope.depth, ms});
        }
    }

    void VkProfiler::beginFrame(VkCommandBuffer cmd, uint32_t frameIndex) {
        fetchFrameDuration(device.getDevice());

        this->frameIndex = frameIndex;
        Profiler::FrameData &frameData = frames[frameIndex];
        vkCmdResetQueryPool(cmd, frameData.queryPool, 0, 2 * VK_PROFILER_MAX_SCOPES); // Reset the query pool for this frame
        frameData.queryIndex = 0; // Reset the query index for this frame
        frameData.scopeData.clear(); // Clear the scope data stack for this frame
        frameData.scopeStack = std::stack<uint32_t>{}; // Clear the scope stack for this frame
    }

    void VkProfiler::endFrame(VkCommandBuffer cmd, uint32_t frameIndex) {
        FrameData &frame = frames[frameIndex];
        if (frame.scopeStack.size() != 0) {
            throw std::runtime_error("Mismatched beginScope/endScope calls at the end of frame " + std::to_string(frameIndex));
        }
    }

    void VkProfiler::beginScope(VkCommandBuffer cmd, std::string_view scopeName) {
        FrameData &frame = frames[frameIndex];
        const uint32_t queryIndex = frame.queryIndex;
        frame.queryIndex += 2; //start/end

        std::string name = std::string(scopeName);
        frame.scopeStack.push(queryIndex);
        frame.scopeData.push_back({name, queryIndex, static_cast<uint32_t>(frame.scopeStack.size()) - 1});

        device.cmdBeginLabel(cmd, name.c_str());

        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.queryPool, queryIndex);
    }

    void VkProfiler::endScope(VkCommandBuffer cmd) {
        FrameData &frame = frames[frameIndex];
        if (frame.scopeStack.empty()) {
            throw std::runtime_error("Mismatched beginScope/endScope calls");
        }

        const uint32_t queryIndex = frame.scopeStack.top();
        frame.scopeStack.pop();

        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.queryPool, queryIndex + 1);

        device.cmdEndLabel(cmd);
    }
}