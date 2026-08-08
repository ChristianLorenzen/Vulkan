#pragma once

// GPU model: uploads CPU mesh data (Assets/Import/ModelMeshData) into vertex /
// index buffers and owns the node hierarchy + submeshes. The Assimp import and
// procedural primitive generation live headless in Assets/Import/ModelImporter,
// so this header no longer pulls in assimp.

#include "engine/Assets/Import/ModelMeshData.hpp"
#include "Core/Handles/MaterialHandle.hpp"
#include "Core/Handles/PrimitiveType.hpp"
#include "Renderer/Resources/Vertex.hpp"
#include "Renderer/Vulkan/VulkanBuffer.hpp"
#include "Renderer/Vulkan/vk_device.hpp"
#include "Renderer/Material/Material.hpp"

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

#include <cstdint>
#include <memory>
#include <string>

namespace Faye
{
    class Model
    {
    public:
        static constexpr uint32_t kInvalidNodeIndex = ~0u;

        struct MeshNode
        {
            std::string name;
            std::vector<uint32_t> submeshIndices;
            std::vector<uint32_t> childNodeIndices;
        };

        struct Submesh
        {
            uint32_t firstVertex = 0;
            uint32_t vertexCount = 0;
            uint32_t firstIndex = 0;
            uint32_t indexCount = 0;
            uint32_t importedMaterialIndex = 0;
            MaterialHandle materialHandle{};

            bool hasIndices() const { return indexCount > 0; }
        };

        struct Bounds
        {
            glm::vec3 min{0.0f};
            glm::vec3 max{0.0f};

            glm::vec3 center() const { return (min + max) * 0.5f; }
            glm::vec3 extents() const { return (max - min) * 0.5f; }
        };

        Model(VulkanDevice &device, const ModelMeshData &meshData);
        Model(VulkanDevice &device, const std::vector<Vertex> &vertices);
        ~Model();

        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;

        static std::unique_ptr<Model> createModelFromFile(VulkanDevice &device, const std::string &modelPath);
        static std::unique_ptr<Model> createPrimitive(VulkanDevice &device, PrimitiveType primitiveType, uint32_t subdivisions = 64);

        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);
        void drawSubmesh(VkCommandBuffer commandBuffer, const Submesh &submesh) const;
        void assignImportedMaterialHandles(const std::vector<MaterialHandle> &materialHandles);
        const Bounds &getLocalBounds() const { return localBounds; }
        const std::vector<MaterialData> &getImportedMaterials() const { return importedMaterials; }
        const std::vector<Submesh> &getSubmeshes() const { return submeshes; }
        VkDeviceAddress getVertexBufferAddress() const { return vertexBuffer->getDeviceAddress(); }
        const std::vector<MeshNode> &getMeshNodes() const { return meshNodes; }
        uint32_t getRootNodeIndex() const { return rootNodeIndex; }

    private:
        void calculateLocalBounds(const std::vector<Vertex> &vertices);
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffers(const std::vector<uint32_t> &indices);

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        VulkanDevice &vk_device;

        std::unique_ptr<VulkanBuffer> vertexBuffer;
        uint32_t vertexCount;
        std::unique_ptr<VulkanBuffer> indexBuffer;
        uint32_t indexCount = 0;
        bool hasIndexBuffer = false;

        std::vector<Submesh> submeshes;
        std::vector<MaterialData> importedMaterials;
        std::vector<MeshNode> meshNodes;
        uint32_t rootNodeIndex = kInvalidNodeIndex;

        Bounds localBounds{};
    };
} // namespace Faye
