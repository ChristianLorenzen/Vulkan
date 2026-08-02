#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

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
            if (!freeList.empty())
            {
                const uint32_t index = freeList.back();
                freeList.pop_back();
                return {index, generations[index]};   // gen was bumped at destroy
            }
            generations.push_back(0);
            return {uint32_t(generations.size() - 1), 0};
        }

        void destroy(Entity e)
        {
            assert(alive(e));
            ++generations[e.index];   // instantly invalidates every outstanding copy
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

        uint32_t slotCount() const { return uint32_t(generations.size()); }
        uint32_t liveCount() const { return uint32_t(generations.size() - freeList.size()); }

    private:
        std::vector<uint32_t> generations;   // generations[i] = current gen of slot i
        std::vector<uint32_t> freeList;      // destroyed slots, LIFO (hot slots stay hot)
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
