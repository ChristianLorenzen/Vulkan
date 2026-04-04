#pragma once

#include "Vulkan.hpp"

namespace Faye
{
    class VulkanShaderManager
    {
    public:
        VulkanShaderManager();
        ~VulkanShaderManager();

        VulkanShaderManager(const VulkanShaderManager &) = delete;
        void operator=(const VulkanShaderManager &) = delete;
        VulkanShaderManager(VulkanShaderManager &&) = delete;
        VulkanShaderManager &operator=(VulkanShaderManager &&) = delete;

        static std::string shaderFileChange(const std::filesystem::path &path);

    private:
        static std::string resolveCompiledShaderPath(const std::filesystem::path &shaderPath);
        static std::string compileShader(const std::filesystem::path &sourcePath);
    };
};