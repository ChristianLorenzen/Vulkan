#include "Model.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "include/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

#include <assimp/GltfMaterial.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>

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
        glm::mat4 aiMatrixToGlm(const aiMatrix4x4 &matrix)
        {
            glm::mat4 result{1.0f};
            result[0][0] = matrix.a1;
            result[1][0] = matrix.a2;
            result[2][0] = matrix.a3;
            result[3][0] = matrix.a4;

            result[0][1] = matrix.b1;
            result[1][1] = matrix.b2;
            result[2][1] = matrix.b3;
            result[3][1] = matrix.b4;

            result[0][2] = matrix.c1;
            result[1][2] = matrix.c2;
            result[2][2] = matrix.c3;
            result[3][2] = matrix.c4;

            result[0][3] = matrix.d1;
            result[1][3] = matrix.d2;
            result[2][3] = matrix.d3;
            result[3][3] = matrix.d4;
            return result;
        }

        Vertex makeVertex(const glm::vec3 &position, const glm::vec3 &color, const glm::vec3 &normal, const glm::vec2 &uv, const glm::vec4 &tangent = {1.0f, 0.0f, 0.0f, 1.0f})
        {
            Vertex vertex{};
            vertex.pos = position;
            vertex.color = color;
            vertex.normal = normal;
            vertex.uv = uv;
            vertex.tangent = tangent;
            return vertex;
        }

        Builder makeCubeBuilder()
        {
            Builder builder{};

            const glm::vec3 positions[] = {
                {-0.5f, -0.5f, 0.5f},
                {0.5f, -0.5f, 0.5f},
                {0.5f, 0.5f, 0.5f},
                {-0.5f, 0.5f, 0.5f},
                {0.5f, -0.5f, -0.5f},
                {-0.5f, -0.5f, -0.5f},
                {-0.5f, 0.5f, -0.5f},
                {0.5f, 0.5f, -0.5f},
                {-0.5f, -0.5f, -0.5f},
                {-0.5f, -0.5f, 0.5f},
                {-0.5f, 0.5f, 0.5f},
                {-0.5f, 0.5f, -0.5f},
                {0.5f, -0.5f, 0.5f},
                {0.5f, -0.5f, -0.5f},
                {0.5f, 0.5f, -0.5f},
                {0.5f, 0.5f, 0.5f},
                {-0.5f, 0.5f, 0.5f},
                {0.5f, 0.5f, 0.5f},
                {0.5f, 0.5f, -0.5f},
                {-0.5f, 0.5f, -0.5f},
                {-0.5f, -0.5f, -0.5f},
                {0.5f, -0.5f, -0.5f},
                {0.5f, -0.5f, 0.5f},
                {-0.5f, -0.5f, 0.5f},
            };

            const glm::vec3 normals[] = {
                {0.0f, 0.0f, 1.0f},
                {0.0f, 0.0f, -1.0f},
                {-1.0f, 0.0f, 0.0f},
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.0f, -1.0f, 0.0f},
            };

            const glm::vec2 uv[] = {
                {0.0f, 0.0f},
                {1.0f, 0.0f},
                {1.0f, 1.0f},
                {0.0f, 1.0f},
            };
            Mesh mesh{};
            mesh.vertices.reserve(24);

            for (int face = 0; face < 6; ++face)
            {
                for (int corner = 0; corner < 4; ++corner)
                {
                    mesh.vertices.push_back(makeVertex(positions[face * 4 + corner], {1.0f, 1.0f, 1.0f}, normals[face], uv[corner]));
                }
            }

            mesh.indices = {
                0,
                1,
                2,
                0,
                2,
                3,
                4,
                5,
                6,
                4,
                6,
                7,
                8,
                9,
                10,
                8,
                10,
                11,
                12,
                13,
                14,
                12,
                14,
                15,
                16,
                17,
                18,
                16,
                18,
                19,
                20,
                21,
                22,
                20,
                22,
                23,
            };
            builder.meshes.push_back(mesh);
            return builder;
        }

        Builder makePlaneBuilder()
        {
            Builder builder{};
            Mesh mesh{};
            mesh.vertices = {
                makeVertex({-0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}),
                makeVertex({0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}),
                makeVertex({0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}),
                makeVertex({-0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}),
            };
            mesh.indices = {0, 1, 2, 0, 2, 3};
            builder.meshes.push_back(mesh);
            return builder;
        }

        Builder makeSphereBuilder(uint32_t sectors = 24, uint32_t stacks = 16)
        {
            Builder builder{};
            const float radius = 0.5f;
            Mesh mesh{};
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

                    mesh.vertices.push_back(makeVertex(
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

                    mesh.indices.push_back(current);
                    mesh.indices.push_back(next);
                    mesh.indices.push_back(current + 1);

                    mesh.indices.push_back(current + 1);
                    mesh.indices.push_back(next);
                    mesh.indices.push_back(next + 1);
                }
            }

            builder.meshes.push_back(mesh);
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
            Mesh mesh{};
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

                    mesh.vertices.push_back(makeVertex(
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

                    mesh.indices.push_back(current);
                    mesh.indices.push_back(next);
                    mesh.indices.push_back(current + 1);

                    mesh.indices.push_back(current + 1);
                    mesh.indices.push_back(next);
                    mesh.indices.push_back(next + 1);
                }
            }

            builder.meshes.push_back(mesh);
            return builder;
        }
    }

    Model::Model(VulkanDevice &device, const Builder &builder) : vk_device{device}
    {
        importedMaterials = builder.materials;
        submeshes.clear();
        submeshes.reserve(builder.meshes.size());

        std::vector<Vertex> meshVertices = std::vector<Vertex>{};
        uint32_t currentVertexOffset = 0;
        uint32_t currentIndexOffset = 0;
        for (const auto &mesh : builder.meshes)
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
        std::vector<uint32_t> meshIndices = std::vector<uint32_t>{};
        uint32_t vertexOffset = 0;
        for (const auto &mesh : builder.meshes)
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
        std::string directory = modelPath.substr(0, modelPath.find_last_of('/'));
        LOG_INFO(Logger::getInstance(), "Loading model from file at directory {}.", directory);
        Builder builder{};
        builder.directory = directory;
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
        // Creating importer class instance
        Assimp::Importer importer;

        const aiScene *scene = importer.ReadFile(modelPath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);

        if (scene == nullptr)
        {
            LOG_INFO(Logger::getInstance(), "Failed to load model at path {}. Assimp error: {}", modelPath, importer.GetErrorString());
            throw std::runtime_error("Failed to load model at path " + modelPath);
        }

        materials.clear();
        loadedTextures.clear();
        materials.reserve(scene->mNumMaterials);
        for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++)
        {
            materials.push_back(processMaterial(scene->mMaterials[materialIndex], scene));
        }

        processNode(scene->mRootNode, scene);
    }

    void Builder::processNode(aiNode *node, const aiScene *scene, const glm::mat4 &parentTransform)
    {
        const glm::mat4 nodeTransform = parentTransform * aiMatrixToGlm(node->mTransformation);

        for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; meshIndex++)
        {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[meshIndex]];
            // Process the mesh and add it to the builder
            meshes.push_back(processMesh(mesh, scene, nodeTransform));
        }

        for (unsigned int childIndex = 0; childIndex < node->mNumChildren; childIndex++)
        {
            processNode(node->mChildren[childIndex], scene, nodeTransform);
        }
    }

    Mesh Builder::processMesh(aiMesh *mesh, const aiScene * /*scene*/, const glm::mat4 &nodeTransform)
    {
        Mesh meshData{};
        meshData.materialIndex = mesh->mMaterialIndex;
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));
        // Process the mesh and add it to the builder
        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++)
        {
            Vertex vertex{};

            const glm::vec3 localPosition = {
                mesh->mVertices[vertexIndex].x,
                mesh->mVertices[vertexIndex].y,
                mesh->mVertices[vertexIndex].z};
            vertex.pos = glm::vec3(nodeTransform * glm::vec4(localPosition, 1.0f));

            if (mesh->HasNormals())
            {
                const glm::vec3 localNormal = {
                    mesh->mNormals[vertexIndex].x,
                    mesh->mNormals[vertexIndex].y,
                    mesh->mNormals[vertexIndex].z};
                vertex.normal = glm::normalize(normalMatrix * localNormal);
            }

            if (mesh->HasTextureCoords(0))
            {
                vertex.uv = {
                    mesh->mTextureCoords[0][vertexIndex].x,
                    1.0f - mesh->mTextureCoords[0][vertexIndex].y};
            }

            if (mesh->HasTangentsAndBitangents())
            {
                const glm::vec3 tangent = glm::normalize(normalMatrix * glm::vec3{
                    mesh->mTangents[vertexIndex].x,
                    mesh->mTangents[vertexIndex].y,
                    mesh->mTangents[vertexIndex].z});
                const glm::vec3 bitangent = glm::normalize(normalMatrix * glm::vec3{
                    mesh->mBitangents[vertexIndex].x,
                    mesh->mBitangents[vertexIndex].y,
                    mesh->mBitangents[vertexIndex].z});
                const float handedness = glm::dot(glm::cross(vertex.normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
                vertex.tangent = glm::vec4(glm::normalize(tangent), handedness);
            }

            vertex.color = {1.0f, 1.0f, 1.0f};

            meshData.vertices.push_back(vertex);
        }

        for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++)
        {
            const aiFace &face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3 && "Non-triangulated face found in model");

            meshData.indices.push_back(face.mIndices[0]);
            meshData.indices.push_back(face.mIndices[1]);
            meshData.indices.push_back(face.mIndices[2]);
        }

        return meshData;
    }

    MaterialData Builder::processMaterial(aiMaterial *material, const aiScene *scene)
    {
        MaterialData result{};

        aiString name;
        material->Get(AI_MATKEY_NAME, name);
        result.name = name.C_Str();

        aiColor3D color(1.0f, 1.0f, 1.0f);
        aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
        bool hasBaseColor = false;
        float shininess = 0.0f;
        float opacity = 1.0f;
        float metallicFactor = result.metallicFactor;
        float roughnessFactor = result.roughnessFactor;
        float normalScale = result.normalScale;
        float specularStrength = result.specularStrength;
        float reflectivity = result.reflectivity;
        float emissiveIntensity = result.emissiveIntensity;
        float alphaCutoff = result.alphaCutoff;
        aiString alphaMode;

        if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS)
        {
            hasBaseColor = true;
            result.baseColorFactor = {baseColor.r, baseColor.g, baseColor.b, baseColor.a};
            result.color = {baseColor.r, baseColor.g, baseColor.b};
            result.diffuse = result.color;
        }
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
        {
            if (!hasBaseColor)
            {
                result.baseColorFactor = {color.r, color.g, color.b, result.baseColorFactor.a};
            }
            result.color = {color.r, color.g, color.b};
            result.diffuse = result.color;
        }
        if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
        {
            result.ambient = {color.r, color.g, color.b};
        }
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
        {
            result.specular = {color.r, color.g, color.b};
        }
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
        {
            result.emissive = {color.r, color.g, color.b};
        }
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
        {
            result.shininess = shininess;
        }
        if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
        {
            result.opacity = opacity;
            result.baseColorFactor.a = opacity;
        }
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) == AI_SUCCESS)
        {
            result.metallicFactor = metallicFactor;
        }
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) == AI_SUCCESS)
        {
            result.roughnessFactor = roughnessFactor;
        }
        if (material->Get(AI_MATKEY_BUMPSCALING, normalScale) == AI_SUCCESS)
        {
            result.normalScale = normalScale;
        }
        if (material->Get(AI_MATKEY_SHININESS_STRENGTH, specularStrength) == AI_SUCCESS)
        {
            result.specularStrength = specularStrength;
        }
        if (material->Get(AI_MATKEY_REFLECTIVITY, reflectivity) == AI_SUCCESS)
        {
            result.reflectivity = reflectivity;
        }
        if (material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveIntensity) == AI_SUCCESS)
        {
            result.emissiveIntensity = emissiveIntensity;
        }
        if (material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == AI_SUCCESS)
        {
            result.alphaCutoff = alphaCutoff;
        }
        if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS)
        {
            const std::string importedAlphaMode = alphaMode.C_Str();
            if (importedAlphaMode == "MASK")
            {
                result.alphaMode = MaterialAlphaMode::Mask;
            }
            else if (importedAlphaMode == "BLEND")
            {
                LOG_WARNING(
                    Logger::getInstance(),
                    "Material '{}' requests alpha mode BLEND, which is not supported in the current v1 pipeline. Falling back to opaque rendering.",
                    result.name);
            }
        }
        {
            int twoSided = 0;
            if (material->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
            {
                result.doubleSided = (twoSided != 0);
            }
        }

        for (int textureType = aiTextureType_NONE; textureType <= aiTextureType_UNKNOWN; textureType++)
        {
            for (unsigned int textureIndex = 0; textureIndex < material->GetTextureCount(static_cast<aiTextureType>(textureType)); textureIndex++)
            {
                aiString texturePath;
                const std::optional<TextureType> resolvedTextureType = Texture::textureTypeFromAssimp(static_cast<aiTextureType>(textureType));

                if (!resolvedTextureType.has_value())
                {
                    LOG_INFO(
                        Logger::getInstance(),
                        "Skipping unsupported Assimp texture type {} for material {}.",
                        textureType,
                        result.name);
                    continue;
                }

                material->GetTexture(static_cast<aiTextureType>(textureType), textureIndex, &texturePath);

                // Load the texture and add it to the material
                LOG_INFO(Logger::getInstance(), "Found texture at path {} for material {}.", texturePath.C_Str(), result.name);
                std::filesystem::path fullTexturePath = std::filesystem::path(directory) / texturePath.C_Str();
                LOG_INFO(Logger::getInstance(), "Full texture path resolved to {}.", fullTexturePath.string());
                result.textures.push_back(loadTexture(
                    fullTexturePath.string(),
                    scene,
                    *resolvedTextureType));
            }
        }

        return result;
    }

    Texture Builder::loadTexture(const std::string &texturePath, const aiScene *scene, TextureType textureType)
    {
        const std::string cacheKey = texturePath + "#" + std::to_string(static_cast<uint32_t>(textureType));
        if (const auto iterator = loadedTextures.find(cacheKey); iterator != loadedTextures.end())
        {
            LOG_INFO(Logger::getInstance(), "Reusing cached texture at path {}.", texturePath);
            return iterator->second;
        }

        const aiTexture *aiTex = scene->GetEmbeddedTexture(texturePath.c_str());
        if (aiTex != nullptr)
        {
            // Handle embedded texture
            LOG_INFO(Logger::getInstance(), "Loading embedded texture at path {}.", texturePath);
        }
        else
        {
            // Handle external texture file
            LOG_INFO(Logger::getInstance(), "Loading external texture at path {}.", texturePath);
            // Use stb_image or another library to load the texture from the file system
            int width, height, channels;
            unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (data)
            {
                LOG_INFO(Logger::getInstance(), "Successfully loaded texture at path {} with dimensions {}x{} and {} channels.", texturePath, width, height, channels);
                std::vector<unsigned char> textureData(data, data + (width * height * 4)); // forcing 4 channels.
                stbi_image_free(data);
                Texture texture = Texture::create(std::move(textureData), width, height, channels, texturePath, textureType);
                loadedTextures.emplace(cacheKey, texture);
                return texture;
            }
            else
            {
                LOG_ERROR(Logger::getInstance(), "Failed to load texture at path {}. stbi error: {}", texturePath, stbi_failure_reason());
            }
        }
        return Texture{};
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