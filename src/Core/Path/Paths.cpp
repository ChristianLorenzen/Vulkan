#include "Core/Path/Paths.hpp"

#include <algorithm>
#include <cctype>

// FAYE_ASSET_ROOT is injected by CMake as the absolute path to the repo root.
// It is intentionally resolved here — this is the single place in the codebase
// that knows about it.
#ifndef FAYE_ASSET_ROOT
#  error "FAYE_ASSET_ROOT must be defined via CMake target_compile_definitions"
#endif

namespace Faye
{
    const std::filesystem::path &Paths::root()
    {
        static const std::filesystem::path kRoot{FAYE_ASSET_ROOT};
        return kRoot;
    }

    std::filesystem::path Paths::compiledShaders()
    {
        return root() / "src" / "shaders" / "compiled";
    }

    std::filesystem::path Paths::shaderSources()
    {
        return root() / "src" / "shaders";
    }

    std::filesystem::path Paths::assets()
    {
        return root() / "src" / "Assets";
    }

    std::filesystem::path Paths::bin()
    {
        return root() / "bin";
    }

    std::filesystem::path Paths::projects()
    {
        return root() / "assets";
    }

    std::filesystem::path Paths::resolve(std::string_view relative)
    {
        return root() / relative;
    }

    std::filesystem::path Paths::compiledShader(std::string_view name)
    {
        auto p = compiledShaders() / name;
        if (p.extension() != ".spv")
            p += ".spv";
        return p;
    }

    std::string Paths::getFileName(std::filesystem::path path)
    {
        return path.filename().string();
    }

    std::string Paths::getFileName(std::string path)
    {
        // TODO: Seems string parsing might be more efficient. Not worried for now.
        return std::filesystem::path(path).filename().string();
    }

    std::string Paths::normalizeExtension(std::string extension)
    {
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char character)
                       { return static_cast<char>(std::tolower(character)); });

        if (!extension.empty() && extension.front() != '.')
        {
            extension.insert(extension.begin(), '.');
        }

        return extension;
    }

} // namespace Faye
