#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Faye
{
    enum class ShaderStage
    {
        Unknown,
        Vertex,
        Fragment,
        Compute,
        Geometry,
        TessellationControl,
        TessellationEvaluation,
    };

    // A single member inside a UBO, SSBO, or push constant block.
    struct ShaderMember
    {
        std::string name;
        uint32_t    offsetInBytes = 0;
        uint32_t    sizeInBytes   = 0;
        uint32_t    arrayCount    = 1;

        enum class Type
        {
            Unknown,
            Float,
            Vec2,
            Vec3,
            Vec4,
            Int,
            UInt,
            Bool,
            Mat2,
            Mat3,
            Mat4,
            Sampler2D,
        } type = Type::Float;
    };

    // A descriptor set binding: uniform/storage buffer, image, or sampler.
    struct ShaderBinding
    {
        std::string name;
        uint32_t    set     = 0;
        uint32_t    binding = 0;
        uint32_t    count   = 1; // >1 for arrays; 0 for runtime/bindless arrays

        enum class Kind
        {
            UniformBuffer,
            StorageBuffer,
            CombinedImageSampler,
            SampledImage,
            Sampler,
            StorageImage,
        } kind = Kind::UniformBuffer;

        uint32_t                  blockSizeBytes = 0;
        std::vector<ShaderMember> members;        // populated for UBO / SSBO
    };

    // A push constant block with its member layout.
    struct PushConstantRange
    {
        std::string               name;
        uint32_t                  offset    = 0;
        uint32_t                  sizeBytes = 0;
        std::vector<ShaderMember> members;
    };

    // An input interface variable (vertex attributes for vertex shaders).
    struct ShaderInputVariable
    {
        std::string        name;
        uint32_t           location = 0;
        ShaderMember::Type type     = ShaderMember::Type::Unknown;
    };

    // All data reflected from one compiled SPIRV module.
    struct ShaderReflectionData
    {
        ShaderStage stage      = ShaderStage::Unknown;
        std::string entryPoint;

        std::vector<ShaderBinding>      bindings;
        std::vector<PushConstantRange>  pushConstants;
        std::vector<ShaderInputVariable> inputs; // vertex attributes (vertex stage only)
    };

    // Reflect a compiled SPIRV module given as a word (uint32_t) stream.
    // Returns a default-constructed ShaderReflectionData on failure.
    ShaderReflectionData reflectShader(const std::vector<uint32_t>& spirvCode);

} // namespace Faye
