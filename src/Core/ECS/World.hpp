#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "Core/ECS/AccessCheck.hpp"
#include "Core/ECS/ComponentPool.hpp"
#include "Core/ECS/Entity.hpp"

namespace Faye::Ecs
{
    class World;

    // ---- Component type reflection (headless — no ImGui, ever) -----------
    // Core keeps this runtime table of component types; the editor keeps a
    // separate ComponentId -> draw-function table. The two join by id at
    // runtime, which is what lets the inspector enumerate arbitrary types
    // while core stays free of editor includes.

    // Reserved for later save/load; declared now so ComponentTypeInfo's shape
    // doesn't churn when serialization lands.
    class Serializer;
    class Deserializer;

    enum class Clone { copy, skip };

    struct ComponentTypeInfo
    {
        ComponentId id = 0;
        const char *name = nullptr;

        // Type-erased operations the editor (and save/load) need without
        // knowing T. Plain function pointers: each is a captureless lambda
        // stamped out per T at registration — the type lives in the CODE of
        // the lambda, not in any data.
        void (*addDefault)(World &, Entity) = nullptr;   // "Add Component" menu item
        void (*remove)(World &, Entity) = nullptr;       // "Remove Component" button
        void *(*tryGetRaw)(World &, Entity) = nullptr;   // component ptr for drawing
        bool (*has)(World &, Entity) = nullptr;
        void (*copyTo)(World &, Entity, Entity) = nullptr;  // for duplication

        void (*serialize)(const void *component, Serializer &) = nullptr;     // reserved
        void (*deserialize)(void *component, Deserializer &) = nullptr;       // reserved
    };

    class ComponentTypeRegistry
    {
    public:
        // Defined after World (the per-type lambdas call World methods).
        // Names are for humans and persistence: ComponentId is first-use
        // ordered and unstable across runs, so anything persistent must key
        // on the registered name, never the id.
        // Clone is a TEMPLATE argument, not a runtime one: Clone::skip must
        // stop the copy from being instantiated at all, which is the only way
        // a move-only component (CameraComponent) can be registered.
        template <class T, Clone C = Clone::copy>
        void registerType(const char *name);

        const ComponentTypeInfo &info(ComponentId id) const { return types[id]; }
        std::span<const ComponentTypeInfo> all() const { return types; }

    private:
        std::vector<ComponentTypeInfo> types;   // indexed by ComponentId; gaps have name == nullptr
    };

    // Multi-component query; implementation after World (it needs the full type).
    //
    // A type may be const-qualified: view<const Transform, Velocity> yields
    // (Entity, const Transform&, Velocity&). Constness is not cosmetic — it
    // selects which AccessCheck fires, so a system declaring read<Transform>
    // must spell it view<const Transform> or abort. That is what lets the
    // scheduler trust reader/reader non-conflict.
    template <class... Ts>
    class View
    {
    public:
        explicit View(World &world) : world(world) {}

        // fn(Entity, Ts&...). CONTRACT: no structural changes (add/remove/
        // create/destroy) from inside fn — mutate component VALUES only.
        // Buffer structural ops and apply after the loop (the system
        // scheduling layer automates this with CommandBuffer).
        template <class Fn>
        void each(Fn &&fn);

    private:
        World &world;
    };

    class World
    {
    public:
        // ---- entities ---------------------------------------------------
        Entity create()
        {
            const Entity e = entities.create();
            if (e.index >= componentMasks.size())
                componentMasks.resize(e.index + 1, 0);
            componentMasks[e.index] = 0;
            return e;
        }

        void destroy(Entity e)
        {
            assert(alive(e));
            for (const auto &callback : onDestroyCallbacks)
                callback(*this, e);
            // The erased sweep: adding component type #10 requires ZERO edits
            // here. This structurally kills the "forgot to erase one map"
            // leak class the pre-ECS EntityManager::destroyEntity had.
            for (size_t id = 0; id < pools.size(); ++id)
            {
                if (!pools[id] || !pools[id]->contains(e))
                    continue;
                fireRemoveHook(ComponentId(id), e);
                pools[id]->removeIfPresent(e);
            }
            componentMasks[e.index] = 0;
            entities.destroy(e);   // generation bump; stale handles die here
        }

        bool alive(Entity e) const { return entities.alive(e); }

        // ---- components -------------------------------------------------
        template <class T, class... Args>
        T &add(Entity e, Args &&...args)
        {
            assert(alive(e));
            T &component = poolFor<T>().set.emplace(e, T{std::forward<Args>(args)...});
            componentMasks[e.index] |= componentBit<T>();
            return component;
        }

        template <class T>
        void remove(Entity e)
        {
            assert(alive(e));
            assert(has<T>(e));
            fireRemoveHook(componentId<T>(), e);   // BEFORE the data dies:
            poolFor<T>().set.remove(e);            // teardown needs the component intact
            componentMasks[e.index] &= ~componentBit<T>();
        }

        // Hands out a MUTABLE reference, so a system must have declared write<T>
        // — see checkComponentWrite. Read-only callers inside a system want
        // tryRead() or view<const T> instead.
        template <class T>
        T *tryGet(Entity e)
        {
#if FAYE_ECS_CHECK_ACCESS
            checkComponentWrite(componentBit<T>());   // mutable views iterate through here
#endif
            if (!alive(e))
                return nullptr;
            ComponentPool<T> *pool = poolIfExists<T>();
            return pool ? pool->set.tryGet(e) : nullptr;
        }

        template <class T>
        const T *tryGet(Entity e) const
        {
#if FAYE_ECS_CHECK_ACCESS
            checkComponentRead(componentBit<T>());
#endif
            if (!alive(e))
                return nullptr;
            const ComponentPool<T> *pool = poolIfExists<T>();
            return pool ? pool->set.tryGet(e) : nullptr;
        }

        // Explicit read-only access from a non-const World. Systems hold
        // World& but most only read, and overload resolution would otherwise
        // silently pick the mutable tryGet and demand write<T>.
        template <class T>
        const T *tryRead(Entity e) const
        {
            return tryGet<T>(e);   // const overload: read-checked
        }

        template <class T>
        bool has(Entity e) const
        {
            return alive(e) && (componentMasks[e.index] & componentBit<T>()) != 0;
        }

        template <class... Ts>
        View<Ts...> view()
        {
            return View<Ts...>(*this);
        }

        // ---- lifecycle hooks (the channel scripts need) -----------------
        using RemoveHook = std::function<void(World &, Entity, void *component)>;

        template <class T>
        void setRemoveHook(RemoveHook hook)
        {
            const ComponentId id = componentId<T>();
            if (id >= removeHooks.size())
                removeHooks.resize(id + 1);
            removeHooks[id] = std::move(hook);
        }

        void addOnDestroy(std::function<void(World &, Entity)> callback)
        {
            onDestroyCallbacks.push_back(std::move(callback));
        }

        // ---- component type reflection ----------------------------------
        ComponentTypeRegistry &types() { return typeRegistry; }
        const ComponentTypeRegistry &types() const { return typeRegistry; }

        // ---- low-level access (views, reflection, extraction) -----------
        uint64_t maskOf(uint32_t entityIndex) const { return componentMasks[entityIndex]; }
        Entity entityAt(uint32_t entityIndex) const { return entities.handleFor(entityIndex); }
        const EntityRegistry &registry() const { return entities; }

        template <class T>
        ComponentPool<T> &poolFor()
        {
            const ComponentId id = componentId<T>();
            if (id >= pools.size())
                pools.resize(id + 1);
            if (!pools[id])
                pools[id] = std::make_unique<ComponentPool<T>>();
            return static_cast<ComponentPool<T> &>(*pools[id]);
        }

        template <class T>
        ComponentPool<T> *poolIfExists()
        {
            const ComponentId id = componentId<T>();
            if (id >= pools.size() || !pools[id])
                return nullptr;
            return static_cast<ComponentPool<T> *>(pools[id].get());
        }

        template <class T>
        const ComponentPool<T> *poolIfExists() const
        {
            const ComponentId id = componentId<T>();
            if (id >= pools.size() || !pools[id])
                return nullptr;
            return static_cast<const ComponentPool<T> *>(pools[id].get());
        }

    private:
        void fireRemoveHook(ComponentId id, Entity e)
        {
            if (id < removeHooks.size() && removeHooks[id])
                removeHooks[id](*this, e, pools[id]->tryGetRaw(e));
        }

        EntityRegistry entities;
        std::vector<std::unique_ptr<IComponentPool>> pools;   // index = ComponentId
        std::vector<uint64_t> componentMasks;                 // index = entity index; bit = ComponentId
        std::vector<RemoveHook> removeHooks;                  // index = ComponentId
        std::vector<std::function<void(World &, Entity)>> onDestroyCallbacks;
        ComponentTypeRegistry typeRegistry;
    };

    // ---- ComponentTypeRegistry implementation ----------------------------

    // Yields the per-T copy thunk, or nullptr for Clone::skip. The branch must
    // be `if constexpr`: a captured `clone` flag would make the lambda non-
    // captureless (no conversion to a function pointer) AND would still
    // instantiate world.add<T>(dst, *c) for skipped types, which is exactly
    // what a move-only component cannot survive.
    template <class T, Clone C>
    constexpr auto makeCopyTo() -> void (*)(World &, Entity, Entity)
    {
        if constexpr (C == Clone::copy)
        {
            static_assert(std::is_copy_constructible_v<T>,
                          "component registered with Clone::copy must be copy-constructible: "
                          "register it with Clone::skip, or make the type copyable");
            return [](World &world, Entity src, Entity dst)
            {
                if (const T *component = world.tryRead<T>(src))
                    world.add<T>(dst, *component);
            };
        }
        else
        {
            // Callers MUST null-check copyTo; skipped types have no thunk.
            return nullptr;
        }
    }

    // Out of line because the lambdas need World's full definition.
    template <class T, Clone C>
    void ComponentTypeRegistry::registerType(const char *name)
    {
        const ComponentId id = componentId<T>();
        if (id >= types.size())
            types.resize(id + 1);
        types[id] = ComponentTypeInfo{
            .id = id,
            .name = name,
            .addDefault = [](World &world, Entity e) { world.add<T>(e); },
            .remove = [](World &world, Entity e) { world.remove<T>(e); },
            .tryGetRaw = [](World &world, Entity e) -> void * { return world.tryGet<T>(e); },
            .has = [](World &world, Entity e) { return world.has<T>(e); },
            .copyTo = makeCopyTo<T, C>(),
        };
    }

    // ---- View implementation ---------------------------------------------

    // Read-only accessor for a pool's dense entity list. Returns an empty span
    // when the type has no pool yet, so callers never have to create one just
    // to discover it is empty. See the note in View::each.
    template <class T>
    inline std::span<const uint32_t> denseEntitiesOf(World &world)
    {
        const ComponentPool<T> *pool = world.poolIfExists<T>();
        return pool ? pool->set.entities() : std::span<const uint32_t>{};
    }

    // Fetches one component for View::each with the right constness, and
    // therefore the right AccessCheck: a const T reaches the read-checked
    // accessor, a mutable T the write-checked one.
    template <class T>
    inline auto *viewComponentOf(World &world, Entity e)
    {
        if constexpr (std::is_const_v<T>)
            return world.tryRead<std::remove_const_t<T>>(e);
        else
            return world.tryGet<T>(e);
    }

    template <class... Ts>
    template <class Fn>
    void View<Ts...>::each(Fn &&fn)
    {
        static_assert(sizeof...(Ts) > 0, "a view needs at least one component type");

        // Every id/pool lookup strips const: componentId<const T> is a DIFFERENT
        // static from componentId<T>, so leaving it on would mint a second id
        // for the same component and corrupt every mask it appears in.
        const uint64_t requiredMask = (componentBit<std::remove_const_t<Ts>>() | ...);

        // Pick the smallest pool as the driver: it bounds the work. If 200
        // entities have Transform but 5 have Water, view<Transform, Water>
        // must walk 5, not 200.
        //
        // poolIfExists, NOT poolFor: iterating must never mutate the World.
        // poolFor lazily resizes `pools` and inserts, so a read-only view over
        // a not-yet-created type would reallocate shared state underneath the
        // other systems reading it through tryGet — an unsynchronised race the
        // access masks cannot see, because it is pool-vector mutation rather
        // than a component touch. A missing pool yields an empty span, which
        // wins the smallest-pool contest and correctly visits nothing.
        const std::array<std::span<const uint32_t>, sizeof...(Ts)> entitySpans{
            denseEntitiesOf<std::remove_const_t<Ts>>(world)...};
        size_t driver = 0;
        for (size_t i = 1; i < entitySpans.size(); ++i)
            if (entitySpans[i].size() < entitySpans[driver].size())
                driver = i;

        for (const uint32_t entityIndex : entitySpans[driver])
        {
            // One AND rejects non-matches instead of N-1 paged sparse probes.
            if ((world.maskOf(entityIndex) & requiredMask) != requiredMask)
                continue;
            // Mask passed => every lookup below succeeds (single-threaded).
            const Entity e = world.entityAt(entityIndex);
            fn(e, *viewComponentOf<Ts>(world, e)...);
        }
    }
}
