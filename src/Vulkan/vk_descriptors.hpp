#pragma once
 
#include "vk_device.hpp"
 
// std
#include <memory>
#include <unordered_map>
#include <vector>
 
namespace Faye {
 
class VulkanDescriptorSetLayout {
 public:
  class Builder {
   public:
    Builder(VulkanDevice &vk_device) : vk_device{vk_device} {}
 
    Builder &addBinding(
        uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        uint32_t count = 1);
    std::unique_ptr<VulkanDescriptorSetLayout> build() const;
 
   private:
    VulkanDevice &vk_device;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
  };
 
  VulkanDescriptorSetLayout(
      VulkanDevice &vk_device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
  ~VulkanDescriptorSetLayout();
  VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout &) = delete;
  VulkanDescriptorSetLayout &operator=(const VulkanDescriptorSetLayout &) = delete;
 
  VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
 
 private:
  VulkanDevice &vk_device;
  VkDescriptorSetLayout descriptorSetLayout;
  std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
 
  friend class VulkanDescriptorWriter;
};
 
class VulkanDescriptorPool {
 public:
  class Builder {
   public:
    Builder(VulkanDevice &vk_device) : vk_device{vk_device} {}
 
    Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
    Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
    Builder &setMaxSets(uint32_t count);
    std::unique_ptr<VulkanDescriptorPool> build() const;
 
   private:
    VulkanDevice &vk_device;
    std::vector<VkDescriptorPoolSize> poolSizes{};
    uint32_t maxSets = 1000;
    VkDescriptorPoolCreateFlags poolFlags = 0;
  };
 
  VulkanDescriptorPool(
      VulkanDevice &vk_device,
      uint32_t maxSets,
      VkDescriptorPoolCreateFlags poolFlags,
      const std::vector<VkDescriptorPoolSize> &poolSizes);
  ~VulkanDescriptorPool();
  VulkanDescriptorPool(const VulkanDescriptorPool &) = delete;
  VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;
 
  bool allocateDescriptor(
      const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;
 
  void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;
 
  void resetPool();

  VkDescriptorPool getPool() { return descriptorPool; };
 
 private:
  VulkanDevice &vk_device;
  VkDescriptorPool descriptorPool;
 
  friend class VulkanDescriptorWriter;
};
 
class VulkanDescriptorWriter {
 public:
  VulkanDescriptorWriter(VulkanDescriptorSetLayout &setLayout, VulkanDescriptorPool &pool);
 
  VulkanDescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
  VulkanDescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);
 
  bool build(VkDescriptorSet &set);
  void overwrite(VkDescriptorSet &set);
 
 private:
  VulkanDescriptorSetLayout &setLayout;
  VulkanDescriptorPool &pool;
  std::vector<VkWriteDescriptorSet> writes;
};
 
}  // namespace lve