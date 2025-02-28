#pragma once

#include "../Vulkan/vk_device.hpp"
#include "Vertex.hpp"
#include "../Vulkan/VulkanBuffer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>

#include <memory>

namespace Faye {
    struct Builder {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};

        void loadModel(const std::string &modelPath);
    };

    class Model {
        public:
            Model(VulkanDevice &device, const Builder &builder);
            Model(VulkanDevice &device, const std::vector<Vertex> &vertices);
            ~Model();

            Model(const Model &) = delete;
            Model &operator=(const Model &) = delete;

            static std::unique_ptr<Model> createModelFromFile(VulkanDevice &device, const std::string &modelPath);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);

        private:
            void createVertexBuffers(const std::vector<Vertex> &vertices);
            void createIndexBuffers(const std::vector<uint32_t> &indices);


            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            
            VulkanDevice& vk_device;
            
            std::unique_ptr<VulkanBuffer> vertexBuffer;
            uint32_t vertexCount;
            
            std::unique_ptr<VulkanBuffer> indexBuffer;
            uint32_t indexCount;

            bool hasIndexBuffer = false;
    };
}