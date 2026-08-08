#pragma once

#include "vk_device.hpp"

namespace Faye
{

    // VMA-centric buffer memory usage. Replaces raw VkMemoryPropertyFlags so
    // callers express intent instead of leaking Vulkan memory-type flags.
    enum class BufferMemoryUsage
    {
        GpuOnly,    // device-local, no host access (vertex/index/storage buffers)
        HostVisible, // host-visible, sequential-write access (per-frame UBO updates)
        Staging,    // host-visible staging for uploads (transfer source)
    };

    class VulkanBuffer
    {
    public:
        VulkanBuffer(
            VulkanDevice &device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            BufferMemoryUsage memoryUsage,
            VkDeviceSize minOffsetAlignment = 1);
        ~VulkanBuffer();

        VulkanBuffer(const VulkanBuffer &) = delete;
        VulkanBuffer &operator=(const VulkanBuffer &) = delete;

        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap();

        void writeToBuffer(void *data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        void writeToIndex(void *data, int index);
        VkResult flushIndex(int index);
        VkDescriptorBufferInfo descriptorInfoForIndex(int index);
        VkResult invalidateIndex(int index);

        VkBuffer getBuffer() const { return buffer; }
        void *getMappedMemory() const { return mapped; }
        uint32_t getInstanceCount() const { return instanceCount; }
        VkDeviceSize getInstanceSize() const { return instanceSize; }
        VkDeviceSize getAlignmentSize() const { return instanceSize; }
        VkBufferUsageFlags getUsageFlags() const { return usageFlags; }
        VkDeviceSize getBufferSize() const { return bufferSize; }
        VkDeviceAddress getDeviceAddress() const;

    private:
        static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

        VulkanDevice &vk_device;
        void *mapped = nullptr;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        VkDeviceSize bufferSize;
        uint32_t instanceCount;
        VkDeviceSize instanceSize;
        VkDeviceSize alignmentSize;
        VkBufferUsageFlags usageFlags;
        BufferMemoryUsage memoryUsage;
    };

} // namespace lve