#include "vk_shader_manager.hpp"
#include "Core/Path/Paths.hpp"

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
    return Paths::compiledShader(fileName).string();
};

std::string Faye::VulkanShaderManager::compileShader(const std::filesystem::path &sourcePath)
{
    const std::string resolvedOutputPath = resolveCompiledShaderPath(sourcePath);
    LOG_INFO(Logger::get(), "Compiling shader: {} -> {}", sourcePath.filename().string(), resolvedOutputPath);
    const std::string command = "glslc " + sourcePath.string() + " -I " + Paths::shaderSources().string() + " -o " + resolvedOutputPath;
    int success = std::system(command.c_str());
    if (success == 0)
    {
        LOG_INFO(Logger::get(), "Shader compiled successfully: {}", sourcePath.filename().string());
        return resolvedOutputPath;
    }
    else
    {
        LOG_ERROR(Logger::get(), "Failed to compile shader: {}", sourcePath.filename().string());
        return "";
    }
}
