#include "Renderer/Resources/Model.hpp"

#include "engine/Assets/Import/ModelImporter.hpp"
#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace Faye
{
    Model::Model(VulkanDevice &device, const ModelMeshData &meshData) : vk_device{device}
    {
        importedMaterials = meshData.materials;
        submeshes.clear();
        submeshes.reserve(meshData.meshes.size());

        std::vector<Vertex> meshVertices = std::vector<Vertex>{};
        uint32_t currentVertexOffset = 0;
        uint32_t currentIndexOffset = 0;
        for (const auto &mesh : meshData.meshes)
        {
            submeshes.push_back(Submesh{
                currentVertexOffset,
                static_cast<uint32_t>(mesh.vertices.size()),
                currentIndexOffset,
                static_cast<uint32_t>(mesh.indices.size()),
                mesh.materialIndex});

            meshVertices.insert(meshVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
            currentVertexOffset += static_cast<uint32_t>(mesh.vertices.size());
            currentIndexOffset += static_cast<uint32_t>(mesh.indices.size());
        }

        // Build mesh node hierarchy (submesh indices map 1:1 to meshData.meshes indices)
        meshNodes.reserve(meshData.nodes.size());
        for (const auto &meshNode : meshData.nodes)
        {
            MeshNode node;
            node.name = meshNode.name;
            node.submeshIndices = meshNode.meshDataIndices;
            node.childNodeIndices = meshNode.childNodeIndices;
            meshNodes.push_back(std::move(node));
        }
        rootNodeIndex = meshData.rootNodeIndex;
        std::vector<uint32_t> meshIndices = std::vector<uint32_t>{};
        uint32_t vertexOffset = 0;
        for (const auto &mesh : meshData.meshes)
        {
            for (const auto index : mesh.indices)
            {
                meshIndices.push_back(vertexOffset + index);
            }
            vertexOffset += static_cast<uint32_t>(mesh.vertices.size());
        }
        vertices = meshVertices;
        indices = meshIndices;
        calculateLocalBounds(meshVertices);
        createVertexBuffers(meshVertices);
        createIndexBuffers(meshIndices);
        LOG_INFO(Logger::get(), "Model created successfully {} {}.", vertexCount, indexCount);
    }

    Model::Model(VulkanDevice &device, const std::vector<Vertex> &vertices) : vk_device{device}
    {
        this->vertices = vertices;
        calculateLocalBounds(vertices);
        createVertexBuffers(vertices);
        LOG_INFO(Logger::get(), "Model created successfully {}.", vertexCount);
    }

    Model::~Model()
    {
    }

    std::unique_ptr<Model> Model::createModelFromFile(VulkanDevice &device, const std::string &modelPath)
    {
        return std::make_unique<Model>(device, Assets::importModelFromFile(modelPath));
    }

    std::unique_ptr<Model> Model::createPrimitive(VulkanDevice &device, PrimitiveType primitiveType, uint32_t subdivisions)
    {
        return std::make_unique<Model>(device, Assets::makePrimitiveMesh(primitiveType, subdivisions));
    }

    void Model::calculateLocalBounds(const std::vector<Vertex> &vertices)
    {
        if (vertices.empty())
        {
            localBounds = Bounds{};
            return;
        }

        glm::vec3 minBounds{std::numeric_limits<float>::max()};
        glm::vec3 maxBounds{std::numeric_limits<float>::lowest()};

        for (const auto &vertex : vertices)
        {
            minBounds = glm::min(minBounds, vertex.pos);
            maxBounds = glm::max(maxBounds, vertex.pos);
        }

        localBounds.min = minBounds;
        localBounds.max = maxBounds;
    }

    void Model::createVertexBuffers(const std::vector<Vertex> &vertices)
    {
        vertexCount = static_cast<uint32_t>(vertices.size());

        assert(vertexCount >= 3 && "Vertex count is too small");
        VkDeviceSize bufferSize = sizeof(Vertex) * vertexCount;
        uint32_t vertexSize = sizeof(Vertex);

        VulkanBuffer stagingBuffer{
            vk_device,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            BufferMemoryUsage::Staging};

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)vertices.data());

        vertexBuffer = std::make_unique<VulkanBuffer>(
            vk_device,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            BufferMemoryUsage::GpuOnly);

        vk_device.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
    }

    void Model::createIndexBuffers(const std::vector<uint32_t> &indices)
    {
        indexCount = static_cast<uint32_t>(indices.size());

        hasIndexBuffer = indexCount > 0;

        if (!hasIndexBuffer)
        {
            return;
        }

        assert(indexCount >= 3 && "Index count is too small");
        VkDeviceSize bufferSize = sizeof(uint32_t) * indexCount;
        uint32_t indexSize = sizeof(uint32_t);

        VulkanBuffer stagingBuffer{
            vk_device,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            BufferMemoryUsage::Staging};

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)indices.data());

        indexBuffer = std::make_unique<VulkanBuffer>(
            vk_device,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            BufferMemoryUsage::GpuOnly);
        vk_device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
    }

    void Model::draw(VkCommandBuffer commandBuffer)
    {
        if (!submeshes.empty())
        {
            for (const Submesh &submesh : submeshes)
            {
                drawSubmesh(commandBuffer, submesh);
            }
            return;
        }

        if (hasIndexBuffer)
        {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void Model::drawSubmesh(VkCommandBuffer commandBuffer, const Submesh &submesh) const
    {
        if (submesh.hasIndices())
        {
            vkCmdDrawIndexed(commandBuffer, submesh.indexCount, 1, submesh.firstIndex, 0, 0);
            return;
        }

        vkCmdDraw(commandBuffer, submesh.vertexCount, 1, submesh.firstVertex, 0);
    }

    void Model::assignImportedMaterialHandles(const std::vector<MaterialHandle> &materialHandles)
    {
        for (Submesh &submesh : submeshes)
        {
            if (submesh.importedMaterialIndex < materialHandles.size())
            {
                submesh.materialHandle = materialHandles[submesh.importedMaterialIndex];
            }
        }
    }

    void Model::bind(VkCommandBuffer commandBuffer)
    {
        if (hasIndexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }
} // namespace Faye
