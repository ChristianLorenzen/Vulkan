#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"

#include "Core/IO/FileSystem.hpp"
#include "Core/Path/Paths.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace Faye
{
    namespace
    {
        const std::filesystem::path kEffectDefinitionsDirectory = Paths::assets() / "PostProcessEffects";

        std::vector<PostProcessEffectDefinition> &registry()
        {
            static std::vector<PostProcessEffectDefinition> definitions{};
            return definitions;
        }

        bool &definitionsLoaded()
        {
            static bool loaded = false;
            return loaded;
        }

        std::string trim(std::string value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return {};
            }

            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        std::vector<std::string> splitCommaSeparated(std::string_view value)
        {
            std::vector<std::string> parts;
            std::stringstream stream{std::string(value)};
            std::string item;
            while (std::getline(stream, item, ','))
            {
                parts.push_back(trim(item));
            }
            return parts;
        }

        bool parseBool(std::string_view value)
        {
            return value == "true" || value == "1" || value == "yes";
        }

        glm::vec4 parseVec4(std::string_view value)
        {
            const auto parts = splitCommaSeparated(value);
            if (parts.size() != 4)
            {
                throw std::runtime_error("Expected four comma-separated values");
            }

            return {
                std::stof(parts[0]),
                std::stof(parts[1]),
                std::stof(parts[2]),
                std::stof(parts[3])};
        }

        PostProcessParameterControlType parseControlType(std::string_view value)
        {
            if (value == "Float")
            {
                return PostProcessParameterControlType::Float;
            }
            if (value == "Color4")
            {
                return PostProcessParameterControlType::Color4;
            }

            throw std::runtime_error("Unknown post process parameter control type");
        }

        PostProcessParameterBinding parseBinding(std::string_view value)
        {
            if (value == "Color")
            {
                return PostProcessParameterBinding::Color;
            }
            if (value == "ParamX")
            {
                return PostProcessParameterBinding::ParamX;
            }
            if (value == "ParamY")
            {
                return PostProcessParameterBinding::ParamY;
            }
            if (value == "ParamZ")
            {
                return PostProcessParameterBinding::ParamZ;
            }
            if (value == "ParamW")
            {
                return PostProcessParameterBinding::ParamW;
            }

            throw std::runtime_error("Unknown post process parameter binding");
        }

        PostProcessEffectDefinition parseEffectDefinitionFile(const std::filesystem::path &filePath)
        {
            PostProcessEffectDefinition definition{};

            std::stringstream stream(FileSystem::readTextFile(filePath.string()));
            std::string line;
            while (std::getline(stream, line))
            {
                line = trim(line);
                if (line.empty() || line.starts_with('#'))
                {
                    continue;
                }

                const size_t separatorIndex = line.find('=');
                if (separatorIndex == std::string::npos)
                {
                    throw std::runtime_error("Invalid post process effect definition line");
                }

                const std::string key = trim(line.substr(0, separatorIndex));
                const std::string value = trim(line.substr(separatorIndex + 1));

                if (key == "id")
                {
                    definition.id = value;
                }
                else if (key == "displayName")
                {
                    definition.displayName = value;
                }
                else if (key == "vertexShader")
                {
                    // Extract filename only — handles both legacy full paths
                    // ("./src/shaders/compiled/X.spv") and portable names ("X.spv").
                    definition.vertexShaderPath =
                        Paths::compiledShader(std::filesystem::path{value}.filename().string()).string();
                }
                else if (key == "fragmentShader")
                {
                    definition.fragmentShaderPath =
                        Paths::compiledShader(std::filesystem::path{value}.filename().string()).string();
                }
                else if (key == "showInEditor")
                {
                    definition.showInEditor = parseBool(value);
                }
                else if (key == "input")
                {
                    const auto parts = splitCommaSeparated(value);
                    if (parts.size() != 2)
                    {
                        throw std::runtime_error("Invalid input definition in post process effect file");
                    }

                    definition.inputs.push_back(PostProcessInputDefinition{parts[0], static_cast<uint32_t>(std::stoul(parts[1]))});
                }
                else if (key == "parameter")
                {
                    const auto parts = splitCommaSeparated(value);
                    if (parts.size() != 7)
                    {
                        throw std::runtime_error("Invalid parameter definition in post process effect file");
                    }

                    definition.parameters.push_back(PostProcessParameterDefinition{
                        parts[0],
                        parts[1],
                        parseControlType(parts[2]),
                        parseBinding(parts[3]),
                        std::stof(parts[4]),
                        std::stof(parts[5]),
                        std::stof(parts[6]),
                    });
                }
                else if (key == "defaultColor")
                {
                    definition.defaultParameters.color = parseVec4(value);
                }
                else if (key == "defaultParams")
                {
                    definition.defaultParameters.params = parseVec4(value);
                }
                else
                {
                    throw std::runtime_error("Unknown post process effect definition property");
                }
            }

            if (definition.id.empty() || definition.displayName.empty() || definition.vertexShaderPath.empty() || definition.fragmentShaderPath.empty())
            {
                throw std::runtime_error("Post process effect definition is missing required fields");
            }

            if (definition.inputs.empty())
            {
                throw std::runtime_error("Post process effect definition must declare at least one input");
            }

            return definition;
        }

        void ensureDefaultDefinitionsRegistered()
        {
            if (definitionsLoaded())
            {
                return;
            }

            std::vector<std::filesystem::path> definitionFiles;
            for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path{kEffectDefinitionsDirectory}))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".ppfx")
                {
                    definitionFiles.push_back(entry.path());
                }
            }

            std::sort(definitionFiles.begin(), definitionFiles.end());
            for (const auto &filePath : definitionFiles)
            {
                registerPostProcessEffectDefinition(parseEffectDefinitionFile(filePath));
            }

            definitionsLoaded() = true;

            if (registry().empty())
            {
                throw std::runtime_error("No post process effect definitions were loaded from data files");
            }
        }
    }

    void registerPostProcessEffectDefinition(PostProcessEffectDefinition definition)
    {
        auto &definitions = registry();
        auto existing = std::find_if(definitions.begin(), definitions.end(), [&](const PostProcessEffectDefinition &candidate)
                                     { return candidate.id == definition.id; });
        if (existing != definitions.end())
        {
            *existing = std::move(definition);
            return;
        }

        definitions.push_back(std::move(definition));
    }

    const std::vector<PostProcessEffectDefinition> &getPostProcessEffectDefinitions()
    {
        ensureDefaultDefinitionsRegistered();
        return registry();
    }

    // Named by reflection from PostProcessEffectComponent::definitionId, so the
    // inspector's effect dropdown is this list, live, with no registration step
    // and no string to keep in sync. The pointers handed to the sink are into
    // the registry's own strings and stay valid for the frame.
    void postProcessEffectOptions(const Ecs::OptionSink &sink)
    {
        for (const PostProcessEffectDefinition &definition : getPostProcessEffectDefinitions())
        {
            if (!definition.showInEditor)
                continue;
            sink.add(definition.id.c_str(), definition.displayName.c_str());
        }
    }

    const PostProcessEffectDefinition *findPostProcessEffectDefinition(std::string_view id)
    {
        const auto &definitions = getPostProcessEffectDefinitions();
        auto iterator = std::find_if(definitions.begin(), definitions.end(), [&](const PostProcessEffectDefinition &definition)
                                     { return definition.id == id; });
        return iterator != definitions.end() ? &*iterator : nullptr;
    }

    const PostProcessEffectDefinition &getPostProcessPresentEffectDefinition()
    {
        static const PostProcessEffectDefinition presentDefinition{
            "present",
            "Present",
            Paths::compiledShader("post_process.vert").string(),
            Paths::compiledShader("post_process.frag").string(),
            {PostProcessInputDefinition{"sourceColor", 0}},
            {},
            PostProcessParameterBlock{},
            false};

        return presentDefinition;
    }

    void reloadPostProcessEffectDefinitions()
    {
        registry().clear();
        definitionsLoaded() = false;
        ensureDefaultDefinitionsRegistered();
    }

    PostProcessEffectComponent makeDefaultPostProcessEffect(std::string_view definitionId)
    {
        const auto *definition = findPostProcessEffectDefinition(definitionId);
        if (definition == nullptr)
        {
            throw std::runtime_error("Unknown post process effect definition id");
        }

        PostProcessEffectComponent effect{};
        effect.definitionId = definition->id;
        effect.parameters = definition->defaultParameters;
        return effect;
    }

    PostProcessStackComponent makeDefaultPostProcessStack()
    {
        PostProcessStackComponent stack{};
        stack.effects.push_back(makeDefaultPostProcessEffect("tint"));
        stack.effects.push_back(makeDefaultPostProcessEffect("tonemap"));
        return stack;
    }

    float *getPostProcessFloatParameter(PostProcessEffectComponent &effect, PostProcessParameterBinding binding)
    {
        switch (binding)
        {
        case PostProcessParameterBinding::ParamX:
            return &effect.parameters.params.x;
        case PostProcessParameterBinding::ParamY:
            return &effect.parameters.params.y;
        case PostProcessParameterBinding::ParamZ:
            return &effect.parameters.params.z;
        case PostProcessParameterBinding::ParamW:
            return &effect.parameters.params.w;
        case PostProcessParameterBinding::Color:
            break;
        }

        return nullptr;
    }
}