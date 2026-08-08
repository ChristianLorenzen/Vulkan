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
        return root() / "src" / "engine" / "Assets";
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

    std::string Paths::toProjectRelative(std::string_view path)
    {
        if (path.empty())
        {
            return {};
        }

        const std::filesystem::path normalized = std::filesystem::path(path).lexically_normal();
        if (!normalized.is_absolute())
        {
            return normalized.generic_string();
        }

        const std::filesystem::path relative = normalized.lexically_relative(root());
        // Empty means the two paths share no root (different drive on Windows);
        // a leading ".." means the file lives outside the repo. Neither is
        // portable, so the absolute path is the most useful thing we can keep.
        if (relative.empty() || *relative.begin() == "..")
        {
            return normalized.generic_string();
        }

        return relative.generic_string();
    }

    std::string Paths::fromProjectRelative(std::string_view path)
    {
        if (path.empty())
        {
            return {};
        }

        const std::filesystem::path stored{path};
        if (stored.is_absolute())
        {
            return stored.lexically_normal().generic_string();
        }

        return (root() / stored).lexically_normal().generic_string();
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
