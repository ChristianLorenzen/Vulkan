#include "ShaderReflection.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include <spirv_reflect.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace Faye
{
    namespace
    {
        ShaderMember::Type memberType(const SpvReflectTypeDescription* td)
        {
            if (td == nullptr)
                return ShaderMember::Type::Unknown;

            const bool isMatrix = (td->type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) != 0;
            const bool isInt    = (td->type_flags & SPV_REFLECT_TYPE_FLAG_INT)    != 0;
            const bool isBool   = (td->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL)   != 0;

            if (isMatrix)
            {
                switch (td->traits.numeric.matrix.column_count)
                {
                    case 4: return ShaderMember::Type::Mat4;
                    case 3: return ShaderMember::Type::Mat3;
                    case 2: return ShaderMember::Type::Mat2;
                    default: break;
                }
                return ShaderMember::Type::Unknown;
            }

            if (isBool)
                return ShaderMember::Type::Bool;

            const uint32_t vecs = td->traits.numeric.vector.component_count;
            if (vecs == 4) return ShaderMember::Type::Vec4;
            if (vecs == 3) return ShaderMember::Type::Vec3;
            if (vecs == 2) return ShaderMember::Type::Vec2;

            if (isInt)
            {
                const bool isSigned = td->traits.numeric.scalar.signedness != 0;
                return isSigned ? ShaderMember::Type::Int : ShaderMember::Type::UInt;
            }

            return ShaderMember::Type::Float;
        }

        ShaderMember reflectMember(const SpvReflectBlockVariable& var)
        {
            ShaderMember m;
            m.name          = var.name ? var.name : "";
            m.offsetInBytes = var.offset;
            m.sizeInBytes   = var.size;
            m.arrayCount    = (var.array.dims_count > 0) ? var.array.dims[0] : 1;
            m.type          = memberType(var.type_description);
            return m;
        }

        ShaderStage toShaderStage(SpvReflectShaderStageFlagBits stage)
        {
            switch (stage)
            {
                case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:                  return ShaderStage::Vertex;
                case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:                return ShaderStage::Fragment;
                case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:                 return ShaderStage::Compute;
                case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:                return ShaderStage::Geometry;
                case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    return ShaderStage::TessellationControl;
                case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return ShaderStage::TessellationEvaluation;
                default:                                                    return ShaderStage::Unknown;
            }
        }
    } // anonymous namespace

    ShaderReflectionData reflectShader(const std::vector<uint32_t>& spirvCode)
    {
        ShaderReflectionData result{};

        if (spirvCode.empty())
            return result;

        SpvReflectShaderModule module{};
        const SpvReflectResult spvResult = spvReflectCreateShaderModule(
            spirvCode.size() * sizeof(uint32_t),
            spirvCode.data(),
            &module);

        if (spvResult != SPV_REFLECT_RESULT_SUCCESS)
            return result;

        // Stage and entry point.
        result.stage      = toShaderStage(module.shader_stage);
        result.entryPoint = module.entry_point_name ? module.entry_point_name : "";

        // Descriptor set bindings.
        uint32_t setCount = 0;
        spvReflectEnumerateDescriptorSets(&module, &setCount, nullptr);

        std::vector<SpvReflectDescriptorSet*> sets(setCount);
        spvReflectEnumerateDescriptorSets(&module, &setCount, sets.data());

        for (const SpvReflectDescriptorSet* descSet : sets)
        {
            for (uint32_t b = 0; b < descSet->binding_count; ++b)
            {
                const SpvReflectDescriptorBinding* src = descSet->bindings[b];
                if (src == nullptr)
                    continue;

                ShaderBinding sb;
                sb.name    = src->name ? src->name : "";
                sb.set     = descSet->set;
                sb.binding = src->binding;
                sb.count   = src->count;

                switch (src->descriptor_type)
                {
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                        sb.kind           = ShaderBinding::Kind::UniformBuffer;
                        sb.blockSizeBytes = src->block.size;
                        for (uint32_t m = 0; m < src->block.member_count; ++m)
                            sb.members.push_back(reflectMember(src->block.members[m]));
                        break;

                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        sb.kind           = ShaderBinding::Kind::StorageBuffer;
                        sb.blockSizeBytes = src->block.size;
                        for (uint32_t m = 0; m < src->block.member_count; ++m)
                            sb.members.push_back(reflectMember(src->block.members[m]));
                        break;

                    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                        sb.kind = ShaderBinding::Kind::CombinedImageSampler;
                        break;

                    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                        sb.kind = ShaderBinding::Kind::SampledImage;
                        break;

                    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                        sb.kind = ShaderBinding::Kind::Sampler;
                        break;

                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        sb.kind = ShaderBinding::Kind::StorageImage;
                        break;

                    default:
                        continue; // skip acceleration structures, input attachments, etc.
                }

                result.bindings.push_back(std::move(sb));
            }
        }

        // Push constant blocks.
        uint32_t pushCount = 0;
        spvReflectEnumeratePushConstantBlocks(&module, &pushCount, nullptr);

        std::vector<SpvReflectBlockVariable*> pushBlocks(pushCount);
        spvReflectEnumeratePushConstantBlocks(&module, &pushCount, pushBlocks.data());

        for (const SpvReflectBlockVariable* block : pushBlocks)
        {
            if (block == nullptr)
                continue;

            PushConstantRange pcr;
            pcr.name      = block->name ? block->name : "";
            pcr.offset    = block->offset;
            pcr.sizeBytes = block->size;
            for (uint32_t m = 0; m < block->member_count; ++m)
                pcr.members.push_back(reflectMember(block->members[m]));
            result.pushConstants.push_back(std::move(pcr));
        }

        // Input variables — only meaningful for vertex shaders.
        if (result.stage == ShaderStage::Vertex)
        {
            uint32_t inputCount = 0;
            spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);

            std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
            spvReflectEnumerateInputVariables(&module, &inputCount, inputs.data());

            for (const SpvReflectInterfaceVariable* var : inputs)
            {
                if (var == nullptr)
                    continue;
                // Skip SPIRV built-ins (gl_VertexIndex, gl_InstanceIndex, etc.)
                if (var->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
                    continue;

                ShaderInputVariable iv;
                iv.name     = var->name ? var->name : "";
                iv.location = var->location;
                iv.type     = memberType(var->type_description);
                result.inputs.push_back(std::move(iv));
            }
        }

        spvReflectDestroyShaderModule(&module);
        return result;
    }

} // namespace Faye
