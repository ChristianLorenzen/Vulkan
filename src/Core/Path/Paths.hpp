#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Faye
{
    // Central path resolution for all engine assets.
    // All paths are absolute and derived from FAYE_ASSET_ROOT (the repo root,
    // injected by CMake), eliminating working-directory dependency on any platform.
    class Paths
    {
    public:
        // Absolute path to the repository root.
        static const std::filesystem::path &root();

        // src/shaders/compiled/
        static std::filesystem::path compiledShaders();

        // src/shaders/  (shader source files, watched for hot-reload)
        static std::filesystem::path shaderSources();

        // src/Assets/
        static std::filesystem::path assets();

        // bin/
        static std::filesystem::path bin();

        static std::filesystem::path projects();

        // Resolve any path relative to the repository root.
        static std::filesystem::path resolve(std::string_view relative);

        // Absolute path to a compiled shader. Appends .spv if not already present.
        static std::filesystem::path compiledShader(std::string_view name);

        static std::string getFileName(std::filesystem::path path);
        static std::string getFileName(std::string path);

        // Canonical form for file-extension comparisons: lowercase, with a
        // leading dot ("PNG" -> ".png", ".Vert" -> ".vert"). Empty stays empty.
        static std::string normalizeExtension(std::string extension);
    };

} // namespace Faye
