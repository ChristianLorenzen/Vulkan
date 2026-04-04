#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Faye
{

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
        const std::string &getName() const { return name; }
        const std::string &getVertexShaderPath() const { return vertexShaderPath; }
        const std::string &getFragmentShaderPath() const { return fragmentShaderPath; }
        const glm::vec3 &getColor() const { return color; }
        void setColor(const glm::vec3 &newColor) { color = newColor; }

    private:
        std::string name;
        std::string vertexShaderPath;
        std::string fragmentShaderPath;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
    };
}