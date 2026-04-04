#include "vk_render_pass.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

namespace Faye
{
    namespace
    {
        bool isDepthAttachment(RenderPassAttachmentKind kind)
        {
            return kind == RenderPassAttachmentKind::DepthStencil;
        }

        bool isColorLikeAttachment(RenderPassAttachmentKind kind)
        {
            return kind == RenderPassAttachmentKind::Color ||
                   kind == RenderPassAttachmentKind::Motion ||
                   kind == RenderPassAttachmentKind::Resolve;
        }

        VkRect2D makeDefaultRenderArea(VkExtent2D extent)
        {
            return VkRect2D{{0, 0}, extent};
        }

        VkViewport makeFullscreenViewport(VkExtent2D extent)
        {
            return VkViewport{
                0.0f,
                0.0f,
                static_cast<float>(extent.width),
                static_cast<float>(extent.height),
                0.0f,
                1.0f};
        }
    }

    RenderPassAttachmentReference makeColorAttachmentRef(
        std::string attachmentName,
        VkImageLayout layout)
    {
        return RenderPassAttachmentReference{std::move(attachmentName), layout};
    }

    RenderPassAttachmentReference makeMotionAttachmentRef(
        std::string attachmentName,
        VkImageLayout layout)
    {
        return RenderPassAttachmentReference{std::move(attachmentName), layout};
    }

    RenderPassAttachmentReference makeDepthAttachmentRef(
        std::string attachmentName,
        VkImageLayout layout)
    {
        return RenderPassAttachmentReference{std::move(attachmentName), layout};
    }

    RenderPassSubpassSpecification makeGraphicsSubpass(
        std::vector<RenderPassAttachmentReference> colorAttachments,
        std::optional<RenderPassAttachmentReference> motionAttachment,
        std::optional<RenderPassAttachmentReference> depthStencilAttachment,
        std::vector<RenderPassAttachmentReference> inputAttachments,
        std::vector<RenderPassAttachmentReference> resolveAttachments,
        std::vector<std::string> preserveAttachments)
    {
        RenderPassSubpassSpecification subpass{};
        subpass.colorAttachments = std::move(colorAttachments);
        subpass.inputAttachments = std::move(inputAttachments);
        subpass.resolveAttachments = std::move(resolveAttachments);
        subpass.motionAttachment = std::move(motionAttachment);
        subpass.depthStencilAttachment = std::move(depthStencilAttachment);
        subpass.preserveAttachments = std::move(preserveAttachments);
        subpass.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        return subpass;
    }

    RenderPassBeginOptions makeFullscreenBeginOptions(
        VkExtent2D extent,
        VkSubpassContents contents)
    {
        RenderPassBeginOptions options{};
        options.renderArea = makeDefaultRenderArea(extent);
        options.viewport = makeFullscreenViewport(extent);
        options.scissor = makeDefaultRenderArea(extent);
        options.contents = contents;
        return options;
    }

    RenderPassBuilder &RenderPassBuilder::setName(std::string name)
    {
        specification.name = std::move(name);
        return *this;
    }

    RenderPassBuilder &RenderPassBuilder::addAttachment(RenderPassAttachmentSpecification attachment)
    {
        specification.attachments.push_back(std::move(attachment));
        return *this;
    }

    RenderPassBuilder &RenderPassBuilder::addMotionAttachment(
        std::string name,
        VkFormat format,
        VkImageLayout finalLayout,
        std::optional<VkClearColorValue> clearValue,
        VkAttachmentLoadOp loadOp,
        VkAttachmentStoreOp storeOp,
        VkImageLayout initialLayout,
        VkSampleCountFlagBits samples)
    {
        RenderPassAttachmentSpecification attachment{};
        attachment.name = std::move(name);
        attachment.kind = RenderPassAttachmentKind::Motion;
        attachment.format = format;
        attachment.samples = samples;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = initialLayout;
        attachment.finalLayout = finalLayout;

        if (clearValue.has_value())
        {
            VkClearValue attachmentClearValue{};
            attachmentClearValue.color = *clearValue;
            attachment.clearValue = attachmentClearValue;
        }

        return addAttachment(std::move(attachment));
    }

    RenderPassBuilder &RenderPassBuilder::addColorAttachment(
        std::string name,
        VkFormat format,
        VkImageLayout finalLayout,
        std::optional<VkClearColorValue> clearValue,
        VkAttachmentLoadOp loadOp,
        VkAttachmentStoreOp storeOp,
        VkImageLayout initialLayout,
        VkSampleCountFlagBits samples)
    {
        RenderPassAttachmentSpecification attachment{};
        attachment.name = std::move(name);
        attachment.kind = RenderPassAttachmentKind::Color;
        attachment.format = format;
        attachment.samples = samples;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = initialLayout;
        attachment.finalLayout = finalLayout;

        if (clearValue.has_value())
        {
            VkClearValue attachmentClearValue{};
            attachmentClearValue.color = *clearValue;
            attachment.clearValue = attachmentClearValue;
        }

        return addAttachment(std::move(attachment));
    }

    RenderPassBuilder &RenderPassBuilder::addDepthAttachment(
        std::string name,
        VkFormat format,
        VkImageLayout finalLayout,
        std::optional<VkClearDepthStencilValue> clearValue,
        VkAttachmentLoadOp loadOp,
        VkAttachmentStoreOp storeOp,
        VkImageLayout initialLayout,
        VkSampleCountFlagBits samples)
    {
        RenderPassAttachmentSpecification attachment{};
        attachment.name = std::move(name);
        attachment.kind = RenderPassAttachmentKind::DepthStencil;
        attachment.format = format;
        attachment.samples = samples;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = initialLayout;
        attachment.finalLayout = finalLayout;

        if (clearValue.has_value())
        {
            VkClearValue attachmentClearValue{};
            attachmentClearValue.depthStencil = *clearValue;
            attachment.clearValue = attachmentClearValue;
        }

        return addAttachment(std::move(attachment));
    }

    RenderPassBuilder &RenderPassBuilder::addSubpass(RenderPassSubpassSpecification subpass)
    {
        specification.subpasses.push_back(std::move(subpass));
        return *this;
    }

    RenderPassBuilder &RenderPassBuilder::addDependency(RenderPassDependencySpecification dependency)
    {
        specification.dependencies.push_back(std::move(dependency));
        return *this;
    }

    RenderPassSpecification RenderPassBuilder::build() &
    {
        return std::move(specification);
    }

    RenderPassSpecification RenderPassBuilder::build() &&
    {
        return std::move(specification);
    }

    VulkanRenderPass::VulkanRenderPass(VulkanDevice &deviceRef, RenderPassSpecification specification)
        : device(&deviceRef), specification(std::move(specification))
    {
        buildAttachmentIndexLookup();
        validateSpecification();
        createRenderPass();
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        destroy();
    }

    VulkanRenderPass::VulkanRenderPass(VulkanRenderPass &&other) noexcept
        : device(other.device),
          specification(std::move(other.specification)),
          renderPass(other.renderPass),
          attachmentIndices(std::move(other.attachmentIndices)),
          defaultClearValues(std::move(other.defaultClearValues))
    {
        other.device = nullptr;
        other.renderPass = VK_NULL_HANDLE;
    }

    VulkanRenderPass &VulkanRenderPass::operator=(VulkanRenderPass &&other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        destroy();

        device = other.device;
        specification = std::move(other.specification);
        renderPass = other.renderPass;
        attachmentIndices = std::move(other.attachmentIndices);
        defaultClearValues = std::move(other.defaultClearValues);

        other.device = nullptr;
        other.renderPass = VK_NULL_HANDLE;
        return *this;
    }

    bool VulkanRenderPass::hasAttachment(std::string_view name) const
    {
        return attachmentIndices.contains(std::string(name));
    }

    uint32_t VulkanRenderPass::getAttachmentIndex(std::string_view name) const
    {
        const auto it = attachmentIndices.find(std::string(name));
        if (it == attachmentIndices.end())
        {
            throw std::runtime_error("Unknown render pass attachment: " + std::string(name));
        }
        return it->second;
    }

    void VulkanRenderPass::validateSpecification() const
    {
        if (specification.attachments.empty())
        {
            throw std::runtime_error("Render pass must declare at least one attachment");
        }

        if (specification.subpasses.empty())
        {
            throw std::runtime_error("Render pass must declare at least one subpass");
        }

        for (const auto &attachment : specification.attachments)
        {
            if (attachment.name.empty())
            {
                throw std::runtime_error("Render pass attachment names must not be empty");
            }

            if (attachment.format == VK_FORMAT_UNDEFINED)
            {
                throw std::runtime_error("Render pass attachment '" + attachment.name + "' must define a valid format");
            }
        }

        for (size_t subpassIndex = 0; subpassIndex < specification.subpasses.size(); ++subpassIndex)
        {
            const auto &subpass = specification.subpasses[subpassIndex];

            if (!subpass.resolveAttachments.empty() && subpass.resolveAttachments.size() != subpass.colorAttachments.size())
            {
                throw std::runtime_error("Render pass subpass resolve attachment count must match color attachment count");
            }

            for (const auto &reference : subpass.colorAttachments)
            {
                const auto &attachment = specification.attachments.at(getAttachmentIndex(reference.attachmentName));
                if (!isColorLikeAttachment(attachment.kind))
                {
                    throw std::runtime_error("Subpass color attachment '" + reference.attachmentName + "' must reference a color or resolve attachment");
                }
            }

            for (const auto &reference : subpass.resolveAttachments)
            {
                const auto &attachment = specification.attachments.at(getAttachmentIndex(reference.attachmentName));
                if (attachment.kind != RenderPassAttachmentKind::Resolve)
                {
                    throw std::runtime_error("Subpass resolve attachment '" + reference.attachmentName + "' must reference a resolve attachment");
                }
            }

            if (subpass.motionAttachment.has_value())
            {
                const auto &attachment = specification.attachments.at(getAttachmentIndex(subpass.motionAttachment->attachmentName));
                if (attachment.kind != RenderPassAttachmentKind::Motion)
                {
                    throw std::runtime_error("Subpass motion attachment '" + subpass.motionAttachment->attachmentName + "' must reference a motion attachment");
                }
            }

            for (const auto &reference : subpass.inputAttachments)
            {
                (void)specification.attachments.at(getAttachmentIndex(reference.attachmentName));
            }

            if (subpass.depthStencilAttachment.has_value())
            {
                const auto &attachment = specification.attachments.at(getAttachmentIndex(subpass.depthStencilAttachment->attachmentName));
                if (!isDepthAttachment(attachment.kind))
                {
                    throw std::runtime_error("Subpass depth attachment '" + subpass.depthStencilAttachment->attachmentName + "' must reference a depth attachment");
                }
            }

            for (const auto &attachmentName : subpass.preserveAttachments)
            {
                (void)getAttachmentIndex(attachmentName);
            }
        }
    }

    void VulkanRenderPass::buildAttachmentIndexLookup()
    {
        attachmentIndices.clear();
        attachmentIndices.reserve(specification.attachments.size());
        defaultClearValues.clear();
        defaultClearValues.resize(specification.attachments.size(), VkClearValue{});

        for (uint32_t index = 0; index < specification.attachments.size(); ++index)
        {
            const auto &attachment = specification.attachments[index];
            const auto [_, inserted] = attachmentIndices.emplace(attachment.name, index);
            if (!inserted)
            {
                throw std::runtime_error("Duplicate render pass attachment name: " + attachment.name);
            }

            if (attachment.clearValue.has_value())
            {
                defaultClearValues[index] = *attachment.clearValue;
            }
        }
    }

    void VulkanRenderPass::createRenderPass()
    {
        std::vector<VkAttachmentDescription> attachmentDescriptions;
        attachmentDescriptions.reserve(specification.attachments.size());

        for (const auto &attachment : specification.attachments)
        {
            VkAttachmentDescription description{};
            description.format = attachment.format;
            description.samples = attachment.samples;
            description.loadOp = attachment.loadOp;
            description.storeOp = attachment.storeOp;
            description.stencilLoadOp = attachment.stencilLoadOp;
            description.stencilStoreOp = attachment.stencilStoreOp;
            description.initialLayout = attachment.initialLayout;
            description.finalLayout = attachment.finalLayout;
            attachmentDescriptions.push_back(description);
        }

        struct CompiledSubpass
        {
            VkSubpassDescription description{};
            std::vector<VkAttachmentReference> colorAttachments;
            std::vector<VkAttachmentReference> inputAttachments;
            std::vector<VkAttachmentReference> resolveAttachments;
            std::optional<VkAttachmentReference> depthAttachment;
            std::vector<uint32_t> preserveAttachments;
        };

        std::vector<CompiledSubpass> compiledSubpasses(specification.subpasses.size());
        std::vector<VkSubpassDescription> subpassDescriptions(specification.subpasses.size());

        for (size_t subpassIndex = 0; subpassIndex < specification.subpasses.size(); ++subpassIndex)
        {
            const auto &subpassSpec = specification.subpasses[subpassIndex];
            auto &compiledSubpass = compiledSubpasses[subpassIndex];

            for (const auto &reference : subpassSpec.colorAttachments)
            {
                compiledSubpass.colorAttachments.push_back({getAttachmentIndex(reference.attachmentName),
                                                            reference.layout});
            }

            if (subpassSpec.motionAttachment.has_value())
            {
                compiledSubpass.colorAttachments.push_back({getAttachmentIndex(subpassSpec.motionAttachment->attachmentName),
                                                            subpassSpec.motionAttachment->layout});
            }

            for (const auto &reference : subpassSpec.inputAttachments)
            {
                compiledSubpass.inputAttachments.push_back({getAttachmentIndex(reference.attachmentName),
                                                            reference.layout});
            }

            for (const auto &reference : subpassSpec.resolveAttachments)
            {
                compiledSubpass.resolveAttachments.push_back({getAttachmentIndex(reference.attachmentName),
                                                              reference.layout});
            }

            if (subpassSpec.depthStencilAttachment.has_value())
            {
                compiledSubpass.depthAttachment = VkAttachmentReference{
                    getAttachmentIndex(subpassSpec.depthStencilAttachment->attachmentName),
                    subpassSpec.depthStencilAttachment->layout};
            }

            for (const auto &attachmentName : subpassSpec.preserveAttachments)
            {
                compiledSubpass.preserveAttachments.push_back(getAttachmentIndex(attachmentName));
            }

            compiledSubpass.description.pipelineBindPoint = subpassSpec.bindPoint;
            compiledSubpass.description.colorAttachmentCount = static_cast<uint32_t>(compiledSubpass.colorAttachments.size());
            compiledSubpass.description.pColorAttachments = compiledSubpass.colorAttachments.empty() ? nullptr : compiledSubpass.colorAttachments.data();
            compiledSubpass.description.inputAttachmentCount = static_cast<uint32_t>(compiledSubpass.inputAttachments.size());
            compiledSubpass.description.pInputAttachments = compiledSubpass.inputAttachments.empty() ? nullptr : compiledSubpass.inputAttachments.data();
            compiledSubpass.description.pResolveAttachments = compiledSubpass.resolveAttachments.empty() ? nullptr : compiledSubpass.resolveAttachments.data();
            compiledSubpass.description.pDepthStencilAttachment = compiledSubpass.depthAttachment.has_value() ? &compiledSubpass.depthAttachment.value() : nullptr;
            compiledSubpass.description.preserveAttachmentCount = static_cast<uint32_t>(compiledSubpass.preserveAttachments.size());
            compiledSubpass.description.pPreserveAttachments = compiledSubpass.preserveAttachments.empty() ? nullptr : compiledSubpass.preserveAttachments.data();

            subpassDescriptions[subpassIndex] = compiledSubpass.description;
        }

        std::vector<VkSubpassDependency> dependencies;
        dependencies.reserve(specification.dependencies.size());
        for (const auto &dependencySpec : specification.dependencies)
        {
            VkSubpassDependency dependency{};
            dependency.srcSubpass = dependencySpec.srcSubpass;
            dependency.dstSubpass = dependencySpec.dstSubpass;
            dependency.srcStageMask = dependencySpec.srcStageMask;
            dependency.dstStageMask = dependencySpec.dstStageMask;
            dependency.srcAccessMask = dependencySpec.srcAccessMask;
            dependency.dstAccessMask = dependencySpec.dstAccessMask;
            dependency.dependencyFlags = dependencySpec.dependencyFlags;
            dependencies.push_back(dependency);
        }

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
        createInfo.pAttachments = attachmentDescriptions.data();
        createInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
        createInfo.pSubpasses = subpassDescriptions.data();
        createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        createInfo.pDependencies = dependencies.empty() ? nullptr : dependencies.data();

        if (vkCreateRenderPass(device->getDevice(), &createInfo, nullptr, &renderPass) != VK_SUCCESS)
        {
            const std::string passName = specification.name.empty() ? "unnamed" : specification.name;
            throw std::runtime_error("Failed to create render pass: " + passName);
        }
    }

    void VulkanRenderPass::destroy()
    {
        if (device != nullptr && renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device->getDevice(), renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
    }

    VulkanRenderPassInstance::VulkanRenderPassInstance(
        VulkanDevice &deviceRef,
        const VulkanRenderPass &renderPassRef,
        VkExtent2D renderExtent,
        std::vector<VkImageView> framebufferAttachments,
        uint32_t framebufferLayers)
        : device(&deviceRef),
          renderPass(&renderPassRef),
          extent(renderExtent),
          attachments(std::move(framebufferAttachments)),
          layers(framebufferLayers)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            throw std::runtime_error("Render pass framebuffer extent must be non-zero");
        }

        if (attachments.size() != renderPass->getAttachmentCount())
        {
            throw std::runtime_error("Framebuffer attachment count must match render pass attachment count");
        }

        createFramebuffer();
    }

    VulkanRenderPassInstance::~VulkanRenderPassInstance()
    {
        destroy();
    }

    VulkanRenderPassInstance::VulkanRenderPassInstance(VulkanRenderPassInstance &&other) noexcept
        : device(other.device),
          renderPass(other.renderPass),
          extent(other.extent),
          attachments(std::move(other.attachments)),
          layers(other.layers),
          framebuffer(other.framebuffer)
    {
        other.device = nullptr;
        other.renderPass = nullptr;
        other.framebuffer = VK_NULL_HANDLE;
        other.layers = 1;
        other.extent = {};
    }

    VulkanRenderPassInstance &VulkanRenderPassInstance::operator=(VulkanRenderPassInstance &&other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        destroy();

        device = other.device;
        renderPass = other.renderPass;
        extent = other.extent;
        attachments = std::move(other.attachments);
        layers = other.layers;
        framebuffer = other.framebuffer;

        other.device = nullptr;
        other.renderPass = nullptr;
        other.framebuffer = VK_NULL_HANDLE;
        other.layers = 1;
        other.extent = {};
        return *this;
    }

    void VulkanRenderPassInstance::begin(VkCommandBuffer commandBuffer, const RenderPassBeginOptions &options) const
    {
        assert(framebuffer != VK_NULL_HANDLE && "Cannot begin a render pass instance without a framebuffer.");
        assert(renderPass != nullptr && "Cannot begin a render pass instance without a render pass.");

        const auto &clearValues = options.clearValues.empty() ? renderPass->getDefaultClearValues() : options.clearValues;
        const VkRect2D renderArea = options.renderArea.value_or(makeDefaultRenderArea(extent));

        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = renderPass->getHandle();
        beginInfo.framebuffer = framebuffer;
        beginInfo.renderArea = renderArea;
        beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        beginInfo.pClearValues = clearValues.empty() ? nullptr : clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &beginInfo, options.contents);

        if (options.viewport.has_value())
        {
            vkCmdSetViewport(commandBuffer, 0, 1, &options.viewport.value());
        }

        if (options.scissor.has_value())
        {
            vkCmdSetScissor(commandBuffer, 0, 1, &options.scissor.value());
        }
    }

    void VulkanRenderPassInstance::end(VkCommandBuffer commandBuffer) const
    {
        vkCmdEndRenderPass(commandBuffer);
    }

    void VulkanRenderPassInstance::createFramebuffer()
    {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass->getHandle();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = layers;

        if (vkCreateFramebuffer(device->getDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer for render pass instance");
        }
    }

    void VulkanRenderPassInstance::destroy()
    {
        if (device != nullptr && framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device->getDevice(), framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
}