#include "ModelRegistry.hpp"

#include <filesystem>
#include <stdexcept>

using namespace Faye;

Faye::ModelRegistry::ModelRegistry(VulkanDevice &device, AssetDatabase &assetDatabase)
    : device(device), assetDatabase(assetDatabase) {}

Faye::ModelHandle Faye::ModelRegistry::createPrimitive(PrimitiveType primitiveType, uint32_t subdivisions)
{
    // Primitive assets are singletons: one AssetId per PrimitiveType, and the
    // first creation wins. Ensures scene files can reference "the cube" stably.
    const AssetId assetId = AssetDatabase::idForPrimitive(primitiveType);
    if (const auto existing = findByAssetId(assetId))
        return *existing;

    const ModelHandle handle = registerModel(Model::createPrimitive(device, primitiveType, subdivisions), assetId);

    // Enrich the lean transient record (registered by registerModel) with the
    // built-in name + persistence mode.
    assetDatabase.registerAsset(AssetRecord{
        .id = assetId,
        .type = AssetType::Model,
        .name = std::string(primitiveTypeName(primitiveType)),
        .persistence = AssetPersistenceMode::BuiltIn,
    });
    return handle;
}

std::unique_ptr<Faye::Model> Faye::ModelRegistry::makeModelFromFile(const std::string &modelPath)
{
    return Model::createModelFromFile(device, modelPath);
}

Faye::ModelHandle Faye::ModelRegistry::registerModel(std::unique_ptr<Model> model, AssetId assetId)
{
    if (model == nullptr)
    {
        throw std::runtime_error("Cannot register a null model in ModelRegistry");
    }

    const ModelHandle handle{nextHandleValue++};
    models.emplace(handle.value, std::move(model));
    assetToHandle[assetId] = handle;
    handleToAsset[handle.value] = assetId;

    // Lean transient record; callers upsert richer metadata (name, source uri,
    // persistence) through AssetDatabase::registerAsset afterwards.
    assetDatabase.registerAsset(AssetRecord{
        .id = assetId,
        .type = AssetType::Model,
        .persistence = AssetPersistenceMode::Transient,
    });
    return handle;
}

Faye::ModelHandle Faye::ModelRegistry::registerModel(std::unique_ptr<Model> model)
{
    return registerModel(std::move(model), Uuid::generateV4());
}

Faye::ModelHandle Faye::ModelRegistry::getOrImportByUri(const std::string &modelPath)
{
    const AssetId assetId = AssetDatabase::idForSourceUri(modelPath);
    if (const auto existing = findByAssetId(assetId))
        return *existing;

    std::unique_ptr<Model> model = makeModelFromFile(modelPath);   // throws on failure

    const ModelHandle handle = registerModel(std::move(model), assetId);
    assetDatabase.registerAsset(AssetRecord{
        .id = assetId,
        .type = AssetType::Model,
        .name = std::filesystem::path(modelPath).filename().string(),
        .sourceUri = modelPath,
        .persistence = AssetPersistenceMode::Imported,
    });
    return handle;
}

Faye::Model *Faye::ModelRegistry::getModel(ModelHandle handle)
{
    auto iterator = models.find(handle.value);
    return iterator != models.end() ? iterator->second.get() : nullptr;
}

const Faye::Model *Faye::ModelRegistry::getModel(ModelHandle handle) const
{
    auto iterator = models.find(handle.value);
    return iterator != models.end() ? iterator->second.get() : nullptr;
}

std::optional<Faye::AssetId> Faye::ModelRegistry::assetIdOf(ModelHandle handle) const
{
    const auto it = handleToAsset.find(handle.value);
    return it != handleToAsset.end() ? std::optional<AssetId>{it->second} : std::nullopt;
}

std::optional<Faye::ModelHandle> Faye::ModelRegistry::findByAssetId(const AssetId &assetId) const
{
    const auto it = assetToHandle.find(assetId);
    return it != assetToHandle.end() ? std::optional<ModelHandle>{it->second} : std::nullopt;
}