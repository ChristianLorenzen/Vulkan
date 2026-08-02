#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "Assets/AssetDatabase.hpp"
#include "Assets/AssetId.hpp"
#include "Material.hpp"
#include "Renderer/Material/MaterialHandle.hpp"

namespace Faye
{
    // Runtime handle cache for materials: owns the Material resources and the
    // AssetId <-> handle maps. Identity and metadata records live in the
    // AssetDatabase (which this registry keeps in sync).
    class MaterialRegistry
    {
    public:
        static constexpr MaterialHandle invalidHandle{};

        explicit MaterialRegistry(AssetDatabase &assetDatabase);

        MaterialHandle registerMaterial(MaterialData materialData, MaterialPipelineConfig pipelineConfig = {});
        // Register with an explicit stable AssetId (transient record by default;
        // callers enrich it via AssetDatabase::registerAsset).
        MaterialHandle registerMaterial(MaterialData materialData, MaterialPipelineConfig pipelineConfig, AssetId assetId);

        MaterialHandle registerMaterial(std::unique_ptr<Material> material);
        MaterialHandle registerMaterial(std::unique_ptr<Material> material, AssetId assetId);

        Material *getMaterial(MaterialHandle handle);
        const Material *getMaterial(MaterialHandle handle) const;

        // Every registered handle, ordered by value. The backing map is
        // unordered, so the editor's material picker would otherwise reshuffle
        // its list every frame.
        std::vector<MaterialHandle> getAllHandles() const;

        // Returns the handle of the built-in white fallback material.
        MaterialHandle getDefaultHandle() const { return defaultHandle; }

        // Returns the material for handle, or the fallback material if the
        // handle is invalid or not registered.
        Material *getMaterialOrDefault(MaterialHandle handle);
        const Material *getMaterialOrDefault(MaterialHandle handle) const;

        std::optional<AssetId> assetIdOf(MaterialHandle handle) const;
        std::optional<MaterialHandle> findByAssetId(const AssetId &assetId) const;

    private:
        AssetDatabase &assetDatabase;
        uint32_t nextHandleValue = 1;
        std::unordered_map<uint32_t, std::unique_ptr<Material>> materials;   // handle.value -> material
        std::unordered_map<AssetId, MaterialHandle> assetToHandle;           // assetId -> handle
        std::unordered_map<uint32_t, AssetId> handleToAsset;                 // handle.value -> assetId
        MaterialHandle defaultHandle{};
    };
}