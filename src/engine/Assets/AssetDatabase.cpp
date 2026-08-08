#include "engine/Assets/AssetDatabase.hpp"
#include "Core/Path/Paths.hpp"

namespace Faye
{
    AssetId AssetDatabase::idForSourceUri(const std::string &sourceUri)
    {
        // Hash the project-relative form, never the caller's. An id derived from
        // "/home/me/Vulkan/assets/ship.fbx" would differ on every machine (and
        // from the same file imported via a relative path), which would strand
        // every scene-file reference to it.
        return Uuid::nameBased(Paths::toProjectRelative(sourceUri));
    }

    AssetId AssetDatabase::idForPrimitive(PrimitiveType primitiveType)
    {
        return Uuid::nameBased(std::string("primitive:") + std::string(primitiveTypeName(primitiveType)));
    }

    AssetId AssetDatabase::idForBuiltIn(const std::string &builtinName)
    {
        return Uuid::nameBased(std::string("builtin:") + builtinName);
    }

    void AssetDatabase::registerAsset(AssetRecord record)
    {
        const AssetId id = record.id;
        const bool isNew = recordsById.find(id) == recordsById.end();
        if (isNew)
            orderedIds.push_back(id);

        recordsById[id] = std::move(record);

        // Stored in the same project-relative form the ids are hashed from, so
        // records survive a round trip through a scene file unchanged.
        if (!recordsById[id].sourceUri.empty())
        {
            recordsById[id].sourceUri = Paths::toProjectRelative(recordsById[id].sourceUri);
            idsBySourceUri[recordsById[id].sourceUri] = id;
        }
    }

    bool AssetDatabase::contains(const AssetId &id) const
    {
        return recordsById.find(id) != recordsById.end();
    }

    const AssetRecord *AssetDatabase::findByAssetId(const AssetId &id) const
    {
        const auto it = recordsById.find(id);
        return it != recordsById.end() ? &it->second : nullptr;
    }

    const AssetRecord *AssetDatabase::findBySourceUri(const std::string &sourceUri) const
    {
        const auto it = idsBySourceUri.find(Paths::toProjectRelative(sourceUri));
        if (it == idsBySourceUri.end())
            return nullptr;
        const auto record = recordsById.find(it->second);
        return record != recordsById.end() ? &record->second : nullptr;
    }

    std::vector<AssetId> AssetDatabase::getAllAssetIds() const
    {
        return orderedIds;
    }

    void AssetDatabase::setDirty(const AssetId &id, bool dirty)
    {
        const auto it = recordsById.find(id);
        if (it != recordsById.end())
            it->second.dirty = dirty;
    }
}
