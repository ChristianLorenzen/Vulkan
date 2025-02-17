// #pragma once

// #include "../Vulkan/vk_device.cpp"

// #define GLM_FORCE_RADIANS
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE
// #include <glm/glm.hpp>
// #include <vector>

// namespace Faye {
//     class Model {
//         public:
//             Model(VulkanDevice &device, const std::vector<Vertex> &vertices) {}
//             ~Model() {}

//             Model(const Model &) = delete;
//             Model &operator=(const Model &) = delete;

//             void bind(VkCommandBuffer commandBuffer);
//             void draw(VkCommandBuffer commandBuffer);

//         private:
//             void createVertexBuffers(const std::vector<Vertex> &vertices);

//             VulkanDevice& vk_device;
//             VkBuffer vertexBuffer;
//             VkDeviceMemory vertexBufferMemory;
//             uint32_t vertexCount;
//     }
// }