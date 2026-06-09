#pragma once

#include "Renderer/Resources/PrimitiveType.hpp"
#include "Renderer/Resources/Vertex.hpp"
#include "Renderer/Vulkan/VulkanBuffer.hpp"
#include "Renderer/Vulkan/vk_device.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

#include <memory>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Faye
{
    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        uint32_t materialIndex = 0;
    };

    struct Builder
    {
        struct NodeData
        {
            std::string name;
            std::vector<uint32_t> meshDataIndices;
            std::vector<uint32_t> childNodeIndices;
        };

        std::vector<Mesh> meshes;
        std::vector<MaterialData> materials;
        std::string directory;
        std::unordered_map<std::string, Texture> loadedTextures;
        std::vector<NodeData> nodes;
        uint32_t rootNodeIndex = ~0u;

        void loadModel(const std::string &modelPath);
        uint32_t processNode(aiNode *node, const aiScene *scene, const glm::mat4 &parentTransform = glm::mat4(1.0f));
        Mesh processMesh(aiMesh *mesh, const aiScene *scene, const glm::mat4 &nodeTransform);
        MaterialData processMaterial(aiMaterial *material, const aiScene *scene);
        Texture loadTexture(const std::string &texturePath, const aiScene *scene, TextureType textureType);
        Texture processTexture(aiTextureType type, const aiScene *scene);
        static Builder makePrimitive(PrimitiveType primitiveType, uint32_t subdivisions = 64);
    };

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

        Model(VulkanDevice &device, const Builder &builder);
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
        uint32_t indexCount;

        bool hasIndexBuffer = false;
        Bounds localBounds{};
        std::vector<MaterialData> importedMaterials;
        std::vector<Submesh> submeshes;
        std::vector<MeshNode> meshNodes;
        uint32_t rootNodeIndex = kInvalidNodeIndex;
    };
}