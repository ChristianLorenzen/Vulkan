#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <filesystem>
#include <stdio.h>
#include <functional>

#include "Vulkan.hpp"
#include "vk_render_system.hpp"
#include "Core/Path/Paths.hpp"

#include "quill/LogMacros.h"

#include <chrono>

using namespace Faye;

struct SimplePushConstantData
{
    glm::mat4 modelMatrix{1.0f};                 // offset   0 — 64 bytes
    glm::mat4 priorModelMatrix{1.0f};            // offset  64 — 64 bytes
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f}; // offset 128 — 16 bytes
    VkDeviceAddress vertexBufferAddress{0};      // offset 144 —  8 bytes (vertex shader only)
    uint32_t albedoSlot{0};                      // offset 152
    uint32_t normalSlot{0};                      // offset 156
    uint32_t metallicSlot{0};                    // offset 160
    uint32_t roughnessSlot{0};                   // offset 164
    uint32_t aoSlot{0};                          // offset 168
}; // total: 172 bytes

namespace
{
    constexpr std::string_view kDefaultVertexShader = "shader.vert";
    constexpr std::string_view kDefaultFragmentShader = "shader.frag";
    const std::filesystem::path kCompiledShaderDirectory = Paths::compiledShaders();

    // A pipeline only supports PATCH_LIST if it was actually built with tessellation
    // control/evaluation stages (see SimpleRenderSystem::createPipeline). Branching on
    // MaterialDomain::Water alone would be wrong until Phase 4 gives water real
    // tesc/tese shaders -- until then this always resolves to TRIANGLE_LIST.
    VkPrimitiveTopology topologyForMaterial(const Faye::MaterialPipelineConfig &pipelineConfig)
    {
        const bool hasTessellation = !pipelineConfig.tessControlShaderPath.empty() &&
                                      !pipelineConfig.tessEvalShaderPath.empty();
        return hasTessellation ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

size_t Faye::SimpleRenderSystem::MaterialPipelineKeyHasher::operator()(const MaterialPipelineKey &key) const
{
    const size_t vertexHash = std::hash<std::string>{}(key.vertexShaderPath);
    const size_t fragmentHash = std::hash<std::string>{}(key.fragmentShaderPath);
    const size_t tescHash = std::hash<std::string>{}(key.tessControlShaderPath);
    const size_t teseHash = std::hash<std::string>{}(key.tessEvalShaderPath);
    size_t combined = vertexHash ^ (fragmentHash + 0x9e3779b9 + (vertexHash << 6) + (vertexHash >> 2));
    combined ^= tescHash + 0x9e3779b9 + (combined << 6) + (combined >> 2);
    combined ^= teseHash + 0x9e3779b9 + (combined << 6) + (combined >> 2);
    combined ^= key.alphaBlend ? size_t{0x9e3779b97f4a7c15ULL} : size_t{0};
    combined ^= static_cast<size_t>(key.domain) * 0x9e3779b97f4a7c15ULL;
    return combined;
}

Faye::SimpleRenderSystem::SimpleRenderSystem(VulkanDevice &device, VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat, MaterialCache &materialCache, VulkanDescriptorSetLayout &globalSetLayout, VkDescriptorSetLayout materialSetLayout, VkDescriptorSetLayout bindlessSetLayout, VkDescriptorSetLayout waterFieldSetLayout)
    : vk_device(device), sceneColorFormat(colorFormat), sceneMotionFormat(motionFormat), sceneDepthFormat(depthFormat), materialCache(materialCache), globalDescriptorSetLayout(globalSetLayout)
{
    LOG_INFO(Logger::get(), "Creating Vulkan Pipeline Layout...");
    createPipelineLayout(globalSetLayout.getDescriptorSetLayout(), materialSetLayout, bindlessSetLayout, waterFieldSetLayout);
}

Faye::SimpleRenderSystem::~SimpleRenderSystem()
{
    if (depthPrepassPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(vk_device.getDevice(), depthPrepassPipelineLayout, nullptr);
    }
    vkDestroyPipelineLayout(vk_device.getDevice(), pipelineLayout, nullptr);
}

// ------------------------------------- Conversion Functions ------------------------------------- //
void Faye::SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout materialSetLayout, VkDescriptorSetLayout bindlessSetLayout, VkDescriptorSetLayout waterFieldSetLayout)
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, materialSetLayout, bindlessSetLayout, waterFieldSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    LOG_INFO(Logger::get(), "Created pipelinelayoutinfo struct...");

    if (vkCreatePipelineLayout(vk_device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

std::string Faye::SimpleRenderSystem::resolveCompiledShaderPath(const std::string &shaderPath)
{
    std::filesystem::path resolvedPath = shaderPath.empty()
                                             ? std::filesystem::path(kDefaultVertexShader)
                                             : std::filesystem::path(shaderPath);

    if (!resolvedPath.has_parent_path())
    {
        resolvedPath = kCompiledShaderDirectory / resolvedPath;
    }
    else if (resolvedPath.extension() != ".spv")
    {
        resolvedPath = kCompiledShaderDirectory / resolvedPath.filename();
    }
    else if (resolvedPath.string().find("/compiled/") == std::string::npos)
    {
        resolvedPath = kCompiledShaderDirectory / resolvedPath.filename();
    }

    if (resolvedPath.extension() != ".spv")
    {
        resolvedPath += ".spv";
    }

    return resolvedPath.generic_string();
}

Faye::SimpleRenderSystem::MaterialPipelineKey Faye::SimpleRenderSystem::makePipelineKey(const MaterialState &materialState)
{
    const std::string_view vertexShader = !materialState.pipelineConfig.vertexShaderPath.empty()
                                              ? std::string_view(materialState.pipelineConfig.vertexShaderPath)
                                              : kDefaultVertexShader;
    const std::string_view fragmentShader = !materialState.pipelineConfig.fragmentShaderPath.empty()
                                                ? std::string_view(materialState.pipelineConfig.fragmentShaderPath)
                                                : kDefaultFragmentShader;

    const bool hasTessellation = !materialState.pipelineConfig.tessControlShaderPath.empty() &&
                                  !materialState.pipelineConfig.tessEvalShaderPath.empty();

    return MaterialPipelineKey{
        resolveCompiledShaderPath(std::string(vertexShader)),
        resolveCompiledShaderPath(std::string(fragmentShader)),
        materialState.pipelineConfig.enableAlphaBlending,
        materialState.pipelineConfig.domain,
        hasTessellation ? resolveCompiledShaderPath(materialState.pipelineConfig.tessControlShaderPath) : std::string(),
        hasTessellation ? resolveCompiledShaderPath(materialState.pipelineConfig.tessEvalShaderPath) : std::string()};
}

std::unique_ptr<VulkanPipeline> Faye::SimpleRenderSystem::createPipeline(const MaterialPipelineKey &key) const
{
    assert(pipelineLayout != nullptr && "Pipeline layout is null");

    PipelineConfigInfo pipelineConfig{};
    VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.colorBlendAttachments.resize(2, pipelineConfig.colorBlendAttachments.front());

    if (key.alphaBlend)
    {
        // Translucent material (e.g. water): blend colour attachment 0 only.
        // Attachment 1 carries motion vectors and must never be blended.
        VkPipelineColorBlendAttachmentState &att0 = pipelineConfig.colorBlendAttachments[0];
        att0.blendEnable = VK_TRUE;
        att0.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att0.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att0.colorBlendOp = VK_BLEND_OP_ADD;
        att0.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att0.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att0.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    pipelineConfig.colorAttachmentFormats = {sceneColorFormat, sceneMotionFormat};
    pipelineConfig.depthAttachmentFormat = sceneDepthFormat;
    pipelineConfig.pipelineLayout = pipelineLayout;

    const bool hasTessellation = !key.tessControlShaderPath.empty() && !key.tessEvalShaderPath.empty();
    if (hasTessellation)
    {
        pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        pipelineConfig.tessellationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        pipelineConfig.tessellationInfo.patchControlPoints = 4;
    }

    return std::make_unique<VulkanPipeline>(
        vk_device,
        key.vertexShaderPath,
        key.fragmentShaderPath,
        pipelineConfig,
        key.tessControlShaderPath,
        key.tessEvalShaderPath);
}

VulkanPipeline &Faye::SimpleRenderSystem::getOrCreatePipeline(const MaterialState &materialState)
{
    MaterialPipelineKey key = makePipelineKey(materialState);
    auto iterator = pipelineCache.find(key);

    if (iterator == pipelineCache.end())
    {
        LOG_INFO(
            Logger::get(),
            "Creating material pipeline for vertex shader '{}' and fragment shader '{}'",
            key.vertexShaderPath,
            key.fragmentShaderPath);
        iterator = pipelineCache.emplace(key, createPipeline(key)).first;
    }

    return *iterator->second;
}

void Faye::SimpleRenderSystem::invalidatePipelines(const std::string &compiledShader)
{
    std::vector<Faye::SimpleRenderSystem::MaterialPipelineKey> keysToInvalidate = {};

    LOG_INFO(Logger::get(), "Checking pipelines for invalidation against compiled shader '{}'", compiledShader);

    for (const auto &[key, pipeline] : pipelineCache)
    {
        LOG_INFO(
            Logger::get(),
            "Checking pipeline with vertex shader '{}' and fragment shader '{}' for invalidation",
            key.vertexShaderPath,
            key.fragmentShaderPath);
        if (key.vertexShaderPath == compiledShader || key.fragmentShaderPath == compiledShader)
        {
            keysToInvalidate.push_back(key);
            LOG_INFO(
                Logger::get(),
                "Marked pipeline with vertex shader '{}' and fragment shader '{}' for invalidation",
                key.vertexShaderPath,
                key.fragmentShaderPath);
        }
    }

    for (const auto &key : keysToInvalidate)
    {
        LOG_INFO(
            Logger::get(),
            "Invalidating pipeline with vertex shader '{}' and fragment shader '{}'",
            key.vertexShaderPath,
            key.fragmentShaderPath);
        pipelineCache.erase(key);
    }
}

void Faye::SimpleRenderSystem::renderScene(FrameContext &frameContext, const RenderSceneSnapshot &renderScene)
{
    // Push global UBO + prepass depth + lighting UBO directly into the command
    // buffer (no pool allocation). Lit fragment shaders read the lighting UBO at
    // set 0, binding 2.
    auto bufferInfo = frameContext.globalBuffer->descriptorInfo();
    auto lightingInfo = frameContext.lightingBuffer->descriptorInfo();
    VulkanDescriptorWriter(globalDescriptorSetLayout)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &frameContext.prepassDepthInfo)
        .writeBuffer(2, &lightingInfo)
        .pushDescriptors(frameContext.commandBuffer, pipelineLayout, 0);

    for (const auto &renderable : renderScene.renderables)
    {
        if (renderable.model == nullptr)
        {
            continue;
        }
        renderable.model->bind(frameContext.commandBuffer);

        const auto &submeshes = renderable.model->getSubmeshes();
        if (submeshes.empty())
        {
            if (renderable.material == nullptr || !renderable.materialHandle.isValid())
            {
                continue;
            }

            const MaterialState &materialState = materialCache.getOrCreateState(renderable.materialHandle, *renderable.material);
            VulkanPipeline &pipeline = getOrCreatePipeline(materialState);
            pipeline.bind(frameContext.commandBuffer);

            vkCmdSetCullMode(frameContext.commandBuffer, VK_CULL_MODE_NONE);
            vkCmdSetFrontFace(frameContext.commandBuffer, VK_FRONT_FACE_CLOCKWISE);
            vkCmdSetPrimitiveTopology(frameContext.commandBuffer, topologyForMaterial(materialState.pipelineConfig));
            vkCmdSetDepthTestEnable(frameContext.commandBuffer, VK_TRUE);
            vkCmdSetDepthWriteEnable(frameContext.commandBuffer,
                                     materialState.pipelineConfig.enableAlphaBlending ? VK_FALSE : VK_TRUE);
            vkCmdSetDepthCompareOp(frameContext.commandBuffer, VK_COMPARE_OP_LESS);

            // Bind material params UBO (set 1).
            vkCmdBindDescriptorSets(
                frameContext.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                1,
                1,
                &materialState.descriptorSet,
                0,
                nullptr);

            SimplePushConstantData push{};
            push.modelMatrix = renderable.modelMatrix;
            push.priorModelMatrix = renderable.priorModelMatrix;
            push.baseColor = glm::vec4(renderable.material->getColor(), 1.0f);
            push.vertexBufferAddress = renderable.model->getVertexBufferAddress();
            push.albedoSlot = materialState.albedoSlot;
            push.normalSlot = materialState.normalSlot;
            push.metallicSlot = materialState.metallicSlot;
            push.roughnessSlot = materialState.roughnessSlot;
            push.aoSlot = materialState.aoSlot;

            vkCmdPushConstants(
                frameContext.commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(SimplePushConstantData),
                &push);

            renderable.model->draw(frameContext.commandBuffer);
            continue;
        }

        for (const auto &submesh : submeshes)
        {
            MaterialHandle resolvedHandle = renderable.materialHandle;
            const Material *resolvedMaterial = renderable.material;

            if (submesh.materialHandle.isValid() && renderable.materialRegistry != nullptr)
            {
                if (const Material *importedMaterial = renderable.materialRegistry->getMaterial(submesh.materialHandle))
                {
                    resolvedHandle = submesh.materialHandle;
                    resolvedMaterial = importedMaterial;
                }
            }

            if (resolvedMaterial == nullptr || !resolvedHandle.isValid())
            {
                continue;
            }

            const MaterialState &materialState = materialCache.getOrCreateState(resolvedHandle, *resolvedMaterial);
            VulkanPipeline &pipeline = getOrCreatePipeline(materialState);
            pipeline.bind(frameContext.commandBuffer);

            vkCmdSetCullMode(frameContext.commandBuffer, VK_CULL_MODE_NONE);
            vkCmdSetFrontFace(frameContext.commandBuffer, VK_FRONT_FACE_CLOCKWISE);
            vkCmdSetPrimitiveTopology(frameContext.commandBuffer, topologyForMaterial(materialState.pipelineConfig));
            vkCmdSetDepthTestEnable(frameContext.commandBuffer, VK_TRUE);
            vkCmdSetDepthWriteEnable(frameContext.commandBuffer,
                                     materialState.pipelineConfig.enableAlphaBlending ? VK_FALSE : VK_TRUE);
            vkCmdSetDepthCompareOp(frameContext.commandBuffer, VK_COMPARE_OP_LESS);

            // Bind material params UBO (set 1).
            vkCmdBindDescriptorSets(
                frameContext.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                1,
                1,
                &materialState.descriptorSet,
                0,
                nullptr);

            SimplePushConstantData push{};
            push.modelMatrix = renderable.modelMatrix;
            push.priorModelMatrix = renderable.priorModelMatrix;
            push.baseColor = glm::vec4(resolvedMaterial->getColor(), 1.0f);
            push.vertexBufferAddress = renderable.model->getVertexBufferAddress();
            push.albedoSlot = materialState.albedoSlot;
            push.normalSlot = materialState.normalSlot;
            push.metallicSlot = materialState.metallicSlot;
            push.roughnessSlot = materialState.roughnessSlot;
            push.aoSlot = materialState.aoSlot;

            vkCmdPushConstants(
                frameContext.commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(SimplePushConstantData),
                &push);

            renderable.model->drawSubmesh(frameContext.commandBuffer, submesh);
        }
    }
}

void Faye::SimpleRenderSystem::prepareDepthPrepassPipeline(
    VkFormat depthFormat,
    VkDescriptorSetLayout globalSetLayout)
{
    if (depthPrepassPipelineLayout == VK_NULL_HANDLE)
    {
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcRange.offset = 0;
        pcRange.size = sizeof(SimplePushConstantData);

        std::vector<VkDescriptorSetLayout> layouts{globalSetLayout};
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        layoutInfo.pSetLayouts = layouts.data();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pcRange;

        if (vkCreatePipelineLayout(vk_device.getDevice(), &layoutInfo, nullptr, &depthPrepassPipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create depth prepass pipeline layout");
        }
    }

    PipelineConfigInfo config{};
    VulkanPipeline::defaultPipelineConfigInfo(config);
    config.colorBlendAttachments.clear(); // depth-only: no colour outputs
    config.colorBlendingInfo.attachmentCount = 0;
    config.colorBlendingInfo.pAttachments = nullptr;
    config.depthAttachmentFormat = depthFormat;
    config.pipelineLayout = depthPrepassPipelineLayout;

    depthPrepassPipeline = std::make_unique<VulkanPipeline>(
        vk_device,
        resolveCompiledShaderPath("depth_prepass.vert"),
        resolveCompiledShaderPath("depth_prepass.frag"),
        config);
}

void Faye::SimpleRenderSystem::renderDepthPrepass(
    FrameContext &frameContext, const RenderSceneSnapshot &renderScene)
{
    if (!depthPrepassPipeline)
    {
        return;
    }

    depthPrepassPipeline->bind(frameContext.commandBuffer);

    vkCmdSetCullMode(frameContext.commandBuffer, VK_CULL_MODE_NONE);
    vkCmdSetFrontFace(frameContext.commandBuffer, VK_FRONT_FACE_CLOCKWISE);
    vkCmdSetPrimitiveTopology(frameContext.commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    vkCmdSetDepthTestEnable(frameContext.commandBuffer, VK_TRUE);
    vkCmdSetDepthWriteEnable(frameContext.commandBuffer, VK_TRUE);
    vkCmdSetDepthCompareOp(frameContext.commandBuffer, VK_COMPARE_OP_LESS);

    // Push global UBO + prepass depth into the depth prepass pipeline.
    auto bufferInfo = frameContext.globalBuffer->descriptorInfo();
    VulkanDescriptorWriter(globalDescriptorSetLayout)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &frameContext.prepassDepthInfo)
        .pushDescriptors(frameContext.commandBuffer, depthPrepassPipelineLayout, 0);

    for (const auto &renderable : renderScene.renderables)
    {
        if (renderable.model == nullptr)
        {
            continue;
        }

        // Skip non-opaque materials (e.g. water) — they are translucent/animated and
        // must NOT occlude other objects in the depth prepass.
        if (renderable.material != nullptr && renderable.material->getPipelineConfig().domain != MaterialDomain::Opaque)
        {
            continue;
        }

        SimplePushConstantData push{};
        push.modelMatrix = renderable.modelMatrix;
        push.priorModelMatrix = renderable.priorModelMatrix;
        push.baseColor = glm::vec4(1.0f);
        push.vertexBufferAddress = renderable.model->getVertexBufferAddress();

        vkCmdPushConstants(
            frameContext.commandBuffer,
            depthPrepassPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);

        renderable.model->bind(frameContext.commandBuffer);

        const auto &submeshes = renderable.model->getSubmeshes();
        if (submeshes.empty())
        {
            renderable.model->draw(frameContext.commandBuffer);
        }
        else
        {
            for (const auto &submesh : submeshes)
            {
                renderable.model->drawSubmesh(frameContext.commandBuffer, submesh);
            }
        }
    }
}
