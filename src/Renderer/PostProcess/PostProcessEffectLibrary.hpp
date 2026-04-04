#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

    struct PostProcessEffectComponent
    {
        std::string definitionId{"tint"};
        bool enabled{true};
        PostProcessParameterBlock parameters{};
    };

    struct PostProcessStackComponent
    {
        bool enabled{true};
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