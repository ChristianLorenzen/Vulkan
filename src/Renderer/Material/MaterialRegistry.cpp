#include "MaterialRegistry.hpp"

#include <algorithm>
#include <stdexcept>

using namespace Faye;

Faye::MaterialRegistry::MaterialRegistry(AssetDatabase &assetDatabase)
    : assetDatabase(assetDatabase)
{
    MaterialData fallback{};
    fallback.name = "Default Material";
    fallback.color = {1.0f, 1.0f, 1.0f};
    fallback.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    fallback.metallicFactor = 0.0f;
    fallback.roughnessFactor = 1.0f;
    const AssetId defaultAssetId = AssetDatabase::idForBuiltIn("Default Material");
    defaultHandle = registerMaterial(std::make_unique<Material>(std::move(fallback)), defaultAssetId);
    assetDatabase.registerAsset(AssetRecord{
        .id = defaultAssetId,
        .type = AssetType::Material,
        .name = "Default Material",
        .persistence = AssetPersistenceMode::BuiltIn,
    });
}

Faye::MaterialHandle Faye::MaterialRegistry::registerMaterial(MaterialData materialData, MaterialPipelineConfig pipelineConfig, AssetId assetId)
{
    return registerMaterial(std::make_unique<Material>(std::move(materialData), std::move(pipelineConfig)), assetId);
}

Faye::MaterialHandle Faye::MaterialRegistry::registerMaterial(MaterialData materialData, MaterialPipelineConfig pipelineConfig)
{
    return registerMaterial(std::make_unique<Material>(std::move(materialData), std::move(pipelineConfig)), Uuid::generateV4());
}

Faye::MaterialHandle Faye::MaterialRegistry::registerMaterial(std::unique_ptr<Faye::Material> material, AssetId assetId)
{
    if (material == nullptr)
    {
        throw std::runtime_error("Cannot register a null material in MaterialRegistry");
    }

    const MaterialHandle handle{nextHandleValue++};
    materials.emplace(handle.value, std::move(material));
    assetToHandle[assetId] = handle;
    handleToAsset[handle.value] = assetId;

    assetDatabase.registerAsset(AssetRecord{
        .id = assetId,
        .type = AssetType::Material,
        .persistence = AssetPersistenceMode::Transient,
    });
    return handle;
}

Faye::MaterialHandle Faye::MaterialRegistry::registerMaterial(std::unique_ptr<Faye::Material> material)
{
    return registerMaterial(std::move(material), Uuid::generateV4());
}

Faye::Material *Faye::MaterialRegistry::getMaterial(MaterialHandle handle)
{
    auto iterator = materials.find(handle.value);
    return iterator != materials.end() ? iterator->second.get() : nullptr;
}

const Faye::Material *Faye::MaterialRegistry::getMaterial(MaterialHandle handle) const
{
    auto iterator = materials.find(handle.value);
    return iterator != materials.end() ? iterator->second.get() : nullptr;
}

std::vector<Faye::MaterialHandle> Faye::MaterialRegistry::getAllHandles() const
{
    std::vector<MaterialHandle> handles;
    handles.reserve(materials.size());
    for (const auto &[value, material] : materials)
    {
        handles.push_back(MaterialHandle{value});
    }

    std::sort(handles.begin(), handles.end(), [](MaterialHandle a, MaterialHandle b) {
        return a.value < b.value;
    });
    return handles;
}

Faye::Material *Faye::MaterialRegistry::getMaterialOrDefault(MaterialHandle handle)
{
    if (Material *material = getMaterial(handle))
    {
        return material;
    }
    return getMaterial(defaultHandle);
}

const Faye::Material *Faye::MaterialRegistry::getMaterialOrDefault(MaterialHandle handle) const
{
    if (const Material *material = getMaterial(handle))
    {
        return material;
    }
    return getMaterial(defaultHandle);
}

std::optional<Faye::AssetId> Faye::MaterialRegistry::assetIdOf(MaterialHandle handle) const
{
    const auto it = handleToAsset.find(handle.value);
    return it != handleToAsset.end() ? std::optional<AssetId>{it->second} : std::nullopt;
}

std::optional<Faye::MaterialHandle> Faye::MaterialRegistry::findByAssetId(const AssetId &assetId) const
{
    const auto it = assetToHandle.find(assetId);
    return it != assetToHandle.end() ? std::optional<MaterialHandle>{it->second} : std::nullopt;
}
