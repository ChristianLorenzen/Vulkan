#include "vk_shader_manager.hpp"

#include <filesystem>
#include <stdio.h>

Faye::VulkanShaderManager::VulkanShaderManager() {
};

Faye::VulkanShaderManager::~VulkanShaderManager() {
};

std::string Faye::VulkanShaderManager::shaderFileChange(const std::filesystem::path &path)
{
    return compileShader(path);
}

std::string Faye::VulkanShaderManager::resolveCompiledShaderPath(const std::filesystem::path &shaderPath)
{
    std::string fileName = shaderPath.filename().string();
    return "./src/shaders/compiled/" + fileName + ".spv";
};

std::string Faye::VulkanShaderManager::compileShader(const std::filesystem::path &sourcePath)
{
    const std::string resolvedOutputPath = resolveCompiledShaderPath(sourcePath);
    LOG_INFO(Logger::getInstance(), "Compiling shader: {} -> {}", sourcePath.filename().string(), resolvedOutputPath);
    const std::string command = "glslc " + sourcePath.string() + " -o " + resolvedOutputPath;
    int success = std::system(command.c_str());
    if (success == 0)
    {
        LOG_INFO(Logger::getInstance(), "Shader compiled successfully: {}", sourcePath.filename().string());
        return resolvedOutputPath;
    }
    else
    {
        LOG_ERROR(Logger::getInstance(), "Failed to compile shader: {}", sourcePath.filename().string());
        return "";
    }
}
