#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Core/ECS/World.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Core/Serialization/Uuid.hpp"
#include "Scene/Entities/Entity.hpp"

namespace Faye
{
    struct SkyboxSettings
    {
        bool enabled = true;
        float rotation = 0.0f;
        float intensity = 1.0f;
    };
    
    struct SceneSettings
    {
        SkyboxSettings skybox{ .enabled = true, .rotation = 0.0f, .intensity = 1.0f };
    };

    // Thin facade over Ecs::World: callers hold generational Ecs::Entity
    // handles directly (a stale handle safely reads as dead — no aliasing).
    // Scene adds what the World deliberately doesn't know about: entity
    // naming, creation-order enumeration for the editor, the primary-camera
    // invariant, and the per-type convenience wrappers (which forward to
    // world.add<T>/remove<T>/tryGet<T>).
    class Scene
    {
    public:
        using EntityMetadata = Faye::EntityMetadata;

        explicit Scene(std::string sceneName = "Scene");

        // Destroys all remaining entities through the normal path so every
        // remove hook (scripts, camera) fires while the World is intact.
        ~Scene();

        // Destroy every entity (used by fill-in-place scene loads / New).
        void clear();

        // The World's remove hooks capture `this`; pinning the object is
        // simpler than rebinding them on move.
        Scene(const Scene &) = delete;
        Scene &operator=(const Scene &) = delete;
        Scene(Scene &&) = delete;
        Scene &operator=(Scene &&) = delete;

        Entity createEntity(std::string name = {});
        // Create with a persisted GUID (scene load). Collisions mint a fresh
        // random GUID (see EntityRegistry::createWithGuid).
        Entity createEntityWithGuid(std::string name, Uuid guid);
        Entity getEntity(Ecs::Entity entity);
        Entity duplicateEntity(Ecs::Entity entity);
        Entity getPrimaryCameraEntity();
        void destroyEntity(Ecs::Entity entity);
        void destroyEntity(Entity entity);
        bool isValid(Ecs::Entity entity) const;

        Uuid getSceneUuid() const { return sceneUuid; }
        void setSceneUuid(Uuid uuid) { sceneUuid = uuid; }

        std::string_view getName() const { return name; }
        SceneSettings &getSceneSettings() { return sceneSettings; }
        void setName(std::string sceneName) { name = std::move(sceneName); }
        const std::vector<Ecs::Entity> &getEntities() const { return sceneEntities; }

        void setEntityName(Ecs::Entity entity, std::string name);
        std::string_view getEntityName(Ecs::Entity entity) const;
        const EntityMetadata *tryGetEntityMetadata(Ecs::Entity entity) const;

        // Component access is generic: the World already stores components by
        // type, so Scene forwards instead of restating one wrapper per type.
        // These three cover every component; the two named overloads below
        // exist only because they carry invariants a plain add cannot.

        // Returns the existing component untouched if already present.
        // Throws if the handle is dead.
        template <class T>
        T &add(Ecs::Entity entity)
        {
            return addOrGet<T>(entity);
        }

        // No-op when the component — or the entity itself — is absent.
        template <class T>
        void remove(Ecs::Entity entity)
        {
            removeIfPresent<T>(entity);
        }

        template <class T>
        T *tryGet(Ecs::Entity entity)
        {
            return world.tryGet<T>(entity);
        }

        template <class T>
        const T *tryGet(Ecs::Entity entity) const
        {
            return world.tryGet<T>(entity);
        }

        // Doubles as "set the model": assigns modelHandle on first add and on
        // every re-add, so add<MeshRendererComponent> is NOT equivalent.
        MeshRendererComponent &addMesh(Ecs::Entity entity, ModelHandle modelHandle = {});

        // Maintains the scene-wide primary-camera invariant.
        CameraComponent &addCamera(Ecs::Entity entity, bool primary = false);

        void setPrimaryCamera(Ecs::Entity entity);
        void setPrimaryCamera(Entity entity);
        Ecs::Entity getPrimaryCameraHandle() const { return primaryCameraEntity; }
        CameraComponent *getPrimaryCamera();
        const CameraComponent *getPrimaryCamera() const;

        Ecs::World &getWorld() { return world; }
        const Ecs::World &getWorld() const { return world; }

    private:
        Ecs::Entity require(Ecs::Entity entity) const;   // throws on dead/null handles
        static std::string defaultEntityName(Ecs::Entity entity);

        // add-or-get, preserving the pre-ECS semantics: adding a component
        // the entity already has returns the existing one untouched.
        template <class T>
        T &addOrGet(Ecs::Entity entity)
        {
            require(entity);
            if (T *existing = world.tryGet<T>(entity))
                return *existing;
            return world.add<T>(entity);
        }

        template <class T>
        void removeIfPresent(Ecs::Entity entity)
        {
            if (world.has<T>(entity))
                world.remove<T>(entity);
        }

        std::string name;
        SceneSettings sceneSettings;
        Uuid sceneUuid = Uuid::generateV4();
        Ecs::World world;
        std::vector<Ecs::Entity> sceneEntities;   // creation order, backs getEntities()
        Ecs::Entity primaryCameraEntity{};        // null when no primary camera
    };

    // ---- Entity facade forwarding ----------------------------------------
    // Declared in Entity.hpp, defined here: they need Scene complete, and
    // Scene.hpp is the header that includes Entity.hpp (not the reverse).

    template <class T>
    T &Entity::add() const
    {
        return requireScene().add<T>(entityHandle);
    }

    template <class T>
    void Entity::remove() const
    {
        requireScene().remove<T>(entityHandle);
    }

    template <class T>
    T *Entity::tryGet() const
    {
        return scene != nullptr ? scene->tryGet<T>(entityHandle) : nullptr;
    }
}
