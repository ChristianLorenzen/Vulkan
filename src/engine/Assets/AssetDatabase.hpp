#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/Assets/AssetId.hpp"
#include "Core/Handles/PrimitiveType.hpp"

namespace Faye
{
    enum class AssetType : uint8_t
    {
        Model = 0,
        Material,
    };

    enum class AssetPersistenceMode : uint8_t
    {
        BuiltIn = 0,   // registered by the engine/editor, stable id
        Imported,      // came from a source file on disk, stable id
        Transient,     // runtime-created, random id, not persisted by default
    };

    // Identity + metadata for one asset. The registries own the runtime
    // resources (Model/Material + handles); this owns the record that scene
    // files persist (the "GUID + list of properties" idea).
    struct AssetRecord
    {
        AssetId id{};
        AssetType type{AssetType::Model};
        std::string name{};
        std::string sourceUri{};          // empty for built-ins/primitives
        std::string primitiveName{};      // set for built-in primitive models ("Cube", ...)
        AssetPersistenceMode persistence{AssetPersistenceMode::Transient};
        bool dirty = false;
    };

    // Source of truth for asset identity and metadata. Pure data — no Vulkan,
    // no GPU resources — so it is usable headless (unit-testable).
    class AssetDatabase
    {
    public:
        // Deterministic v5-style id for a file-sourced asset, stable across
        // runs and machines (no sidecar DB required).
        static AssetId idForSourceUri(const std::string &sourceUri);
        // Deterministic id for a built-in primitive (one asset per PrimitiveType).
        static AssetId idForPrimitive(PrimitiveType primitiveType);
        // Deterministic id for any other built-in (e.g. "Default Material").
        static AssetId idForBuiltIn(const std::string &builtinName);

        // Upsert: replaces any existing record with the same id (so callers
        // can register a lean Transient record first, then enrich it).
        void registerAsset(AssetRecord record);
        bool contains(const AssetId &id) const;
        const AssetRecord *findByAssetId(const AssetId &id) const;
        const AssetRecord *findBySourceUri(const std::string &sourceUri) const;
        std::vector<AssetId> getAllAssetIds() const;
        void setDirty(const AssetId &id, bool dirty);

    private:
        std::vector<AssetId> orderedIds;                       // registration order
        std::unordered_map<AssetId, AssetRecord> recordsById;  // id -> record
        std::unordered_map<std::string, AssetId> idsBySourceUri;  // uri -> id
    };
}
