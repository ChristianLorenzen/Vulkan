#include "engine/Assets/Import/ModelImporter.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <filesystem>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

#include <limits>
#include <string>
#include <vector>

namespace Faye::Assets
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

        // Assimp returns whatever the exporter baked in, which for Windows-authored
        // FBX/OBJ is routinely a backslash path. std::filesystem on POSIX treats '\'
        // as an ordinary filename character, not a separator, so an un-normalised
        // reference collapses into an invalid filename.
        std::string normalizeSeparators(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }

        // The embedded path is only a hint: exporters routinely bake absolute or
        // machine-specific paths (the stylized-tropical pack points into the FBX
        // Converter's own install directory). So try the reference as given, then
        // fall back to the basename beside the model and in nearby `textures/`
        // folders -- packs commonly ship `fbx/`, `obj/` and `textures/` as siblings.
        //
        // Returns an empty path when nothing matches; the caller warns and continues.
        std::filesystem::path resolveTexturePath(const std::string &rawPath, const std::string &directory)
        {
            namespace fs = std::filesystem;

            const fs::path normalized{normalizeSeparators(rawPath)};
            const fs::path modelDir{directory};

            std::vector<fs::path> candidates;
            candidates.push_back((modelDir / normalized).lexically_normal());
            if (normalized.is_absolute())
            {
                candidates.push_back(normalized);
            }

            const fs::path basename = normalized.filename();
            if (!basename.empty())
            {
                candidates.push_back(modelDir / basename);
                for (const fs::path &root : {modelDir, modelDir / "..", modelDir / ".." / ".."})
                {
                    candidates.push_back((root / "textures" / basename).lexically_normal());
                    candidates.push_back((root / "Textures" / basename).lexically_normal());
                }
            }

            std::error_code ec;
            for (const fs::path &candidate : candidates)
            {
                if (fs::is_regular_file(candidate, ec))
                {
                    return candidate;
                }
            }
            return {};
        }

        // Per-import state shared across the recursive node/material processing.
        // Kept private here so no Assimp types leak into the public header.
        class Importer
        {
        public:
            explicit Importer(ModelMeshData &data, std::string directory)
                : data{data}, directory{std::move(directory)} {}

            void loadModel(const std::string &modelPath)
            {
                Assimp::Importer importer;

                const aiScene *scene = importer.ReadFile(modelPath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);

                if (scene == nullptr)
                {
                    LOG_ERROR(Logger::get(), "Failed to load model at path {}. Assimp error: {}", modelPath, importer.GetErrorString());
                    throw std::runtime_error("Failed to load model at path " + modelPath +
                                             ": " + importer.GetErrorString());
                }

                data.materials.clear();
                loadedTextures.clear();
                data.materials.reserve(scene->mNumMaterials);
                for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++)
                {
                    data.materials.push_back(processMaterial(scene->mMaterials[materialIndex], scene));
                }

                data.rootNodeIndex = processNode(scene->mRootNode, scene);
            }

        private:
            uint32_t processNode(aiNode *node, const aiScene *scene, const glm::mat4 &parentTransform = glm::mat4(1.0f))
            {
                const glm::mat4 nodeTransform = parentTransform * aiMatrixToGlm(node->mTransformation);

                NodeData nodeData;
                nodeData.name = node->mName.C_Str();

                for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; meshIndex++)
                {
                    aiMesh *mesh = scene->mMeshes[node->mMeshes[meshIndex]];
                    uint32_t meshDataIdx = static_cast<uint32_t>(data.meshes.size());
                    data.meshes.push_back(processMesh(mesh, scene, nodeTransform));
                    nodeData.meshDataIndices.push_back(meshDataIdx);
                }

                uint32_t nodeIndex = static_cast<uint32_t>(data.nodes.size());
                data.nodes.push_back(std::move(nodeData));

                for (unsigned int childIndex = 0; childIndex < node->mNumChildren; childIndex++)
                {
                    uint32_t childNodeIndex = processNode(node->mChildren[childIndex], scene, nodeTransform);
                    data.nodes[nodeIndex].childNodeIndices.push_back(childNodeIndex);
                }

                return nodeIndex;
            }

            Mesh processMesh(aiMesh *mesh, const aiScene * /*scene*/, const glm::mat4 &nodeTransform)
            {
                Mesh meshData{};
                meshData.materialIndex = mesh->mMaterialIndex;
                const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));
                // Process the mesh and add it to the importer's data
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

            MaterialData processMaterial(aiMaterial *material, const aiScene *scene)
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
                            Logger::get(),
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
                                Logger::get(),
                                "Skipping unsupported Assimp texture type {} for material {}.",
                                textureType,
                                result.name);
                            continue;
                        }

                        material->GetTexture(static_cast<aiTextureType>(textureType), textureIndex, &texturePath);

                        LOG_INFO(Logger::get(), "Found texture reference '{}' for material {}.", texturePath.C_Str(), result.name);

                        // check for embedded texture before trying to resolve in filesystem
                        if (scene->GetEmbeddedTexture(texturePath.C_Str()) != nullptr)
                        {
                            result.textures.push_back(loadTexture(texturePath.C_Str(), scene, *resolvedTextureType));
                            continue;
                        }

                        const std::filesystem::path fullTexturePath = resolveTexturePath(texturePath.C_Str(), directory);
                        if (fullTexturePath.empty())
                        {
                            LOG_WARNING(
                                Logger::get(),
                                "Could not resolve texture '{}' for material '{}' relative to '{}'. Continuing without it.",
                                texturePath.C_Str(),
                                result.name,
                                directory);
                            continue;
                        }

                        LOG_INFO(Logger::get(), "Resolved texture to {}.", fullTexturePath.string());
                        result.textures.push_back(loadTexture(
                            fullTexturePath.string(),
                            scene,
                            *resolvedTextureType));
                    }
                }

                return result;
            }

            Texture loadTexture(const std::string &texturePath, const aiScene *scene, TextureType textureType)
            {
                const std::string cacheKey = texturePath + "#" + std::to_string(static_cast<uint32_t>(textureType));
                if (const auto iterator = loadedTextures.find(cacheKey); iterator != loadedTextures.end())
                {
                    LOG_INFO(Logger::get(), "Reusing cached texture at path {}.", texturePath);
                    return iterator->second;
                }

                const aiTexture *aiTex = scene->GetEmbeddedTexture(texturePath.c_str());
                if (aiTex != nullptr)
                {
                    // Handle embedded texture
                    LOG_INFO(Logger::get(), "Loading embedded texture at path {}.", texturePath);
                }
                else
                {
                    // Handle external texture file
                    LOG_INFO(Logger::get(), "Loading external texture at path {}.", texturePath);
                    // Use stb_image or another library to load the texture from the file system
                    int width, height, channels;
                    unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
                    if (data)
                    {
                        LOG_INFO(Logger::get(), "Successfully loaded texture at path {} with dimensions {}x{} and {} channels.", texturePath, width, height, channels);
                        std::vector<unsigned char> textureData(data, data + (width * height * 4)); // forcing 4 channels.
                        stbi_image_free(data);
                        Texture texture = Texture::create(std::move(textureData), width, height, channels, texturePath, textureType);
                        loadedTextures.emplace(cacheKey, texture);
                        return texture;
                    }
                    else
                    {
                        LOG_ERROR(Logger::get(), "Failed to load texture at path {}. stbi error: {}", texturePath, stbi_failure_reason());
                    }
                }
                return Texture{};
            }

            ModelMeshData &data;
            std::string directory;
            std::unordered_map<std::string, Texture> loadedTextures;
        };

        ModelMeshData makeCubeMeshData()
        {
            ModelMeshData data{};

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
            data.meshes.push_back(mesh);
            return data;
        }

        ModelMeshData makePlaneMeshData()
        {
            ModelMeshData data{};
            Mesh mesh{};
            mesh.vertices = {
                makeVertex({-0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}),
                makeVertex({0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}),
                makeVertex({0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}),
                makeVertex({-0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}),
            };
            mesh.indices = {0, 1, 2, 0, 2, 3};
            data.meshes.push_back(mesh);
            return data;
        }

        // 64x64 subdivided plane for Gerstner wave displacement.
        // Spans [-0.5, 0.5] in XZ at Y=0. UV tiles once across the full mesh.
        // Tangent is +X with handedness +1, consistent with water.vert analytics.
        ModelMeshData makeWaterPlaneMeshData(uint32_t divisions = 64)
        {
            ModelMeshData data{};
            Mesh mesh{};

            const float half = 0.5f;
            const float step = 1.0f / static_cast<float>(divisions);
            const uint32_t verts = divisions + 1;

            mesh.vertices.reserve(verts * verts);
            for (uint32_t row = 0; row < verts; ++row)
            {
                for (uint32_t col = 0; col < verts; ++col)
                {
                    const float x = -half + static_cast<float>(col) * step;
                    const float z = -half + static_cast<float>(row) * step;
                    const float u = static_cast<float>(col) * step;
                    const float v = static_cast<float>(row) * step;
                    mesh.vertices.push_back(makeVertex(
                        {x, 0.0f, z},
                        {1.0f, 1.0f, 1.0f},
                        {0.0f, 1.0f, 0.0f},
                        {u, v},
                        {1.0f, 0.0f, 0.0f, 1.0f}));
                }
            }

            mesh.indices.reserve(divisions * divisions * 6);
            for (uint32_t row = 0; row < divisions; ++row)
            {
                for (uint32_t col = 0; col < divisions; ++col)
                {
                    const uint32_t tl = row * verts + col;
                    const uint32_t tr = tl + 1;
                    const uint32_t bl = tl + verts;
                    const uint32_t br = bl + 1;

                    mesh.indices.push_back(tl);
                    mesh.indices.push_back(bl);
                    mesh.indices.push_back(tr);

                    mesh.indices.push_back(tr);
                    mesh.indices.push_back(bl);
                    mesh.indices.push_back(br);
                }
            }

            data.meshes.push_back(mesh);
            return data;
        }

        ModelMeshData makeSphereMeshData(uint32_t sectors = 24, uint32_t stacks = 16)
        {
            ModelMeshData data{};
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

            data.meshes.push_back(mesh);
            return data;
        }

        ModelMeshData makeCapsuleMeshData(uint32_t sectors = 24, uint32_t hemisphereStacks = 8, uint32_t cylinderStacks = 6)
        {
            struct CapsuleRing
            {
                float y = 0.0f;
                float radial = 0.0f;
                float normalRadial = 0.0f;
                float normalY = 0.0f;
            };

            ModelMeshData data{};
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

            data.meshes.push_back(mesh);
            return data;
        }
    } // namespace

    ModelMeshData importModelFromFile(const std::string &modelPath)
    {
        const std::string directory = modelPath.substr(0, modelPath.find_last_of('/'));
        LOG_INFO(Logger::get(), "Loading model from file at directory {}.", directory);

        ModelMeshData data;
        data.directory = directory;
        Importer importer{data, directory};
        importer.loadModel(modelPath);
        return data;
    }

    ModelMeshData makePrimitiveMesh(PrimitiveType primitiveType, uint32_t subdivisions)
    {
        switch (primitiveType)
        {
        case PrimitiveType::Cube:
            return makeCubeMeshData();
        case PrimitiveType::Sphere:
            return makeSphereMeshData();
        case PrimitiveType::Plane:
            return makePlaneMeshData();
        case PrimitiveType::Capsule:
            return makeCapsuleMeshData();
        case PrimitiveType::WaterPlane:
            return makeWaterPlaneMeshData(subdivisions);
        case PrimitiveType::Count:
            break;
        }

        throw std::runtime_error("Unsupported primitive type requested from makePrimitiveMesh");
    }
} // namespace Faye::Assets
