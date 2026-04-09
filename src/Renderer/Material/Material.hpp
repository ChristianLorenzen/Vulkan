#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <assimp/material.h>

namespace Faye
{

    enum class TextureType
    {
        Albedo,
        Normal,
        Metallic,
        Roughness,
        AmbientOcclusion,
        Height,
    };

    struct Texture
    {
        using AssetId = uint32_t;
        AssetId id = 0;
        TextureType type;
        std::string path;
        std::vector<unsigned char> data;
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
            texture.data = std::move(data);
            texture.width = width;
            texture.height = height;
            texture.channels = channels;
            texture.type = type;
            texture.path = std::move(path);
            return texture;
        }

        static TextureType textureTypeFromAssimp(aiTextureType assimpType)
        {
            switch (assimpType)
            {
            case aiTextureType_DIFFUSE:
                return TextureType::Albedo;
            case aiTextureType_NORMALS:
                return TextureType::Normal;
            case aiTextureType_METALNESS:
                return TextureType::Metallic;
            case aiTextureType_DIFFUSE_ROUGHNESS:
                return TextureType::Roughness;
            case aiTextureType_AMBIENT_OCCLUSION:
                return TextureType::AmbientOcclusion;
            case aiTextureType_HEIGHT:
                return TextureType::Height;
            default:
                return TextureType::Albedo; // Default to Albedo if type is unrecognized
            }
        }
    };

    struct MaterialData
    {
        std::string name;
        std::vector<Texture> textures;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
        glm::vec3 ambient{1.0f, 1.0f, 1.0f};
        glm::vec3 specular{1.0f, 1.0f, 1.0f};
        float shininess = 0.0f;

        MaterialData() = default;

        MaterialData(std::string name, const glm::vec3 &color)
            : name(std::move(name)), color(color), diffuse(color)
        {
        }

        MaterialData(std::string name, const glm::vec3 &color, std::vector<Texture> textures)
            : name(std::move(name)), textures(std::move(textures)), color(color), diffuse(color)
        {
        }
    };

    struct MaterialPipelineConfig
    {
        std::string vertexShaderPath{"shader.vert"};
        std::string fragmentShaderPath{"shader.frag"};

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

        void setMaterialData(MaterialData data) { materialData = std::move(data); }
        void setPipelineConfig(MaterialPipelineConfig config) { pipelineConfig = std::move(config); }

    private:
        MaterialData materialData{};
        MaterialPipelineConfig pipelineConfig{};
    };
}