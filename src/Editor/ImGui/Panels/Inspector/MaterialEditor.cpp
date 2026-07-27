#include "Editor/ImGui/Panels/Inspector/MaterialEditor.hpp"

#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "Editor/ImGui/EditorWidgets.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Material/MaterialTemplate.hpp"
#include "Renderer/Material/TextureLoader.hpp"

#include "imgui.h"
#include "quill/LogMacros.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace Faye
{
    namespace
    {
        // The five slots the shaders actually sample (see the push-constant
        // block in vk_render_system.cpp and MaterialCache::refreshState). Height
        // is a valid TextureType but nothing binds it, so it is not offered.
        constexpr std::array<TextureType, 5> kEditableTextureSlots{
            TextureType::Albedo,
            TextureType::Normal,
            TextureType::Metallic,
            TextureType::Roughness,
            TextureType::AmbientOcclusion};

        constexpr ImVec2 kTextureSlotSize{48.0f, 48.0f};

        const char *textureTypeLabel(TextureType type)
        {
            switch (type)
            {
            case TextureType::Albedo:
                return "Albedo";
            case TextureType::Normal:
                return "Normal";
            case TextureType::Metallic:
                return "Metallic";
            case TextureType::Roughness:
                return "Roughness";
            case TextureType::AmbientOcclusion:
                return "Ambient Occlusion";
            case TextureType::Height:
                return "Height";
            }

            return "Unknown";
        }

        const char *materialAlphaModeLabel(MaterialAlphaMode mode)
        {
            switch (mode)
            {
            case MaterialAlphaMode::Opaque:
                return "Opaque";
            case MaterialAlphaMode::Mask:
                return "Mask";
            }

            return "Unknown";
        }

        const Texture *findTexture(const MaterialData &materialData, TextureType type)
        {
            for (const Texture &texture : materialData.textures)
            {
                if (texture.type == type)
                    return &texture;
            }
            return nullptr;
        }

        void clearTexture(Material &material, TextureType type)
        {
            MaterialData &materialData = material.getMaterialData();
            const auto removed = std::remove_if(
                materialData.textures.begin(), materialData.textures.end(),
                [type](const Texture &texture) { return texture.type == type; });

            if (removed != materialData.textures.end())
            {
                materialData.textures.erase(removed, materialData.textures.end());
                // MaterialCache re-seeds the slot with the fallback texture.
                material.markDirty();
            }
        }

        // One row per shader-sampled slot, always shown so an empty slot is as
        // visible as a filled one. The thumbnail is the button.
        void drawTextureSlots(MaterialHandle handle,
                              Material &material,
                              const ComponentDrawContext::TextureThumbnailFn *thumbnails,
                              TexturePickerPopup *picker)
        {
            const MaterialData &materialData = material.getMaterialData();

            // Each slot is a 48px thumbnail row, so five of them dominate the
            // panel. Collapsed by default; the header carries the filled count
            // so the collapsed state still says whether anything is assigned.
            size_t assignedCount = 0;
            for (const TextureType type : kEditableTextureSlots)
            {
                if (findTexture(materialData, type) != nullptr)
                    ++assignedCount;
            }

            const std::string texturesLabel =
                "Textures (" + std::to_string(assignedCount) + "/" +
                std::to_string(kEditableTextureSlots.size()) + ")###textures";

            if (!EditorUI::subSection(texturesLabel.c_str(), false))
                return;

            for (const TextureType type : kEditableTextureSlots)
            {
                const Texture *texture = findTexture(materialData, type);
                ImGui::PushID(static_cast<int>(type));

                ImTextureID preview = 0;
                if (texture != nullptr && thumbnails != nullptr && *thumbnails)
                {
                    preview = (*thumbnails)(handle, type);
                }

                const bool clicked = preview != 0
                    ? ImGui::ImageButton("##slot", preview, kTextureSlotSize)
                    : ImGui::Button(texture != nullptr ? "?" : "+", kTextureSlotSize);

                if (clicked && picker != nullptr)
                {
                    picker->open(handle, type);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("%s", texture != nullptr && !texture->path.empty()
                                                ? texture->path.c_str()
                                                : "Click to assign a texture");
                }

                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextUnformatted(textureTypeLabel(type));
                if (texture == nullptr)
                {
                    ImGui::TextDisabled("Empty");
                }
                else
                {
                    const std::string fileName = texture->path.empty()
                                                     ? std::string("<embedded>")
                                                     : Paths::getFileName(texture->path);
                    ImGui::TextDisabled("%s", fileName.c_str());
                    if (ImGui::SmallButton("Clear"))
                    {
                        clearTexture(material, type);
                    }
                }
                ImGui::EndGroup();

                ImGui::PopID();
            }
        }
    }

    bool assignTexture(Material &material, TextureType type, const std::filesystem::path &path)
    {
        Texture loaded;
        try
        {
            loaded = loadTextureFromFile(path.string(), type);
        }
        catch (const std::exception &e)
        {
            LOG_WARNING(Logger::get(), "Texture assign failed for {}: {}", path.string(), e.what());
            return false;
        }

        MaterialData &materialData = material.getMaterialData();
        for (Texture &texture : materialData.textures)
        {
            if (texture.type == type)
            {
                texture = std::move(loaded);
                material.markDirty();
                return true;
            }
        }

        materialData.textures.push_back(std::move(loaded));
        material.markDirty();
        return true;
    }

    void drawMaterialProperties(MaterialHandle handle,
                                Material &material,
                                std::string_view usageSummary,
                                const ComponentDrawContext::TextureThumbnailFn *thumbnails,
                                MaterialTemplateRegistry *templateRegistry,
                                TexturePickerPopup *picker)
    {
        MaterialData &materialData = material.getMaterialData();
        bool materialChanged = false;
        std::array<char, 128> nameBuffer{};
        EditorUI::copyNameToBuffer(materialData.name, nameBuffer);

        const std::string handleTooltip =
            "Material handle " + std::to_string(handle.value) +
            (usageSummary.empty() ? std::string{} : ("\nSubmeshes: " + std::string(usageSummary)));

        if (EditorUI::beginProperties("##materialHeader"))
        {
            EditorUI::propertyLabel("Name", handleTooltip.c_str());
            if (ImGui::InputText("##materialName", nameBuffer.data(), nameBuffer.size()))
            {
                materialData.name = nameBuffer.data();
                materialChanged = true;
            }

            // Built-in PBR is index 0; registered templates follow.
            std::vector<std::pair<MaterialTemplateHandle, std::string>> templateItems;
            templateItems.push_back({kBuiltinPBRTemplateHandle, "Built-in PBR"});
            if (templateRegistry != nullptr)
            {
                for (uint32_t h = 1; h <= templateRegistry->count(); ++h)
                {
                    if (const MaterialTemplate *tmpl = templateRegistry->get(h))
                        templateItems.push_back({h, tmpl->name});
                }
            }

            int currentIndex = 0;
            for (int i = 0; i < static_cast<int>(templateItems.size()); ++i)
            {
                if (templateItems[i].first == materialData.templateHandle)
                {
                    currentIndex = i;
                    break;
                }
            }

            const MaterialPipelineConfig &pipelineConfig = material.getPipelineConfig();
            const std::string shaderTooltip =
                "vert: " + pipelineConfig.vertexShaderPath + "\nfrag: " + pipelineConfig.fragmentShaderPath;

            EditorUI::propertyLabel("Shader", shaderTooltip.c_str());
            if (ImGui::BeginCombo("##shaderTemplate", templateItems[currentIndex].second.c_str()))
            {
                for (int i = 0; i < static_cast<int>(templateItems.size()); ++i)
                {
                    const bool selected = (i == currentIndex);
                    if (ImGui::Selectable(templateItems[i].second.c_str(), selected))
                    {
                        materialData.templateHandle = templateItems[i].first;
                        if (templateItems[i].first == kBuiltinPBRTemplateHandle)
                        {
                            material.setPipelineConfig({"shader.vert", "shader.frag"});
                        }
                        else if (templateRegistry != nullptr)
                        {
                            if (const MaterialTemplate *tmpl = templateRegistry->get(templateItems[i].first))
                                material.setPipelineConfig({tmpl->vertShaderPath, tmpl->fragShaderPath});
                        }
                        materialChanged = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            EditorUI::endProperties();
        }

        const MaterialTemplate *activeTemplate = (templateRegistry != nullptr)
                                                     ? templateRegistry->get(materialData.templateHandle)
                                                     : nullptr;

        // Collapsed by default: the full PBR set is fourteen rows, and the
        // name/shader rows above are what you need to identify a material.
        if (EditorUI::subSection("Properties###materialProperties", false) &&
            EditorUI::beginProperties("##materialProperties"))
        {
            if (activeTemplate != nullptr && !activeTemplate->properties.empty())
            {
                // Custom template: only the properties it declares.
                for (const auto &desc : activeTemplate->properties)
                {
                    EditorUI::propertyLabel(desc.label.c_str());
                    if (desc.field == "baseColorFactor" && desc.type == ShaderMember::Type::Vec4)
                    {
                        if (ImGui::ColorEdit4("##baseColorFactor", &materialData.baseColorFactor.x))
                        {
                            materialData.opacity = materialData.baseColorFactor.a;
                            materialChanged = true;
                        }
                    }
                    else if (desc.field == "metallicFactor" && desc.type == ShaderMember::Type::Float)
                    {
                        materialChanged |= ImGui::DragFloat("##metallicFactor", &materialData.metallicFactor,
                                                            0.01f, desc.minVal, desc.maxVal);
                    }
                    else if (desc.field == "roughnessFactor" && desc.type == ShaderMember::Type::Float)
                    {
                        materialChanged |= ImGui::DragFloat("##roughnessFactor", &materialData.roughnessFactor,
                                                            0.01f, desc.minVal, desc.maxVal);
                    }
                    else if (desc.field == "emissive" && desc.type == ShaderMember::Type::Vec3)
                    {
                        materialChanged |= ImGui::ColorEdit3("##emissive", &materialData.emissive.x);
                    }
                    else if (desc.field == "emissiveIntensity" && desc.type == ShaderMember::Type::Float)
                    {
                        materialChanged |= ImGui::DragFloat("##emissiveIntensity", &materialData.emissiveIntensity,
                                                            0.1f, desc.minVal, desc.maxVal);
                    }
                    else if (desc.field == "shininess" && desc.type == ShaderMember::Type::Float)
                    {
                        materialChanged |= ImGui::DragFloat("##shininess", &materialData.shininess,
                                                            1.0f, desc.minVal, desc.maxVal);
                    }
                    else
                    {
                        ImGui::TextDisabled("(unsupported property type)");
                    }
                }
            }
            else
            {
                EditorUI::propertyLabel("Color");
                materialChanged |= ImGui::ColorEdit3("##color", &materialData.color.x);

                EditorUI::propertyLabel("Base Color");
                if (ImGui::ColorEdit4("##baseColor", &materialData.baseColorFactor.x))
                {
                    materialData.opacity = materialData.baseColorFactor.a;
                    materialChanged = true;
                }

                EditorUI::propertyLabel("Metallic");
                materialChanged |= ImGui::DragFloat("##metallic", &materialData.metallicFactor, 0.01f, 0.0f, 1.0f);

                EditorUI::propertyLabel("Roughness");
                materialChanged |= ImGui::DragFloat("##roughness", &materialData.roughnessFactor, 0.01f, 0.0f, 1.0f);

                EditorUI::propertyLabel("Normal Scale");
                materialChanged |= ImGui::DragFloat("##normalScale", &materialData.normalScale, 0.01f, 0.0f, 8.0f);

                EditorUI::propertyLabel("Occlusion");
                materialChanged |= ImGui::DragFloat("##occlusion", &materialData.occlusionStrength, 0.01f, 0.0f, 1.0f);

                EditorUI::propertyLabel("Opacity");
                if (ImGui::DragFloat("##opacity", &materialData.opacity, 0.01f, 0.0f, 1.0f))
                {
                    materialData.baseColorFactor.a = materialData.opacity;
                    materialChanged = true;
                }

                EditorUI::propertyLabel("Alpha Mode");
                int alphaModeIndex = static_cast<int>(materialData.alphaMode);
                const char *alphaModeLabels[] = {
                    materialAlphaModeLabel(MaterialAlphaMode::Opaque),
                    materialAlphaModeLabel(MaterialAlphaMode::Mask)};
                if (ImGui::Combo("##alphaMode", &alphaModeIndex, alphaModeLabels, IM_ARRAYSIZE(alphaModeLabels)))
                {
                    materialData.alphaMode = static_cast<MaterialAlphaMode>(alphaModeIndex);
                    materialChanged = true;
                }

                if (materialData.alphaMode == MaterialAlphaMode::Mask)
                {
                    EditorUI::propertyLabel("Alpha Cutoff");
                    materialChanged |= ImGui::DragFloat("##alphaCutoff", &materialData.alphaCutoff, 0.01f, 0.0f, 1.0f);
                }

                EditorUI::propertyLabel("Shininess");
                materialChanged |= ImGui::DragFloat("##shininess", &materialData.shininess, 0.1f, 0.0f, 256.0f);

                EditorUI::propertyLabel("Emissive");
                materialChanged |= ImGui::ColorEdit3("##emissive", &materialData.emissive.x);

                EditorUI::propertyLabel("Emissive Intensity");
                materialChanged |= ImGui::DragFloat("##emissiveIntensity", &materialData.emissiveIntensity, 0.01f, 0.0f, 10.0f);

                EditorUI::propertyLabel("Double Sided");
                materialChanged |= ImGui::Checkbox("##doubleSided", &materialData.doubleSided);
            }

            EditorUI::endProperties();
        }

        if (materialChanged)
        {
            // Bumps the revision, which is what makes MaterialCache rebuild
            // the parameter UBO and bindless slots on the next frame.
            material.markDirty();
        }

        drawTextureSlots(handle, material, thumbnails, picker);
    }

    void drawMaterialEntry(const char *label,
                           MaterialHandle handle,
                           MaterialRegistry *materialRegistry,
                           std::string_view usageSummary,
                           const ComponentDrawContext::TextureThumbnailFn *thumbnails,
                           MaterialTemplateRegistry *templateRegistry,
                           TexturePickerPopup *picker)
    {
        ImGui::PushID(static_cast<int>(handle.value));

        Material *material = materialRegistry->getMaterial(handle);
        const char *materialName = material != nullptr && !material->getName().empty()
                                       ? material->getName().c_str()
                                       : "Unnamed Material";

        if (ImGui::TreeNodeEx("##MaterialEntry", ImGuiTreeNodeFlags_DefaultOpen, "%s: %s", label, materialName))
        {
            if (material == nullptr)
            {
                ImGui::TextDisabled("Material %u is missing from the registry.", handle.value);
            }
            else
            {
                drawMaterialProperties(handle, *material, usageSummary, thumbnails, templateRegistry, picker);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}
