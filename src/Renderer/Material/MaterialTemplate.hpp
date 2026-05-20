#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Renderer/Vulkan/ShaderReflection.hpp"

namespace Faye
{
    // A MaterialTemplate describes a named custom shader pair along with the
    // descriptor properties inferred from that shader via SPIRV reflection.
    // Handle 0 is implicitly reserved for the built-in PBR pipeline.
    struct MaterialTemplate
    {
        std::string name;
        std::string vertShaderPath; // path to compiled .spv (or source .vert)
        std::string fragShaderPath;
        ShaderReflectionData reflectionData;
    };

    // Registry that owns and issues handles for MaterialTemplates.
    // Template handles start at 1; handle 0 always means "built-in PBR".
    class MaterialTemplateRegistry
    {
    public:
        using Handle = uint32_t;

        // Register a template and return its handle (>= 1).
        Handle registerTemplate(MaterialTemplate tmpl);

        // Look up a registered template. Returns nullptr for handle 0 (built-in)
        // or any out-of-range handle.
        const MaterialTemplate *get(Handle handle) const;

        // Handle value that refers to the built-in PBR pipeline.
        Handle getDefault() const { return 0u; }

    private:
        // templates[i] corresponds to handle (i + 1).
        std::vector<MaterialTemplate> templates;
        uint32_t nextHandle = 1;
    };
} // namespace Faye
