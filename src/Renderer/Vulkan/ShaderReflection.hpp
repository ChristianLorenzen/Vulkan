#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Faye
{
    // Describes a single property (uniform member or sampler binding) discovered
    // by reflecting a SPIRV shader module.
    struct ShaderProperty
    {
        std::string name;

        enum class Type
        {
            Float,
            Vec2,
            Vec3,
            Vec4,
            Int,
            Sampler2D,
        } type = Type::Float;

        uint32_t offsetInBytes = 0; // byte offset within the UBO block (0 for samplers)
        uint32_t binding = 0;
        uint32_t set = 0;
    };

    // All properties reflected from one shader stage.
    struct ShaderReflectionData
    {
        std::vector<ShaderProperty> properties;
        uint32_t uniformBufferSize = 0; // total byte size of the first properties UBO found
    };

    // Reflect a compiled SPIRV module given as a word (uint32_t) stream.
    // Returns an empty ShaderReflectionData on failure.
    ShaderReflectionData reflectShader(const std::vector<uint32_t> &spirvCode);
} // namespace Faye
