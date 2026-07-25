#pragma once

#include <string>
#include <string_view>

#include "Core/ECS/Entity.hpp"
#include "Scene/Entities/Components.hpp"

namespace Faye
{
    class Scene;

    // Convenience facade over (Scene*, Ecs::Entity). Copyable and cheap; the
    // generational handle means a stale copy safely reads as invalid after
    // the entity dies.
    class Entity
    {
    public:
        Entity() = default;
        Entity(Scene *scene, Ecs::Entity handle) : scene(scene), entityHandle(handle) {}

        Ecs::Entity handle() const { return entityHandle; }
        bool isValid() const;
        explicit operator bool() const { return isValid(); }

        std::string_view getName() const;
        void setName(std::string name) const;
        void destroy();
        void setPrimaryCamera() const;

        // Mirror of Scene's generic component API, bound to this entity.
        // Bodies live at the bottom of Scene.hpp: they need Scene complete,
        // and Scene.hpp is what includes this header.
        template <class T>
        T &add() const;

        template <class T>
        void remove() const;

        // Null-scene safe: reads as "no component" rather than throwing, so
        // callers holding a default-constructed Entity can probe it.
        template <class T>
        T *tryGet() const;

        // See the Scene overloads — these carry extra invariants.
        MeshRendererComponent &addMesh(ModelHandle modelHandle = {}) const;
        CameraComponent &addCamera(bool primary = false) const;

    private:
        Scene &requireScene() const;

        Scene *scene = nullptr;
        Ecs::Entity entityHandle{};   // null by default
    };
}
