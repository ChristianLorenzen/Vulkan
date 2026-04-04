#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include "vk_post_process.hpp"

#include "vk_renderer.hpp"

#include <cassert>
#include <stdexcept>

Faye::FullscreenEffectPass::FullscreenEffectPass(
    VulkanDevice &device,
    VkRenderPass renderPass,
    VulkanDescriptorPool &pool,
    PostProcessEffectDefinition definition)
    : vk_device(device),
      effectDefinition(std::move(definition)),
      descriptorPool(pool),
      renderPass(renderPass)
{
    LOG_INFO(Logger::getInstance(), "Creating FullscreenEffectPass...");
    createDescriptorSetLayout();
    createSceneColorSampler();
    createPipelineLayout();
    createPipeline();
}

Faye::FullscreenEffectPass::~FullscreenEffectPass()
{
    std::vector<VkDescriptorSet> validDescriptorSets;
    validDescriptorSets.reserve(descriptorSets.size());
    for (VkDescriptorSet descriptorSet : descriptorSets)
    {
        if (descriptorSet != VK_NULL_HANDLE)
        {
            validDescriptorSets.push_back(descriptorSet);
        }
    }

    if (!validDescriptorSets.empty())
    {
        descriptorPool.freeDescriptors(validDescriptorSets);
    }

    if (sceneColorSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(vk_device.getDevice(), sceneColorSampler, nullptr);
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(vk_device.getDevice(), pipelineLayout, nullptr);
    }
}

void Faye::FullscreenEffectPass::createDescriptorSetLayout()
{
    VulkanDescriptorSetLayout::Builder layoutBuilder{vk_device};
    for (const auto &input : effectDefinition.inputs)
    {
        layoutBuilder.addBinding(input.binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    descriptorSetLayout = layoutBuilder.build();
}

void Faye::FullscreenEffectPass::createSceneColorSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(vk_device.getDevice(), &samplerInfo, nullptr, &sceneColorSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create post process scene color sampler");
    }
}

void Faye::FullscreenEffectPass::updateDescriptorSet(
    uint32_t frameIndex,
    const std::unordered_map<std::string, std::vector<VkImageView>> &namedInputs)
{
    if (descriptorSets.size() <= frameIndex)
    {
        descriptorSets.resize(frameIndex + 1, VK_NULL_HANDLE);
    }

    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(effectDefinition.inputs.size());
    VulkanDescriptorWriter descriptorWriter{*descriptorSetLayout, descriptorPool};

    for (const auto &input : effectDefinition.inputs)
    {
        const auto inputIterator = namedInputs.find(input.semanticName);
        if (inputIterator == namedInputs.end())
        {
            throw std::runtime_error("Missing named post process input for effect");
        }

        if (frameIndex >= inputIterator->second.size())
        {
            throw std::runtime_error("Named post process input does not provide enough frame image views");
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sceneColorSampler;
        imageInfo.imageView = inputIterator->second[frameIndex];
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos.push_back(imageInfo);
        descriptorWriter.writeImage(input.binding, &imageInfos.back());
    }

    VkDescriptorSet &descriptorSet = descriptorSets[frameIndex];
    if (descriptorSet == VK_NULL_HANDLE)
    {
        if (!descriptorWriter.build(descriptorSet))
        {
            throw std::runtime_error("Failed to allocate fullscreen effect descriptor set");
        }
    }
    else
    {
        descriptorWriter.overwrite(descriptorSet);
    }
}

void Faye::FullscreenEffectPass::createPipelineLayout()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptorSetLayout->getDescriptorSetLayout()};
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PostProcessParameterBlock);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    LOG_INFO(Logger::getInstance(), "Created pipelinelayoutinfo struct...");

    if (vkCreatePipelineLayout(vk_device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create post process pipeline layout");
    }
}

void Faye::FullscreenEffectPass::createPipeline()
{
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout is null");

    PipelineConfigInfo pipelineConfig{};
    VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    vk_pipeline = std::make_unique<VulkanPipeline>(
        vk_device,
        effectDefinition.vertexShaderPath,
        effectDefinition.fragmentShaderPath,
        pipelineConfig);
}

void Faye::FullscreenEffectPass::invalidatePipelines(const std::string &compiledShader)
{
    if (effectDefinition.vertexShaderPath == compiledShader || effectDefinition.fragmentShaderPath == compiledShader)
    {
        LOG_INFO(
            Logger::getInstance(),
            "Invalidating pipeline for fullscreen effect with vertex shader '{}' and fragment shader '{}'",
            effectDefinition.vertexShaderPath,
            effectDefinition.fragmentShaderPath);
        createPipeline();
    }
}

void Faye::FullscreenEffectPass::render(
    FrameContext &frameContext,
    const std::unordered_map<std::string, std::vector<VkImageView>> &namedInputs,
    const PostProcessParameterBlock &parameters)
{
    updateDescriptorSet(static_cast<uint32_t>(frameContext.frameIndex), namedInputs);

    vk_pipeline->bind(frameContext.commandBuffer);

    vkCmdBindDescriptorSets(
        frameContext.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &descriptorSets.at(frameContext.frameIndex),
        0,
        nullptr);

    vkCmdPushConstants(
        frameContext.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PostProcessParameterBlock),
        &parameters);

    vkCmdDraw(frameContext.commandBuffer, 3, 1, 0, 0);
}

Faye::PostProcessChain::PostProcessChain(VulkanDevice &device, VulkanRenderer &renderer, VulkanDescriptorPool &pool)
    : vk_device(device), vk_renderer(renderer), descriptorPool(pool)
{
    recreatePresentPass();
    swapchainGeneration = vk_renderer.getSwapchainGeneration();
}

void Faye::PostProcessChain::syncResources()
{
    if (swapchainGeneration != vk_renderer.getSwapchainGeneration())
    {
        recreatePresentPass();
        swapchainGeneration = vk_renderer.getSwapchainGeneration();
    }
}

void Faye::PostProcessChain::renderEffects(FrameContext &frameContext, const PostProcessStackComponent *stack)
{
    if (!hasEnabledEffects(stack))
    {
        return;
    }

    std::vector<VkImageView> currentSource = vk_renderer.getSceneImageViews();
    uint32_t targetIndex = 0;

    for (const auto &effect : stack->effects)
    {
        if (!effect.enabled)
        {
            continue;
        }

        const auto *definition = findPostProcessEffectDefinition(effect.definitionId);
        if (definition == nullptr)
        {
            continue;
        }

        vk_renderer.beginPostProcessRenderPass(frameContext.commandBuffer, targetIndex);
        getOrCreateEffectPass(*definition).render(frameContext, makeNamedInputViews(currentSource), effect.parameters);
        vk_renderer.endPostProcessRenderPass(frameContext.commandBuffer);

        currentSource = vk_renderer.getPostProcessTargetImageViews(targetIndex);
        targetIndex = (targetIndex + 1) % VulkanRenderer::kPostProcessTargetCount;
    }
}

void Faye::PostProcessChain::renderComposite(FrameContext &frameContext, const PostProcessStackComponent *stack)
{
    const uint32_t finalTargetIndex = getFinalTargetIndex(stack);
    const std::vector<VkImageView> finalSourceViews = finalTargetIndex == VulkanRenderer::kPostProcessTargetCount
                                                          ? vk_renderer.getSceneImageViews()
                                                          : vk_renderer.getPostProcessTargetImageViews(finalTargetIndex);
    presentPass->render(frameContext, makeNamedInputViews(finalSourceViews), PostProcessParameterBlock{});
}

bool Faye::PostProcessChain::hasEnabledEffects(const PostProcessStackComponent *stack) const
{
    return getEnabledEffectCount(stack) > 0;
}

VkDescriptorSet Faye::PostProcessChain::getViewportDescriptorSet(const PostProcessStackComponent *stack) const
{
    const uint32_t finalTargetIndex = getFinalTargetIndex(stack);
    if (finalTargetIndex == VulkanRenderer::kPostProcessTargetCount)
    {
        return vk_renderer.getSceneViewportDescriptorSet();
    }

    return vk_renderer.getPostProcessViewportDescriptorSet(finalTargetIndex);
}

void Faye::PostProcessChain::invalidatePipelines(const std::string &compiledShader)
{
    presentPass->invalidatePipelines(compiledShader);
    for (auto &[_, effectPass] : effectPasses)
    {
        effectPass->invalidatePipelines(compiledShader);
    }
}

void Faye::PostProcessChain::recreatePresentPass()
{
    presentPass = std::make_unique<FullscreenEffectPass>(
        vk_device,
        vk_renderer.getSwapChainRenderPass(),
        descriptorPool,
        getPostProcessPresentEffectDefinition());
}

uint32_t Faye::PostProcessChain::getEnabledEffectCount(const PostProcessStackComponent *stack) const
{
    if (stack == nullptr || !stack->enabled)
    {
        return 0;
    }

    uint32_t count = 0;
    for (const auto &effect : stack->effects)
    {
        if (effect.enabled)
        {
            ++count;
        }
    }

    return count;
}

uint32_t Faye::PostProcessChain::getFinalTargetIndex(const PostProcessStackComponent *stack) const
{
    const uint32_t enabledEffectCount = getEnabledEffectCount(stack);
    if (enabledEffectCount == 0)
    {
        return VulkanRenderer::kPostProcessTargetCount;
    }

    return (enabledEffectCount - 1) % VulkanRenderer::kPostProcessTargetCount;
}

Faye::FullscreenEffectPass &Faye::PostProcessChain::getOrCreateEffectPass(const PostProcessEffectDefinition &definition)
{
    auto iterator = effectPasses.find(definition.id);
    if (iterator == effectPasses.end())
    {
        iterator = effectPasses.emplace(
                                   definition.id,
                                   std::make_unique<FullscreenEffectPass>(
                                       vk_device,
                                       vk_renderer.getPostProcessRenderPass(),
                                       descriptorPool,
                                       definition))
                       .first;
    }

    return *iterator->second;
}

std::unordered_map<std::string, std::vector<VkImageView>> Faye::PostProcessChain::makeNamedInputViews(
    const std::vector<VkImageView> &sourceColorViews) const
{
    return {
        {"sourceColor", sourceColorViews},
        {"sceneColor", vk_renderer.getSceneImageViews()},
        {"sceneMotion", vk_renderer.getSceneMotionImageViews()},
        {"sceneDepth", vk_renderer.getSceneDepthImageViews()},
        {"postProcessTarget0", vk_renderer.getPostProcessTargetImageViews(0)},
        {"postProcessTarget1", vk_renderer.getPostProcessTargetImageViews(1)},
    };
}