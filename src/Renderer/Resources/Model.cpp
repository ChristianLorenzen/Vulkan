#include "Model.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "include/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <stdexcept>
#include <unordered_map>

#include <limits>
#include <vector>
#include <string>

namespace Faye
{
    namespace
    {
        Vertex makeVertex(const glm::vec3 &position, const glm::vec3 &color, const glm::vec3 &normal, const glm::vec2 &uv)
        {
            Vertex vertex{};
            vertex.pos = position;
            vertex.color = color;
            vertex.normal = normal;
            vertex.uv = uv;
            return vertex;
        }

        Builder makeCubeBuilder()
        {
            Builder builder{};

            const glm::vec3 positions[] = {
                {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
                {0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
                {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f},
                {0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f},
                {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
                {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f},
            };

            const glm::vec3 normals[] = {
                {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
            };

            const glm::vec2 uv[] = {
                {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
            };

            builder.vertices.reserve(24);
            for (int face = 0; face < 6; ++face)
            {
                for (int corner = 0; corner < 4; ++corner)
                {
                    builder.vertices.push_back(makeVertex(positions[face * 4 + corner], {1.0f, 1.0f, 1.0f}, normals[face], uv[corner]));
                }
            }

            builder.indices = {
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7,
                8, 9, 10, 8, 10, 11,
                12, 13, 14, 12, 14, 15,
                16, 17, 18, 16, 18, 19,
                20, 21, 22, 20, 22, 23,
            };

            return builder;
        }

        Builder makePlaneBuilder()
        {
            Builder builder{};
            builder.vertices = {
                makeVertex({-0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}),
                makeVertex({0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}),
                makeVertex({0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}),
                makeVertex({-0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}),
            };
            builder.indices = {0, 1, 2, 0, 2, 3};
            return builder;
        }

        Builder makeSphereBuilder(uint32_t sectors = 24, uint32_t stacks = 16)
        {
            Builder builder{};
            const float radius = 0.5f;

            for (uint32_t stack = 0; stack <= stacks; ++stack)
            {
                const float stackAngle = glm::pi<float>() * static_cast<float>(stack) / static_cast<float>(stacks);
                const float y = std::cos(stackAngle);
                const float ringRadius = std::sin(stackAngle);

                for (uint32_t sector = 0; sector <= sectors; ++sector)
                {
                    const float sectorAngle = glm::two_pi<float>() * static_cast<float>(sector) / static_cast<float>(sectors);
                    const float x = ringRadius * std::cos(sectorAngle);
                    const float z = ringRadius * std::sin(sectorAngle);
                    const glm::vec3 normal{x, y, z};

                    builder.vertices.push_back(makeVertex(
                        radius * normal,
                        {1.0f, 1.0f, 1.0f},
                        glm::normalize(normal),
                        {static_cast<float>(sector) / static_cast<float>(sectors), static_cast<float>(stack) / static_cast<float>(stacks)}));
                }
            }

            const uint32_t ringVertexCount = sectors + 1;
            for (uint32_t stack = 0; stack < stacks; ++stack)
            {
                for (uint32_t sector = 0; sector < sectors; ++sector)
                {
                    const uint32_t current = stack * ringVertexCount + sector;
                    const uint32_t next = current + ringVertexCount;

                    builder.indices.push_back(current);
                    builder.indices.push_back(next);
                    builder.indices.push_back(current + 1);

                    builder.indices.push_back(current + 1);
                    builder.indices.push_back(next);
                    builder.indices.push_back(next + 1);
                }
            }

            return builder;
        }

        Builder makeCapsuleBuilder(uint32_t sectors = 24, uint32_t hemisphereStacks = 8, uint32_t cylinderStacks = 6)
        {
            struct CapsuleRing
            {
                float y = 0.0f;
                float radial = 0.0f;
                float normalRadial = 0.0f;
                float normalY = 0.0f;
            };

            Builder builder{};
            const float radius = 0.5f;
            const float halfCylinderHeight = 0.5f;
            std::vector<CapsuleRing> rings;
            rings.reserve(hemisphereStacks * 2 + cylinderStacks + 1);

            for (uint32_t stack = 0; stack <= hemisphereStacks; ++stack)
            {
                const float angle = glm::half_pi<float>() * static_cast<float>(stack) / static_cast<float>(hemisphereStacks);
                rings.push_back(CapsuleRing{
                    halfCylinderHeight + std::cos(angle) * radius,
                    std::sin(angle) * radius,
                    std::sin(angle),
                    std::cos(angle)});
            }

            for (uint32_t stack = 1; stack < cylinderStacks; ++stack)
            {
                const float t = static_cast<float>(stack) / static_cast<float>(cylinderStacks);
                rings.push_back(CapsuleRing{
                    halfCylinderHeight - 2.0f * halfCylinderHeight * t,
                    radius,
                    1.0f,
                    0.0f});
            }

            for (uint32_t stack = 0; stack <= hemisphereStacks; ++stack)
            {
                const float angle = glm::half_pi<float>() * static_cast<float>(stack) / static_cast<float>(hemisphereStacks);
                rings.push_back(CapsuleRing{
                    -halfCylinderHeight - std::sin(angle) * radius,
                    std::cos(angle) * radius,
                    std::cos(angle),
                    -std::sin(angle)});
            }

            for (size_t ringIndex = 0; ringIndex < rings.size(); ++ringIndex)
            {
                const float v = rings.size() > 1 ? static_cast<float>(ringIndex) / static_cast<float>(rings.size() - 1) : 0.0f;
                for (uint32_t sector = 0; sector <= sectors; ++sector)
                {
                    const float sectorAngle = glm::two_pi<float>() * static_cast<float>(sector) / static_cast<float>(sectors);
                    const float cosTheta = std::cos(sectorAngle);
                    const float sinTheta = std::sin(sectorAngle);
                    const CapsuleRing &ring = rings[ringIndex];

                    const glm::vec3 position{
                        ring.radial * cosTheta,
                        ring.y,
                        ring.radial * sinTheta};

                    const glm::vec3 normal = glm::normalize(glm::vec3{
                        ring.normalRadial * cosTheta,
                        ring.normalY,
                        ring.normalRadial * sinTheta});

                    builder.vertices.push_back(makeVertex(
                        position,
                        {1.0f, 1.0f, 1.0f},
                        normal,
                        {static_cast<float>(sector) / static_cast<float>(sectors), v}));
                }
            }

            const uint32_t ringVertexCount = sectors + 1;
            for (uint32_t ring = 0; ring + 1 < rings.size(); ++ring)
            {
                for (uint32_t sector = 0; sector < sectors; ++sector)
                {
                    const uint32_t current = ring * ringVertexCount + sector;
                    const uint32_t next = current + ringVertexCount;

                    builder.indices.push_back(current);
                    builder.indices.push_back(next);
                    builder.indices.push_back(current + 1);

                    builder.indices.push_back(current + 1);
                    builder.indices.push_back(next);
                    builder.indices.push_back(next + 1);
                }
            }

            return builder;
        }
    }

    Model::Model(VulkanDevice &device, const Builder &builder) : vk_device{device}
    {
        vertices = builder.vertices;
        indices = builder.indices;
        calculateLocalBounds(builder.vertices);
        createVertexBuffers(builder.vertices);
        createIndexBuffers(builder.indices);
        LOG_INFO(Logger::getInstance(), "Model created successfully {} {}.", vertexCount, indexCount);
    }

    Model::Model(VulkanDevice &device, const std::vector<Vertex> &vertices) : vk_device{device}
    {
        this->vertices = vertices;
        calculateLocalBounds(vertices);
        createVertexBuffers(vertices);
        LOG_INFO(Logger::getInstance(), "Model created successfully {}.", vertexCount);
    }

    Model::~Model()
    {
    }

    std::unique_ptr<Model> Model::createModelFromFile(VulkanDevice &device, const std::string &modelPath)
    {
        Builder builder{};
        builder.loadModel(modelPath);
        return std::make_unique<Model>(device, builder);
    }

    std::unique_ptr<Model> Model::createPrimitive(VulkanDevice &device, PrimitiveType primitiveType)
    {
        return std::make_unique<Model>(device, Builder::makePrimitive(primitiveType));
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
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)vertices.data());

        vertexBuffer = std::make_unique<VulkanBuffer>(
            vk_device,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)indices.data());

        indexBuffer = std::make_unique<VulkanBuffer>(
            vk_device,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vk_device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
    }

    void Model::draw(VkCommandBuffer commandBuffer)
    {
        if (hasIndexBuffer)
        {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }
    void Model::bind(VkCommandBuffer commandBuffer)
    {
        VkBuffer buffers[] = {vertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        if (hasIndexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void Builder::loadModel(const std::string &modelPath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
        {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

        for (const auto &shape : shapes)
        {
            for (const auto &index : shape.mesh.indices)
            {
                Vertex vertex = {};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

                if (attrib.colors.size() >= static_cast<size_t>(3 * index.vertex_index + 3))
                {
                    vertex.color = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2]};
                }
                else
                {
                    vertex.color = {1.f, 1.f, 1.f};
                }

                if (index.normal_index >= 0)
                {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]};
                }

                if (index.texcoord_index >= 0)
                {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
                }

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    Builder Builder::makePrimitive(PrimitiveType primitiveType)
    {
        switch (primitiveType)
        {
        case PrimitiveType::Cube:
            return makeCubeBuilder();
        case PrimitiveType::Sphere:
            return makeSphereBuilder();
        case PrimitiveType::Plane:
            return makePlaneBuilder();
        case PrimitiveType::Capsule:
            return makeCapsuleBuilder();
        case PrimitiveType::Count:
            break;
        }

        throw std::runtime_error("Unsupported primitive type requested from Builder::makePrimitive");
    }
}