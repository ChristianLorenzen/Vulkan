#include "ModelRegistry.hpp"

#include <filesystem>
#include <stdexcept>

#include "Core/Path/Paths.hpp"

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
        .primitiveName = std::string(primitiveTypeName(primitiveType)),
        .persistence = AssetPersistenceMode::BuiltIn,
    });
    return handle;
}

Faye::ModelHandle Faye::ModelRegistry::recreatePrimitive(PrimitiveType primitiveType, uint32_t subdivisions)
{
    const AssetId assetId = AssetDatabase::idForPrimitive(primitiveType);
    const ModelHandle handle = registerModel(Model::createPrimitive(device, primitiveType, subdivisions), assetId);
    assetDatabase.registerAsset(AssetRecord{
        .id = assetId,
        .type = AssetType::Model,
        .name = std::string(primitiveTypeName(primitiveType)),
        .primitiveName = std::string(primitiveTypeName(primitiveType)),
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
    // The uri is the asset's portable identity, so it is stored (and hashed)
    // project-relative regardless of what the caller passed; only the file read
    // itself needs the absolute path.
    const std::string sourceUri = Paths::toProjectRelative(modelPath);
    const AssetId assetId = AssetDatabase::idForSourceUri(sourceUri);
    if (const auto existing = findByAssetId(assetId))
        return *existing;

    std::unique_ptr<Model> model = makeModelFromFile(Paths::fromProjectRelative(sourceUri));   // throws on failure

    const ModelHandle handle = registerModel(std::move(model), assetId);
    assetDatabase.registerAsset(AssetRecord{
        .id = assetId,
        .type = AssetType::Model,
        .name = std::filesystem::path(sourceUri).filename().string(),
        .sourceUri = sourceUri,
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