#include "ShaderReflection.hpp"

// Suppress any pedantic warnings from the C header when compiled as C++.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include <spirv_reflect.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <cstddef>

namespace Faye
{
    namespace
    {
        ShaderProperty::Type memberType(const SpvReflectTypeDescription *td)
        {
            if (td == nullptr)
            {
                return ShaderProperty::Type::Float;
            }

            const uint32_t vecs = td->traits.numeric.vector.component_count;
            const bool isInt = (td->type_flags & SPV_REFLECT_TYPE_FLAG_INT) != 0;

            if (vecs == 4) return ShaderProperty::Type::Vec4;
            if (vecs == 3) return ShaderProperty::Type::Vec3;
            if (vecs == 2) return ShaderProperty::Type::Vec2;
            if (isInt)     return ShaderProperty::Type::Int;
            return ShaderProperty::Type::Float;
        }
    } // anonymous namespace

    ShaderReflectionData reflectShader(const std::vector<uint32_t> &spirvCode)
    {
        ShaderReflectionData result{};

        if (spirvCode.empty())
        {
            return result;
        }

        SpvReflectShaderModule module{};
        const SpvReflectResult spvResult = spvReflectCreateShaderModule(
            spirvCode.size() * sizeof(uint32_t),
            spirvCode.data(),
            &module);

        if (spvResult != SPV_REFLECT_RESULT_SUCCESS)
        {
            return result;
        }

        uint32_t setCount = 0;
        spvReflectEnumerateDescriptorSets(&module, &setCount, nullptr);

        std::vector<SpvReflectDescriptorSet *> sets(setCount);
        spvReflectEnumerateDescriptorSets(&module, &setCount, sets.data());

        for (const SpvReflectDescriptorSet *descSet : sets)
        {
            for (uint32_t b = 0; b < descSet->binding_count; ++b)
            {
                const SpvReflectDescriptorBinding *binding = descSet->bindings[b];
                if (binding == nullptr)
                {
                    continue;
                }

                const bool isSampler =
                    binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                    binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

                if (isSampler)
                {
                    ShaderProperty prop{};
                    prop.name    = binding->name != nullptr ? binding->name : "";
                    prop.type    = ShaderProperty::Type::Sampler2D;
                    prop.binding = binding->binding;
                    prop.set     = descSet->set;
                    result.properties.push_back(std::move(prop));
                }
                else if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    if (result.uniformBufferSize == 0)
                    {
                        result.uniformBufferSize = binding->block.size;
                    }

                    for (uint32_t m = 0; m < binding->block.member_count; ++m)
                    {
                        const SpvReflectBlockVariable &member = binding->block.members[m];

                        ShaderProperty prop{};
                        prop.name          = member.name != nullptr ? member.name : "";
                        prop.offsetInBytes = member.offset;
                        prop.binding       = binding->binding;
                        prop.set           = descSet->set;
                        prop.type          = memberType(member.type_description);
                        result.properties.push_back(std::move(prop));
                    }
                }
            }
        }

        spvReflectDestroyShaderModule(&module);
        return result;
    }

} // namespace Faye
