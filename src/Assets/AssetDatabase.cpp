#include "Assets/AssetDatabase.hpp"

namespace Faye
{
    AssetId AssetDatabase::idForSourceUri(const std::string &sourceUri)
    {
        return Uuid::nameBased(sourceUri);
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

        if (!recordsById[id].sourceUri.empty())
            idsBySourceUri[recordsById[id].sourceUri] = id;
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
        const auto it = idsBySourceUri.find(sourceUri);
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
