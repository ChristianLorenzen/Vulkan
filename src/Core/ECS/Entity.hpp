#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "Core/Serialization/Uuid.hpp"

namespace Faye::Ecs
{
    struct Entity
    {
        uint32_t index = 0xFFFFFFFF;    // slot number; dense, recyclable
        uint32_t generation = 0;        // which "lifetime" of that slot

        static constexpr Entity null() { return {}; }
        bool isNull() const { return index == 0xFFFFFFFF; }
        bool operator==(const Entity &) const = default;
    };

    class EntityRegistry
    {
    public:
        Entity create()
        {
            return createInternal(Uuid::generateV4());
        }

        // Create an entity with a specific GUID (scene load). If the GUID is
        // already live, a fresh random one is minted instead so uniqueness
        // never breaks.
        Entity createWithGuid(Uuid guid)
        {
            if (byGuid.find(guid) != byGuid.end())
                guid = Uuid::generateV4();
            return createInternal(guid);
        }

        void destroy(Entity e)
        {
            assert(alive(e));
            byGuid.erase(guids[e.index]);   // GUID dies with the slot
            ++generations[e.index];         // instantly invalidates every outstanding copy
            freeList.push_back(e.index);
        }

        bool alive(Entity e) const
        {
            return e.index < generations.size() && generations[e.index] == e.generation;
        }

        // Reconstitute a full handle from a bare index (used by View iteration,
        // which walks pools of indices). Only valid for indices of live slots.
        Entity handleFor(uint32_t index) const
        {
            assert(index < generations.size());
            return {index, generations[index]};
        }

        // Stable identity for save/load. Returns nullopt for dead handles.
        std::optional<Uuid> guidOf(Entity e) const
        {
            if (!alive(e))
                return std::nullopt;
            return guids[e.index];
        }

        // Reverse lookup for scene load (persisted GUID -> live handle).
        std::optional<Entity> findByGuid(Uuid guid) const
        {
            const auto it = byGuid.find(guid);
            if (it == byGuid.end())
                return std::nullopt;
            return it->second;
        }

        uint32_t slotCount() const { return uint32_t(generations.size()); }
        uint32_t liveCount() const { return uint32_t(generations.size() - freeList.size()); }

    private:
        Entity createInternal(Uuid guid)
        {
            if (!freeList.empty())
            {
                const uint32_t index = freeList.back();
                freeList.pop_back();
                guids[index] = guid;                       // gen was bumped at destroy
                byGuid.emplace(guid, Entity{index, generations[index]});
                return {index, generations[index]};
            }
            generations.push_back(0);
            guids.push_back(guid);
            byGuid.emplace(guid, Entity{uint32_t(generations.size() - 1), 0});
            return {uint32_t(generations.size() - 1), 0};
        }

        std::vector<uint32_t> generations;   // generations[i] = current gen of slot i
        std::vector<Uuid> guids;             // guids[i] = stable GUID of slot i (parallel to generations)
        std::vector<uint32_t> freeList;      // destroyed slots, LIFO (hot slots stay hot)
        std::unordered_map<Uuid, Entity> byGuid;   // GUID -> live handle (scene load)
    };
}

// Lets Entity key an unordered_map (extraction history, script lookups, ...).
template <>
struct std::hash<Faye::Ecs::Entity>
{
    size_t operator()(const Faye::Ecs::Entity &e) const noexcept
    {
        return (size_t(e.generation) << 32) ^ size_t(e.index);
    }
};
