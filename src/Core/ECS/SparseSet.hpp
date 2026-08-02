#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "Core/ECS/Entity.hpp"

namespace Faye::Ecs
{
    // Per-component-type storage. Pools are keyed by entity INDEX only —
    // liveness (generation) is the World's job; it checks alive(e) before
    // touching any pool.
    //
    // Invariants (the debug asserts catch every bug in this class):
    //   1. denseEntities.size() == components.size() always.
    //   2. For every dense slot s: sparse[denseEntities[s]] == s.
    //   3. No holes: dense slots [0, size) are all live.
    template <class T>
    class SparseSet
    {
    public:
        static constexpr size_t kPageSize = 1024;         // entity indices per page
        static constexpr uint32_t kTombstone = 0xFFFFFFFF;

        bool contains(Entity e) const
        {
            const uint32_t *slot = sparseSlotFor(e.index);
            return slot != nullptr && *slot != kTombstone;
        }

        T &emplace(Entity e, T &&value)
        {
            uint32_t *slot = sparseSlotForCreate(e.index);
            assert(*slot == kTombstone && "component already present on this entity");
            *slot = uint32_t(denseEntities.size());       // new dense slot = end
            denseEntities.push_back(e.index);
            components.push_back(std::move(value));
            return components.back();
        }

        T *tryGet(Entity e)
        {
            const uint32_t *slot = sparseSlotFor(e.index);
            if (slot == nullptr || *slot == kTombstone)
                return nullptr;
            return &components[*slot];
        }

        const T *tryGet(Entity e) const
        {
            const uint32_t *slot = sparseSlotFor(e.index);
            if (slot == nullptr || *slot == kTombstone)
                return nullptr;
            return &components[*slot];
        }

        void remove(Entity e)                             // swap-and-pop
        {
            uint32_t *slot = sparseSlotFor(e.index);
            assert(slot != nullptr && *slot != kTombstone && "removing a component that is not present");
            const uint32_t removedSlot = *slot;
            const uint32_t lastSlot = uint32_t(denseEntities.size() - 1);

            if (removedSlot != lastSlot)
            {
                // 1. Move the last element into the hole.
                denseEntities[removedSlot] = denseEntities[lastSlot];
                components[removedSlot] = std::move(components[lastSlot]);
                // 2. The moved entity's sparse entry now points at its new home.
                *sparseSlotFor(denseEntities[removedSlot]) = removedSlot;
            }
            // 3. Shrink, and tombstone the removed entity.
            denseEntities.pop_back();
            components.pop_back();
            *slot = kTombstone;
        }

        // Dense views: the reason parallelFor over a component type is trivial.
        std::span<T> raw() { return components; }
        std::span<const T> raw() const { return components; }
        std::span<const uint32_t> entities() const { return denseEntities; }

        size_t size() const { return components.size(); }
        bool empty() const { return components.empty(); }

        void clear()
        {
            for (auto &page : sparsePages)
                if (page)
                    page->fill(kTombstone);
            denseEntities.clear();
            components.clear();
        }

    private:
        const uint32_t *sparseSlotFor(uint32_t entityIndex) const
        {
            const size_t page = entityIndex / kPageSize;
            if (page >= sparsePages.size() || !sparsePages[page])
                return nullptr;
            return &(*sparsePages[page])[entityIndex % kPageSize];
        }

        uint32_t *sparseSlotFor(uint32_t entityIndex)
        {
            return const_cast<uint32_t *>(std::as_const(*this).sparseSlotFor(entityIndex));
        }

        uint32_t *sparseSlotForCreate(uint32_t entityIndex)
        {
            const size_t page = entityIndex / kPageSize;
            if (page >= sparsePages.size())
                sparsePages.resize(page + 1);
            if (!sparsePages[page])
            {
                sparsePages[page] = std::make_unique<std::array<uint32_t, kPageSize>>();
                sparsePages[page]->fill(kTombstone);
            }
            return &(*sparsePages[page])[entityIndex % kPageSize];
        }

        // sparse, paged:  entity index -> dense slot (or tombstone)
        std::vector<std::unique_ptr<std::array<uint32_t, kPageSize>>> sparsePages;
        // dense, packed, parallel:
        std::vector<uint32_t> denseEntities;   // dense slot -> entity index
        std::vector<T> components;             // dense slot -> component data
    };
}
