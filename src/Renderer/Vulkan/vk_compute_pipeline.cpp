#include "vk_compute_pipeline.hpp"
#include "Core/IO/FileSystem.hpp"
#include "Renderer/Resources/Vertex.hpp"
#include "vk_descriptors.hpp"
#include "vk_pipeline.hpp"

#include "quill/LogMacros.h"

#include <cstring>

using namespace Faye;

VkDescriptorType toVkDescriptorType(ShaderBinding::Kind kind)
{
    switch (kind)
    {
    case ShaderBinding::Kind::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ShaderBinding::Kind::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderBinding::Kind::CombinedImageSampler:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case ShaderBinding::Kind::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    default:
        throw std::runtime_error("Unhandled descriptor type");
    }
}

VulkanComputePipeline::VulkanComputePipeline(VulkanDevice &device, const std::string &computeFilepath) : device(device)
{
    
    LOG_INFO(Logger::get(), "Creating Compute Pipeline...");
    
    const std::vector<char> computeShaderBytes = FileSystem::readFile(computeFilepath);

    std::vector<uint32_t> spirvWords(computeShaderBytes.size() / sizeof(uint32_t));
    std::memcpy(spirvWords.data(), computeShaderBytes.data(), computeShaderBytes.size());

    reflection = reflectShader(spirvWords);
    assert(reflection.stage == ShaderStage::Compute && "Reflection stage is not compute");

    auto builder = VulkanDescriptorSetLayout::Builder(device)
    .setFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    for (const ShaderBinding &binding : reflection.bindings) {
        builder.addBinding(binding.binding, toVkDescriptorType(binding.kind), VK_SHADER_STAGE_COMPUTE_BIT, binding.count);
    }
    setLayout = builder.build();

    VkPushConstantRange pushRange{};
    bool hasPushConstants = !reflection.pushConstants.empty();
    if (hasPushConstants) {
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = reflection.pushConstants[0].offset;
        pushRange.size = reflection.pushConstants[0].sizeBytes;
    }

    VkDescriptorSetLayout rawSetLayout = setLayout->getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rawSetLayout;
    layoutInfo.pushConstantRangeCount = hasPushConstants ? 1 : 0;
    layoutInfo.pPushConstantRanges = hasPushConstants ? &pushRange : nullptr;

    if (vkCreatePipelineLayout(device.getDevice(), &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create compute pipeline layout");

    VkShaderModule shaderModule = createShaderModule(device, computeShaderBytes);

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = reflection.entryPoint.empty() ? "main" : reflection.entryPoint.c_str();

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pipelineLayout;

    VkResult result = vkCreateComputePipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, nullptr, &computePipeline);
    vkDestroyShaderModule(device.getDevice(), shaderModule, nullptr);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create compute pipeline");
}

VulkanComputePipeline::~VulkanComputePipeline()
{
    if (computePipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device.getDevice(), computePipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }
}

void VulkanComputePipeline::dispatch(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ,
const std::vector<VulkanComputeBinding> &bindings, const void* pushData, uint32_t pushDataSize) 
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

    VulkanDescriptorWriter writer(*setLayout);
    for (const VulkanComputeBinding &b : bindings) {
        if (b.image != nullptr) writer.writeImage(b.binding, b.image);
        else if (b.buffer != nullptr) writer.writeBuffer(b.binding, b.buffer);
    }
    writer.pushDescriptors(cmd, pipelineLayout, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    if (pushData != nullptr && pushDataSize > 0) {
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushDataSize, pushData);
    }

    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
}