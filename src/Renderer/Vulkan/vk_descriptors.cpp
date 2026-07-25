#include "vk_descriptors.hpp"
 
// std
#include <cassert>
#include <stdexcept>
 
namespace Faye {
 
// *************** Descriptor Set Layout Builder *********************
 
VulkanDescriptorSetLayout::Builder &VulkanDescriptorSetLayout::Builder::addBinding(
    uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    uint32_t count,
    VkDescriptorBindingFlags bindingFlags) {
  assert(bindings.count(binding) == 0 && "Binding already in use");
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = descriptorType;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stageFlags;
  bindings[binding] = layoutBinding;
  bindingFlagsMap[binding] = bindingFlags;
  return *this;
}

VulkanDescriptorSetLayout::Builder &VulkanDescriptorSetLayout::Builder::setFlags(
    VkDescriptorSetLayoutCreateFlags flags) {
  layoutFlags = flags;
  return *this;
}

std::unique_ptr<VulkanDescriptorSetLayout> VulkanDescriptorSetLayout::Builder::build() const {
  return std::make_unique<VulkanDescriptorSetLayout>(vk_device, bindings, bindingFlagsMap, layoutFlags);
}
 
// *************** Descriptor Set Layout *********************
 
VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(
    VulkanDevice &vk_device,
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
    std::unordered_map<uint32_t, VkDescriptorBindingFlags> bindingFlagsMap,
    VkDescriptorSetLayoutCreateFlags flags)
    : vk_device{vk_device}, bindings{bindings} {
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
  for (auto kv : bindings) {
    setLayoutBindings.push_back(kv.second);
  }

  std::vector<VkDescriptorBindingFlags> bindingFlagsList;
  bindingFlagsList.reserve(setLayoutBindings.size());
  for (const auto &kv : bindings) {
    auto it = bindingFlagsMap.find(kv.first);
    bindingFlagsList.push_back(it != bindingFlagsMap.end() ? it->second : 0);
  }

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
  bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlagsList.size());
  bindingFlagsInfo.pBindingFlags = bindingFlagsList.data();

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
  descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
  descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();
  descriptorSetLayoutInfo.flags = flags;
  descriptorSetLayoutInfo.pNext = &bindingFlagsInfo;

  if (vkCreateDescriptorSetLayout(
          vk_device.getDevice(),
          &descriptorSetLayoutInfo,
          nullptr,
          &descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}
 
VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(vk_device.getDevice(), descriptorSetLayout, nullptr);
}
 
// *************** Descriptor Pool Builder *********************
 
VulkanDescriptorPool::Builder &VulkanDescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) {
  poolSizes.push_back({descriptorType, count});
  return *this;
}
 
VulkanDescriptorPool::Builder &VulkanDescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) {
  poolFlags = flags;
  return *this;
}
VulkanDescriptorPool::Builder &VulkanDescriptorPool::Builder::setMaxSets(uint32_t count) {
  maxSets = count;
  return *this;
}
 
std::unique_ptr<VulkanDescriptorPool> VulkanDescriptorPool::Builder::build() const {
  return std::make_unique<VulkanDescriptorPool>(vk_device, maxSets, poolFlags, poolSizes);
}
 
// *************** Descriptor Pool *********************
 
VulkanDescriptorPool::VulkanDescriptorPool(
    VulkanDevice &vk_device,
    uint32_t maxSets,
    VkDescriptorPoolCreateFlags poolFlags,
    const std::vector<VkDescriptorPoolSize> &poolSizes)
    : vk_device{vk_device} {
  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  descriptorPoolInfo.pPoolSizes = poolSizes.data();
  descriptorPoolInfo.maxSets = maxSets;
  descriptorPoolInfo.flags = poolFlags;
 
  if (vkCreateDescriptorPool(vk_device.getDevice(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}
 
VulkanDescriptorPool::~VulkanDescriptorPool() {
  vkDestroyDescriptorPool(vk_device.getDevice(), descriptorPool, nullptr);
}
 
bool VulkanDescriptorPool::allocateDescriptor(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.pSetLayouts = &descriptorSetLayout;
  allocInfo.descriptorSetCount = 1;
 
  // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
  // a new pool whenever an old pool fills up. But this is beyond our current scope
  if (vkAllocateDescriptorSets(vk_device.getDevice(), &allocInfo, &descriptor) != VK_SUCCESS) {
    return false;
  }
  return true;
}
 
void VulkanDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
  vkFreeDescriptorSets(
      vk_device.getDevice(),
      descriptorPool,
      static_cast<uint32_t>(descriptors.size()),
      descriptors.data());
}
 
void VulkanDescriptorPool::resetPool() {
  vkResetDescriptorPool(vk_device.getDevice(), descriptorPool, 0);
}
 
// *************** Descriptor Writer *********************
 
VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanDescriptorSetLayout &setLayout, VulkanDescriptorPool &pool)
    : setLayout{setLayout}, pool{&pool} {}

VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanDescriptorSetLayout &setLayout)
    : setLayout{setLayout}, pool{nullptr} {}
 
VulkanDescriptorWriter &VulkanDescriptorWriter::writeBuffer(
    uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
 
  auto &bindingDescription = setLayout.bindings[binding];
 
  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");
 
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pBufferInfo = bufferInfo;
  write.descriptorCount = 1;
 
  writes.push_back(write);
  return *this;
}
 
VulkanDescriptorWriter &VulkanDescriptorWriter::writeImage(
    uint32_t binding, VkDescriptorImageInfo *imageInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
 
  auto &bindingDescription = setLayout.bindings[binding];
 
  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");
 
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pImageInfo = imageInfo;
  write.descriptorCount = 1;
 
  writes.push_back(write);
  return *this;
}
 
bool VulkanDescriptorWriter::build(VkDescriptorSet &set) {
  assert(pool != nullptr && "build() requires a pool — use VulkanDescriptorWriter(layout, pool)");
  bool success = pool->allocateDescriptor(setLayout.getDescriptorSetLayout(), set);
  if (!success) {
    return false;
  }
  overwrite(set);
  return true;
}

void VulkanDescriptorWriter::overwrite(VkDescriptorSet &set) {
  assert(pool != nullptr && "overwrite() requires a pool — use VulkanDescriptorWriter(layout, pool)");
  for (auto &write : writes) {
    write.dstSet = set;
  }
  vkUpdateDescriptorSets(pool->vk_device.getDevice(), writes.size(), writes.data(), 0, nullptr);
}

void VulkanDescriptorWriter::pushDescriptors(
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    uint32_t setIndex,
    VkPipelineBindPoint bindPoint
  )
{
  // VK_KHR_push_descriptor is not core in Vulkan 1.3 — load the function pointer at
  // call time. VulkanDescriptorWriter is a friend of VulkanDescriptorSetLayout, so
  // we can reach the device through setLayout.vk_device.
  // Cache the function pointer — vkGetDeviceProcAddr is a string lookup and
  // should not be called per draw call.
  static PFN_vkCmdPushDescriptorSetKHR pfn = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
      vkGetDeviceProcAddr(setLayout.vk_device.getDevice(), "vkCmdPushDescriptorSetKHR"));
  assert(pfn && "VK_KHR_push_descriptor extension not available or not enabled");

  for (auto &write : writes) {
    write.dstSet = VK_NULL_HANDLE;
  }
  pfn(commandBuffer,
      bindPoint,
      pipelineLayout,
      setIndex,
      static_cast<uint32_t>(writes.size()),
      writes.data());
}
 
}  // namespace Faye