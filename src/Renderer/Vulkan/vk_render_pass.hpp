#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "vk_device.hpp"

namespace Faye
{
    enum class RenderPassAttachmentKind
    {
        Color,
        Motion,
        DepthStencil,
        Resolve,
        Input
    };

    struct RenderPassAttachmentReference
    {
        std::string attachmentName;
        VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    };

    struct RenderPassAttachmentSpecification
    {
        std::string name;
        RenderPassAttachmentKind kind = RenderPassAttachmentKind::Color;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        std::optional<VkClearValue> clearValue;
    };

    struct RenderPassSubpassSpecification
    {
        std::vector<RenderPassAttachmentReference> colorAttachments;
        std::vector<RenderPassAttachmentReference> inputAttachments;
        std::vector<RenderPassAttachmentReference> resolveAttachments;
        std::optional<RenderPassAttachmentReference> motionAttachment;
        std::optional<RenderPassAttachmentReference> depthStencilAttachment;
        std::vector<std::string> preserveAttachments;
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    };

    struct RenderPassDependencySpecification
    {
        uint32_t srcSubpass = VK_SUBPASS_EXTERNAL;
        uint32_t dstSubpass = 0;
        VkPipelineStageFlags srcStageMask = 0;
        VkPipelineStageFlags dstStageMask = 0;
        VkAccessFlags srcAccessMask = 0;
        VkAccessFlags dstAccessMask = 0;
        VkDependencyFlags dependencyFlags = 0;
    };

    struct RenderPassSpecification
    {
        std::string name;
        std::vector<RenderPassAttachmentSpecification> attachments;
        std::vector<RenderPassSubpassSpecification> subpasses;
        std::vector<RenderPassDependencySpecification> dependencies;
    };

    struct RenderPassBeginOptions
    {
        std::optional<VkRect2D> renderArea;
        std::vector<VkClearValue> clearValues;
        std::optional<VkViewport> viewport;
        std::optional<VkRect2D> scissor;
        VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE;
    };

    RenderPassAttachmentReference makeColorAttachmentRef(
        std::string attachmentName,
        VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    RenderPassAttachmentReference makeMotionAttachmentRef(
        std::string attachmentName,
        VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    RenderPassAttachmentReference makeDepthAttachmentRef(
        std::string attachmentName,
        VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    RenderPassSubpassSpecification makeGraphicsSubpass(
        std::vector<RenderPassAttachmentReference> colorAttachments,
        std::optional<RenderPassAttachmentReference> motionAttachment = std::nullopt,
        std::optional<RenderPassAttachmentReference> depthStencilAttachment = std::nullopt,
        std::vector<RenderPassAttachmentReference> inputAttachments = {},
        std::vector<RenderPassAttachmentReference> resolveAttachments = {},
        std::vector<std::string> preserveAttachments = {});

    RenderPassBeginOptions makeFullscreenBeginOptions(
        VkExtent2D extent,
        VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

    class RenderPassBuilder
    {
    public:
        RenderPassBuilder &setName(std::string name);
        RenderPassBuilder &addAttachment(RenderPassAttachmentSpecification attachment);
        RenderPassBuilder &addColorAttachment(
            std::string name,
            VkFormat format,
            VkImageLayout finalLayout,
            std::optional<VkClearColorValue> clearValue = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}},
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
        RenderPassBuilder &addMotionAttachment(
            std::string name,
            VkFormat format,
            VkImageLayout finalLayout,
            std::optional<VkClearColorValue> clearValue = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}},
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
        RenderPassBuilder &addDepthAttachment(
            std::string name,
            VkFormat format,
            VkImageLayout finalLayout,
            std::optional<VkClearDepthStencilValue> clearValue = VkClearDepthStencilValue{1.0f, 0},
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
        RenderPassBuilder &addSubpass(RenderPassSubpassSpecification subpass);
        RenderPassBuilder &addDependency(RenderPassDependencySpecification dependency);

        RenderPassSpecification build() &;
        RenderPassSpecification build() &&;

    private:
        RenderPassSpecification specification;
    };

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(VulkanDevice &device, RenderPassSpecification specification);
        ~VulkanRenderPass();

        VulkanRenderPass(const VulkanRenderPass &) = delete;
        VulkanRenderPass &operator=(const VulkanRenderPass &) = delete;

        VulkanRenderPass(VulkanRenderPass &&other) noexcept;
        VulkanRenderPass &operator=(VulkanRenderPass &&other) noexcept;

        VkRenderPass getHandle() const { return renderPass; }
        const RenderPassSpecification &getSpecification() const { return specification; }
        const std::vector<VkClearValue> &getDefaultClearValues() const { return defaultClearValues; }
        uint32_t getAttachmentCount() const { return static_cast<uint32_t>(specification.attachments.size()); }
        bool hasAttachment(std::string_view name) const;
        uint32_t getAttachmentIndex(std::string_view name) const;

    private:
        void validateSpecification() const;
        void buildAttachmentIndexLookup();
        void createRenderPass();
        void destroy();

        VulkanDevice *device = nullptr;
        RenderPassSpecification specification;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::unordered_map<std::string, uint32_t> attachmentIndices;
        std::vector<VkClearValue> defaultClearValues;
    };

    class VulkanRenderPassInstance
    {
    public:
        VulkanRenderPassInstance(
            VulkanDevice &device,
            const VulkanRenderPass &renderPass,
            VkExtent2D extent,
            std::vector<VkImageView> attachments,
            uint32_t layers = 1);
        ~VulkanRenderPassInstance();

        VulkanRenderPassInstance(const VulkanRenderPassInstance &) = delete;
        VulkanRenderPassInstance &operator=(const VulkanRenderPassInstance &) = delete;

        VulkanRenderPassInstance(VulkanRenderPassInstance &&other) noexcept;
        VulkanRenderPassInstance &operator=(VulkanRenderPassInstance &&other) noexcept;

        VkFramebuffer getFramebuffer() const { return framebuffer; }
        VkExtent2D getExtent() const { return extent; }
        const VulkanRenderPass &getRenderPass() const { return *renderPass; }

        void begin(VkCommandBuffer commandBuffer, const RenderPassBeginOptions &options = {}) const;
        void end(VkCommandBuffer commandBuffer) const;

    private:
        void createFramebuffer();
        void destroy();

        VulkanDevice *device = nullptr;
        const VulkanRenderPass *renderPass = nullptr;
        VkExtent2D extent{};
        std::vector<VkImageView> attachments;
        uint32_t layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };
}