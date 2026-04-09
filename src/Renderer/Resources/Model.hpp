#pragma once

#include "Renderer/Resources/PrimitiveType.hpp"
#include "Renderer/Resources/Vertex.hpp"
#include "Renderer/Vulkan/VulkanBuffer.hpp"
#include "Renderer/Vulkan/vk_device.hpp"
#include "Renderer/Material/Material.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
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
    };

    struct Builder
    {
        std::vector<Mesh> meshes;
        std::vector<MaterialData> materials;
        std::string directory;

        void loadModel(const std::string &modelPath);
        void processNode(aiNode *node, const aiScene *scene);
        Mesh processMesh(aiMesh *mesh, const aiScene *scene);
        MaterialData processMaterial(aiMaterial *material, const aiScene *scene);
        Texture loadTexture(const std::string &texturePath, const aiScene *scene);
        Texture processTexture(aiTextureType type, const aiScene *scene);
        static Builder makePrimitive(PrimitiveType primitiveType);
    };

    class Model
    {
    public:
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
        static std::unique_ptr<Model> createPrimitive(VulkanDevice &device, PrimitiveType primitiveType);

        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);
        const Bounds &getLocalBounds() const { return localBounds; }
        const std::vector<MaterialData> &getImportedMaterials() const { return importedMaterials; }

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
    };
}