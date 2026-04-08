#pragma once

#include <cstdint>
#include <string>
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
        AssetId id;
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

        static Texture create(const std::vector<unsigned char> &data, int width, int height, int channels, std::string path, TextureType type)
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

    class Material
    {
    public:
        Material() = default;
        Material(std::string name, std::string vertexShaderPath, std::string fragmentShaderPath)
            : name(std::move(name)), vertexShaderPath(std::move(vertexShaderPath)), fragmentShaderPath(std::move(fragmentShaderPath))
        {
        }
        Material(std::string name, std::string vertexShaderPath, std::string fragmentShaderPath, const glm::vec3 &color = {1.0f, 1.0f, 1.0f})
            : name(std::move(name)), vertexShaderPath(std::move(vertexShaderPath)), fragmentShaderPath(std::move(fragmentShaderPath)), color(color)
        {
        }
        Material(std::string name, std::string vertexShaderPath, std::string fragmentShaderPath, const glm::vec3 &color, std::vector<Texture> textures)
            : name(std::move(name)), vertexShaderPath(std::move(vertexShaderPath)), fragmentShaderPath(std::move(fragmentShaderPath)), color(color), textures(std::move(textures))
        {
        }
        const std::string &getName() const { return name; }
        const std::string &getVertexShaderPath() const { return vertexShaderPath; }
        const std::string &getFragmentShaderPath() const { return fragmentShaderPath; }
        const glm::vec3 &getColor() const { return color; }
        const std::vector<Texture> &getTextures() const { return textures; }
        void addTexture(const Texture &texture) { textures.push_back(texture); }
        void setColor(const glm::vec3 &newColor) { color = newColor; }
        void setDiffuse(const glm::vec3 &newDiffuse) { diffuse = newDiffuse; }
        void setAmbient(const glm::vec3 &newAmbient) { ambient = newAmbient; }
        void setSpecular(const glm::vec3 &newSpecular) { specular = newSpecular; }
        void setShininess(float newShininess) { shininess = newShininess; }
        void setName(const std::string &newName) { name = newName; }

    private:
        std::string name;
        std::string vertexShaderPath;
        std::string fragmentShaderPath;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        std::vector<Texture> textures;
        glm::vec3 diffuse;
        glm::vec3 ambient;
        glm::vec3 specular;
        float shininess;
    };
}