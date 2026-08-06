#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Core/ECS/Reflection/Annotations.hpp"
#include "Core/ECS/Reflection/TypeDescriptor.hpp"

namespace Faye
{
    struct PostProcessParameterBlock
    {
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 params{0.0f, 0.0f, 0.0f, 0.0f};
    };

    enum class PostProcessParameterControlType : uint8_t
    {
        Float = 0,
        Color4,
    };

    enum class PostProcessParameterBinding : uint8_t
    {
        Color = 0,
        ParamX,
        ParamY,
        ParamZ,
        ParamW,
    };

    struct PostProcessInputDefinition
    {
        std::string semanticName{};
        uint32_t binding = 0;
    };

    struct PostProcessParameterDefinition
    {
        std::string id{};
        std::string label{};
        PostProcessParameterControlType controlType{PostProcessParameterControlType::Float};
        PostProcessParameterBinding binding{PostProcessParameterBinding::ParamX};
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float dragSpeed = 0.01f;
    };

    struct PostProcessEffectDefinition
    {
        std::string id{};
        std::string displayName{};
        std::string vertexShaderPath{};
        std::string fragmentShaderPath{};
        std::vector<PostProcessInputDefinition> inputs{};
        std::vector<PostProcessParameterDefinition> parameters{};
        PostProcessParameterBlock defaultParameters{};
        bool showInEditor = true;
    };

    // Feeds the definitionId dropdown below. Declared here so the annotation
    // can name it; defined in the .cpp, which is the only place that needs to
    // know how the definition list is stored.
    void postProcessEffectOptions(const Ecs::OptionSink &sink);

    struct PostProcessEffectComponent
    {
        // The provider is named by REFLECTION, so renaming the function above
        // breaks the build here instead of silently turning this into a text
        // box. The set is whatever the library holds at draw time, so an effect
        // added by a reload appears without touching this header.
        FAYE_ATTR(Ecs::OptionsFrom{^^Faye::postProcessEffectOptions})
        FAYE_ATTR(Ecs::Tooltip("Which effect this slot runs."))
        std::string definitionId{"tint"};
        bool enabled{true};
        FAYE_ATTR(Ecs::Category("Parameters")) PostProcessParameterBlock parameters{};
    };

    struct FAYE_ATTR(Ecs::TypeName("Post Process Stack")) PostProcessStackComponent
    {
        bool enabled{true};
        // Order is the render order, so reordering is meaningful here -- which
        // is exactly why Orderable is a per-FIELD opt-in and not a property of
        // std::vector.
        FAYE_ATTR(Ecs::Orderable)
        FAYE_ATTR(Ecs::Category("Effects"))
        // Rows read "1. tint" instead of "0". Names a member of the ELEMENT
        // type, by reflection, so it survives a rename of either side.
        FAYE_ATTR(Ecs::TitleFrom{^^Faye::PostProcessEffectComponent::definitionId})
        std::vector<PostProcessEffectComponent> effects{};
    };

    void registerPostProcessEffectDefinition(PostProcessEffectDefinition definition);
    const std::vector<PostProcessEffectDefinition> &getPostProcessEffectDefinitions();
    const PostProcessEffectDefinition *findPostProcessEffectDefinition(std::string_view id);
    const PostProcessEffectDefinition &getPostProcessPresentEffectDefinition();
    void reloadPostProcessEffectDefinitions();

    PostProcessEffectComponent makeDefaultPostProcessEffect(std::string_view definitionId);
    PostProcessStackComponent makeDefaultPostProcessStack();

    float *getPostProcessFloatParameter(PostProcessEffectComponent &effect, PostProcessParameterBinding binding);
}