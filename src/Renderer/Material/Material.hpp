#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <assimp/material.h>

namespace Faye
{
    enum class MaterialAlphaMode : uint32_t
    {
        Opaque = 0,
        Mask = 1,
    };

    // Which render paths a material participates in. Replaces matching on
    // "does the shader path contain 'water'" (see renderDepthPrepass) with an
    // explicit, pipeline-affecting property.
    enum class MaterialDomain
    {
        Opaque,      // participates in the depth prepass, drawn as triangles
        Transparent, // skips the depth prepass, drawn as triangles
        Water,       // skips the depth prepass, drawn as tessellated patches
    };

    // Handle to a MaterialTemplate (0 = built-in PBR).
    using MaterialTemplateHandle = uint32_t;
    static constexpr MaterialTemplateHandle kBuiltinPBRTemplateHandle = 0;

    enum class TextureType
    {
        Albedo,
        Normal,
        Metallic,
        Roughness,
        AmbientOcclusion,
        Height,
        Equirectangular,
    };

    struct TextureTypeHasher
    {
        size_t operator()(TextureType type) const
        {
            return std::hash<uint32_t>{}(static_cast<uint32_t>(type));
        }
    };

    struct HdrImage { std::vector<float> pixels; int width = 0; int height = 0; };

    struct Texture
    {
        using AssetId = uint32_t;
        AssetId id = 0;
        TextureType type;
        std::string path;
        std::shared_ptr<std::vector<unsigned char>> data;
        int width = 0;
        int height = 0;
        int channels = 0;

        static Texture create(const std::string &path, TextureType type)
        {
            Texture texture{};
            texture.path = path;
            texture.type = type;
            return texture;
        }

        static Texture create(std::vector<unsigned char> data, int width, int height, int channels, std::string path, TextureType type)
        {
            Texture texture{};
            texture.data = std::make_shared<std::vector<unsigned char>>(std::move(data));
            texture.width = width;
            texture.height = height;
            texture.channels = channels;
            texture.type = type;
            texture.path = std::move(path);
            return texture;
        }

        bool hasPixelData() const
        {
            return data != nullptr && !data->empty();
        }

        size_t byteSize() const
        {
            return data != nullptr ? data->size() : 0;
        }

        static std::optional<TextureType> textureTypeFromAssimp(aiTextureType assimpType)
        {
            switch (assimpType)
            {
            case aiTextureType_BASE_COLOR:
            case aiTextureType_DIFFUSE:
                return TextureType::Albedo;
            case aiTextureType_NORMAL_CAMERA:
            case aiTextureType_NORMALS:
                return TextureType::Normal;
            case aiTextureType_METALNESS:
                return TextureType::Metallic;
            case aiTextureType_DIFFUSE_ROUGHNESS:
                return TextureType::Roughness;
            case aiTextureType_AMBIENT_OCCLUSION:
                return TextureType::AmbientOcclusion;
            default:
                return std::nullopt;
            }
        }
    };

    struct MaterialData
    {
        std::string name;
        std::vector<Texture> textures;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
        glm::vec3 ambient{1.0f, 1.0f, 1.0f};
        glm::vec3 specular{1.0f, 1.0f, 1.0f};
        glm::vec3 emissive{0.0f, 0.0f, 0.0f};
        float shininess = 0.0f;
        float opacity = 1.0f;
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;
        float normalScale = 1.0f;
        float occlusionStrength = 1.0f;
        float specularStrength = 1.0f;
        float reflectivity = 0.0f;
        float emissiveIntensity = 1.0f;
        MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;

        // 0 = built-in PBR pipeline. Non-zero values index into MaterialTemplateRegistry.
        MaterialTemplateHandle templateHandle = kBuiltinPBRTemplateHandle;

        MaterialData() = default;

        MaterialData(std::string name, const glm::vec3 &color)
            : name(std::move(name)), color(color), baseColorFactor(color, 1.0f), diffuse(color)
        {
        }

        MaterialData(std::string name, const glm::vec3 &color, std::vector<Texture> textures)
            : name(std::move(name)), textures(std::move(textures)), color(color), baseColorFactor(color, 1.0f), diffuse(color)
        {
        }

        MaterialData(std::string name,
                     const glm::vec3 &color,
                     std::vector<Texture> textures,
                     const glm::vec4 &baseColorFactor,
                     const glm::vec4 &surfaceFactors,
                     const glm::vec4 &specularShininess)
            : name(std::move(name)),
              textures(std::move(textures)),
              color(color),
              baseColorFactor(baseColorFactor),
              diffuse(color),
              specular(specularShininess.x, specularShininess.y, specularShininess.z),
              shininess(specularShininess.w),
              opacity(baseColorFactor.a),
              metallicFactor(surfaceFactors.x),
              roughnessFactor(surfaceFactors.y),
              normalScale(surfaceFactors.z),
              occlusionStrength(surfaceFactors.w)
        {
        }
    };

    struct MaterialPipelineConfig
    {
        std::string vertexShaderPath{"shader.vert"};
        std::string fragmentShaderPath{"shader.frag"};

        // When true the pipeline blends colour attachment 0 with standard
        // src-alpha / one-minus-src-alpha blending and disables depth writes
        // (depth testing stays enabled). Used by translucent materials such
        // as water. Motion-vector attachment 1 is never blended.
        bool enableAlphaBlending = false;

        // Which render paths this material participates in (see MaterialDomain).
        MaterialDomain domain = MaterialDomain::Opaque;

        // Optional tessellation control/evaluation shader paths. Non-empty only for
        // domains that render as tessellated patches (currently just Water, once
        // Phase 4's ring mesh lands). Empty means a standard vertex+fragment pipeline.
        std::string tessControlShaderPath;
        std::string tessEvalShaderPath;

        MaterialPipelineConfig() = default;

        MaterialPipelineConfig(std::string vertexShaderPath, std::string fragmentShaderPath)
            : vertexShaderPath(std::move(vertexShaderPath)), fragmentShaderPath(std::move(fragmentShaderPath))
        {
        }
    };

    class Material
    {
    public:
        Material() = default;
        explicit Material(MaterialData materialData, MaterialPipelineConfig pipelineConfig = {})
            : materialData(std::move(materialData)), pipelineConfig(std::move(pipelineConfig))
        {
        }

        Material(std::string name, std::string vertexShaderPath, std::string fragmentShaderPath)
            : Material(MaterialData(std::move(name), {1.0f, 1.0f, 1.0f}), MaterialPipelineConfig(std::move(vertexShaderPath), std::move(fragmentShaderPath)))
        {
        }

        Material(std::string name, std::string vertexShaderPath, std::string fragmentShaderPath, const glm::vec3 &color = {1.0f, 1.0f, 1.0f})
            : Material(MaterialData(std::move(name), color), MaterialPipelineConfig(std::move(vertexShaderPath), std::move(fragmentShaderPath)))
        {
        }

        Material(std::string name, std::string vertexShaderPath, std::string fragmentShaderPath, const glm::vec3 &color, std::vector<Texture> textures)
            : Material(MaterialData(std::move(name), color, std::move(textures)), MaterialPipelineConfig(std::move(vertexShaderPath), std::move(fragmentShaderPath)))
        {
        }

        const std::string &getName() const { return materialData.name; }
        const std::string &getVertexShaderPath() const { return pipelineConfig.vertexShaderPath; }
        const std::string &getFragmentShaderPath() const { return pipelineConfig.fragmentShaderPath; }
        const glm::vec3 &getColor() const { return materialData.color; }
        const std::vector<Texture> &getTextures() const { return materialData.textures; }
        const MaterialData &getMaterialData() const { return materialData; }
        MaterialData &getMaterialData() { return materialData; }
        const MaterialPipelineConfig &getPipelineConfig() const { return pipelineConfig; }
        uint64_t getRevision() const { return revision; }

        void setMaterialData(MaterialData data)
        {
            materialData = std::move(data);
            markDirty();
        }

        void setPipelineConfig(MaterialPipelineConfig config)
        {
            pipelineConfig = std::move(config);
            markDirty();
        }

        void markDirty()
        {
            ++revision;
        }

    private:
        MaterialData materialData{};
        MaterialPipelineConfig pipelineConfig{};
        uint64_t revision = 1;
    };
}