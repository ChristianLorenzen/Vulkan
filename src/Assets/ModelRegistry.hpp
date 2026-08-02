#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "Assets/AssetDatabase.hpp"
#include "Assets/AssetId.hpp"
#include "Assets/ModelHandle.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/Resources/PrimitiveType.hpp"

namespace Faye
{
    // Runtime handle cache for models: owns the Model GPU resources and the
    // AssetId <-> handle maps. Identity and metadata records live in the
    // AssetDatabase (which this registry keeps in sync).
    class ModelRegistry
    {
    public:
        static constexpr ModelHandle invalidHandle{};

        explicit ModelRegistry(VulkanDevice &device, AssetDatabase &assetDatabase);

        // Primitive assets are singletons: the same PrimitiveType always maps
        // to the same AssetId and the same handle (dedup).
        ModelHandle createPrimitive(PrimitiveType primitiveType, uint32_t subdivisions = 64);

        std::unique_ptr<Model> makeModelFromFile(const std::string &modelPath);

        ModelHandle registerModel(std::unique_ptr<Model> model);
        // Register with an explicit stable AssetId (transient record by default;
        // callers enrich it via AssetDatabase::registerAsset).
        ModelHandle registerModel(std::unique_ptr<Model> model, AssetId assetId);

        // Import from a source uri, reusing the already-imported model when it
        // is already registered (dedup by deterministic asset id). Throws on
        // import failure (see makeModelFromFile).
        ModelHandle getOrImportByUri(const std::string &modelPath);

        Model *getModel(ModelHandle handle);
        const Model *getModel(ModelHandle handle) const;

        std::optional<AssetId> assetIdOf(ModelHandle handle) const
        {
            const auto it = handleToAsset.find(handle.value);
            return it != handleToAsset.end() ? std::optional<AssetId>{it->second} : std::nullopt;
        }

        std::optional<ModelHandle> findByAssetId(const AssetId &assetId) const
        {
            const auto it = assetToHandle.find(assetId);
            return it != assetToHandle.end() ? std::optional<ModelHandle>{it->second} : std::nullopt;
        }

    private:
        VulkanDevice &device;
        AssetDatabase &assetDatabase;
        uint32_t nextHandleValue = 1;
        std::unordered_map<uint32_t, std::unique_ptr<Model>> models;   // handle.value -> model
        std::unordered_map<AssetId, ModelHandle> assetToHandle;        // assetId -> handle
        std::unordered_map<uint32_t, AssetId> handleToAsset;           // handle.value -> assetId
    };
}
