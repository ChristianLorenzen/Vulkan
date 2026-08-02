#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>

#include "Core/ECS/Entity.hpp"
#include "Core/ECS/SparseSet.hpp"

namespace Faye::Ecs
{
    using ComponentId = uint16_t;

    // ComponentId doubles as a bit position in uint64_t access/component masks
    // (the same idea as the pre-ECS componentBit()/ComponentMask, since
    // deleted along with EntityManager/ComponentKind), hence the cap.
    inline constexpr size_t kMaxComponentTypes = 64;

    namespace detail
    {
        // Debug-only tripwire: set while a stage's systems are in flight. Minting
        // an id there means a component type is being touched for the first time
        // from inside parallel work — legal now that the counter is atomic, but
        // it is the shape that precedes real trouble (a pool created under a
        // reader, see World::poolFor), so we make it loud instead of latent.
        inline std::atomic<int> scheduleDepth{0};

        // The per-T `static const id` below is thread-safe on its own — magic
        // statics guarantee one thread runs each initializer. What is NOT safe
        // is two threads initialising DIFFERENT Ts at once: both land here, and
        // a plain `counter++` can hand them the same id. Two component types
        // then share a bit in both the entity mask and the scheduler's access
        // masks, which silently breaks conflict detection and view matching.
        inline ComponentId nextComponentId()
        {
            static std::atomic<ComponentId> counter{0};
            const ComponentId id = counter.fetch_add(1, std::memory_order_relaxed);
            assert(id < kMaxComponentTypes && "raise kMaxComponentTypes / widen masks");
            assert(scheduleDepth.load(std::memory_order_relaxed) == 0 &&
                   "component id minted while a schedule is running: register the type at startup");
            return id;
        }
    }

    // One id per distinct T, assigned on first use, stable for the rest of the
    // run. NOT stable across runs (assignment order = first-use order):
    // anything persistent must key on the registered NAME, never the id.
    template <class T>
    ComponentId componentId()
    {
        static const ComponentId id = detail::nextComponentId();
        return id;
    }

    template <class T>
    uint64_t componentBit()
    {
        return uint64_t(1) << componentId<T>();
    }

    // The erased interface: only the operations that must work WITHOUT knowing
    // T. Typed access never pays a vtable — World::add<T> knows the concrete
    // pool type and calls SparseSet<T> directly.
    struct IComponentPool
    {
        virtual ~IComponentPool() = default;
        virtual void removeIfPresent(Entity e) = 0;   // destroy-entity sweep
        virtual void *tryGetRaw(Entity e) = 0;        // reflection/editor
        virtual bool contains(Entity e) const = 0;
        virtual size_t size() const = 0;
    };

    template <class T>
    struct ComponentPool final : IComponentPool
    {
        SparseSet<T> set;

        void removeIfPresent(Entity e) override
        {
            if (set.contains(e))
                set.remove(e);
        }
        void *tryGetRaw(Entity e) override { return set.tryGet(e); }
        bool contains(Entity e) const override { return set.contains(e); }
        size_t size() const override { return set.size(); }
    };
}
