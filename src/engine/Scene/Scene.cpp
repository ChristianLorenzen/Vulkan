#include "Scene.hpp"

#include <algorithm>
#include <utility>
#include <stdexcept>

#include "engine/Scene/Entities/RegisterComponents.hpp"
namespace Faye
{
    Scene::Scene(std::string sceneName) : name(std::move(sceneName))
    {
        registerEngineComponents(world);

        // No primary-camera remove hook is needed: the primary camera is derived
        // from the components themselves (findPrimaryCamera), so destroying or
        // removing one cannot leave a stale cache behind.
    }

    Scene::~Scene()
    {
        clear();
    }

    // Fill-in-place load: destroy every entity through the normal path so all
    // remove hooks fire, then the scene is rebuilt (from file or the builder).
    void Scene::clear()
    {
        while (!sceneEntities.empty())
        {
            destroyEntity(sceneEntities.back());
        }
    }

    Entity Scene::createEntity(std::string name)
    {
        const Ecs::Entity entity = world.create();
        sceneEntities.push_back(entity);

        if (name.empty())
        {
            name = defaultEntityName(entity);
        }
        world.add<EntityMetadata>(entity, EntityMetadata{std::move(name)});

        return Entity{this, entity};
    }

    Entity Scene::createEntityWithGuid(std::string name, Uuid guid)
    {
        const Ecs::Entity entity = world.createWithGuid(guid);
        sceneEntities.push_back(entity);

        if (name.empty())
        {
            name = defaultEntityName(entity);
        }
        world.add<EntityMetadata>(entity, EntityMetadata{std::move(name)});

        return Entity{this, entity};
    }

    Entity Scene::getEntity(Ecs::Entity entity)
    {
        return Entity{this, isValid(entity) ? entity : Ecs::Entity::null()};
    }

    Entity Scene::duplicateEntity(Ecs::Entity entity)
    {
        if (!isValid(entity))
        {
            return Entity{this, Ecs::Entity::null()};
        }

        // EntityMetadata is not part of the type registry at this point
        // so unless a name is explicitly given, the copy will be unnamed.
        // So basically don't use world.create().
        const Entity copiedEntity = createEntity(std::string(getEntityName(entity)));

        for (const Ecs::ComponentTypeInfo &component : world.types().all())
        {
            // copyTo is null for types registered with Clone::skip.
            if (component.name && component.copyTo && component.has(world, entity))
            {
                component.copyTo(world, entity, copiedEntity.handle());
            }
        }

        return copiedEntity;
    }

    Ecs::Entity Scene::findPrimaryCamera() const
    {
        for (const Ecs::Entity entity : sceneEntities)
        {
            if (const auto *camera = tryGet<CameraComponent>(entity))
            {
                if (camera->primary)
                    return entity;
            }
        }
        return Ecs::Entity::null();
    }

    Entity Scene::getPrimaryCameraEntity()
    {
        return getEntity(findPrimaryCamera());
    }

    void Scene::destroyEntity(Ecs::Entity entity)
    {
        if (!isValid(entity))
        {
            return;
        }

        // world.destroy fires every remove hook (scripts, camera, ...) while
        // the components are still intact.
        world.destroy(entity);
        sceneEntities.erase(std::remove(sceneEntities.begin(), sceneEntities.end(), entity),
                            sceneEntities.end());
    }

    void Scene::destroyEntity(Entity entity)
    {
        destroyEntity(entity.handle());
    }

    bool Scene::isValid(Ecs::Entity entity) const
    {
        return world.alive(entity);
    }

    Ecs::Entity Scene::require(Ecs::Entity entity) const
    {
        if (!world.alive(entity))
        {
            throw std::runtime_error("Attempted to access an entity that does not exist in the scene");
        }

        return entity;
    }

    void Scene::setEntityName(Ecs::Entity entity, std::string name)
    {
        require(entity);

        if (name.empty())
        {
            name = defaultEntityName(entity);
        }

        world.tryGet<EntityMetadata>(entity)->name = std::move(name);
    }

    std::string Scene::defaultEntityName(Ecs::Entity entity)
    {
        // Slot-based, not a creation-order counter: with generational reuse
        // there is no monotonic legacy id left to name after. Two entities
        // can carry the same default name if one died and its slot was
        // recycled — cosmetic only, never used as an identity.
        return "Entity " + std::to_string(entity.index + 1);
    }

    std::string_view Scene::getEntityName(Ecs::Entity entity) const
    {
        const auto *metadata = tryGetEntityMetadata(entity);
        return metadata != nullptr ? metadata->name : std::string_view{};
    }

    const Scene::EntityMetadata *Scene::tryGetEntityMetadata(Ecs::Entity entity) const
    {
        return world.tryGet<EntityMetadata>(entity);
    }

    MeshRendererComponent &Scene::addMesh(Ecs::Entity entity, ModelHandle modelHandle)
    {
        auto &mesh = addOrGet<MeshRendererComponent>(entity);
        mesh.modelHandle = modelHandle;
        return mesh;
    }

    CameraComponent &Scene::addCamera(Ecs::Entity entity, bool primary)
    {
        auto &camera = addOrGet<CameraComponent>(entity);
        camera.primary = primary;

        if (primary || findPrimaryCamera().isNull())
        {
            setPrimaryCamera(entity);
        }

        return camera;
    }

    void Scene::setPrimaryCamera(Ecs::Entity entity)
    {
        auto *camera = tryGet<CameraComponent>(entity);
        if (camera == nullptr)
        {
            throw std::runtime_error("Cannot set primary camera on entity without a camera component");
        }

        for (const Ecs::Entity cameraEntity : getEntities())
        {
            if (auto *cameraComponent = tryGet<CameraComponent>(cameraEntity))
            {
                cameraComponent->primary = false;
            }
        }

        camera->primary = true;
    }

    void Scene::setPrimaryCamera(Entity entity)
    {
        setPrimaryCamera(entity.handle());
    }

    const CameraComponent *Scene::getPrimaryCamera() const
    {
        const Ecs::Entity entity = findPrimaryCamera();
        return entity.isNull() ? nullptr : tryGet<CameraComponent>(entity);
    }

    CameraComponent *Scene::getPrimaryCamera()
    {
        // Delegate to the const overload rather than duplicating the scan.
        // Writing `return getPrimaryCamera();` in the CONST version instead
        // would have called itself forever.
        return const_cast<CameraComponent *>(std::as_const(*this).getPrimaryCamera());
    }
}
