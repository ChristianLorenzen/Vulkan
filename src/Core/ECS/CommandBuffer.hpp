#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "Core/ECS/World.hpp"

namespace Faye::Ecs
{
    // Records structural changes (create/destroy/add/remove) during a stage; a
    // single thread replays them at the stage barrier, in recording order
    // (deterministic). During a stage the World's SHAPE is frozen so concurrent
    // View walks and dense arrays never move under a running system — the
    // keystone that makes parallel iteration safe (flecs/DOTS/Bevy do the same).
    //
    // Flush is tolerant by design: destroying an already-dead entity or removing
    // an absent component is a silent no-op, and an add to a dead entity is
    // dropped. Anything stricter turns benign cross-system races ("two systems
    // both despawn e") into crashes.
    //
    // v1 constraint: values recorded via add() must be copyable (std::function
    // requires a copyable closure).
    class CommandBuffer
    {
    public:
        static constexpr uint32_t kPlaceholderGeneration = 0xFFFFFFFF;

        // Returns a PLACEHOLDER usable in later commands of the SAME buffer;
        // flush materializes it and resolves every later reference through a
        // create table.
        Entity createEntity()
        {
            const Entity placeholder{nextPlaceholder++, kPlaceholderGeneration};
            commands.emplace_back(
                [slot = placeholder.index](World &world, std::vector<Entity> &created)
                { created[slot] = world.create(); });
            return placeholder;
        }

        void destroyEntity(Entity e)
        {
            commands.emplace_back(
                [e](World &world, std::vector<Entity> &created)
                {
                    const Entity real = resolve(e, created);
                    if (world.alive(real))
                        world.destroy(real);
                });
        }

        template <class T>
        void add(Entity e, T value = {})
        {
            commands.emplace_back(
                [e, value = std::move(value)](World &world, std::vector<Entity> &created) mutable
                {
                    const Entity real = resolve(e, created);
                    if (world.alive(real) && !world.has<T>(real))
                        world.add<T>(real, std::move(value));
                });
        }

        template <class T>
        void remove(Entity e)
        {
            commands.emplace_back(
                [e](World &world, std::vector<Entity> &created)
                {
                    const Entity real = resolve(e, created);
                    if (world.alive(real) && world.has<T>(real))
                        world.remove<T>(real);
                });
        }

        void flush(World &world)
        {
            if (commands.empty())
                return;
            std::vector<Entity> created(nextPlaceholder);   // placeholder index -> real
            for (auto &command : commands)
                command(world, created);
            clear();
        }

        void clear()
        {
            commands.clear();
            nextPlaceholder = 0;
        }

        bool empty() const { return commands.empty(); }

    private:
        static Entity resolve(Entity e, const std::vector<Entity> &created)
        {
            return e.generation == kPlaceholderGeneration ? created[e.index] : e;
        }

        std::vector<std::function<void(World &, std::vector<Entity> &)>> commands;
        uint32_t nextPlaceholder = 0;
    };
}
