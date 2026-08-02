#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Renderer/Vulkan/ShaderReflection.hpp"

namespace Faye
{
    struct MaterialTemplatePropertyDescriptor
    {
        std::string label;
        std::string field;
        ShaderMember::Type type = ShaderMember::Type::Float;
        float minVal = 0.0f;
        float maxVal = 1.0f;
    };

    struct MaterialTemplate
    {
        std::string name;
        std::string vertShaderPath;
        std::string fragShaderPath;
        ShaderReflectionData reflectionData;
        std::vector<MaterialTemplatePropertyDescriptor> properties;
    };

    class MaterialTemplateRegistry
    {
    public:
        using Handle = uint32_t;

        Handle registerTemplate(MaterialTemplate tmpl);
        const MaterialTemplate *get(Handle handle) const;
        Handle getDefault() const { return 0u; }
        uint32_t count() const { return nextHandle - 1u; }

    private:
        std::vector<MaterialTemplate> templates;
        uint32_t nextHandle = 1;
    };
} // namespace Faye
