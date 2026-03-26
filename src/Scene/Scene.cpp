#include "Scene.hpp"

#include <algorithm>
#include <stdexcept>

using namespace Faye;

Faye::Scene::Scene(std::string sceneName) : name(std::move(sceneName))
{
}

Faye::Scene::EntityId Faye::Scene::createEntity()
{
    const EntityId entity = nextEntityId++;
    entities.push_back(entity);
    return entity;
}

void Faye::Scene::destroyEntity(EntityId entity)
{
    if (!isValid(entity))
    {
        return;
    }

    entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
    transforms.erase(entity);
    rigidBody2dComponents.erase(entity);
    meshComponents.erase(entity);
    cameraComponents.erase(entity);

    if (primaryCameraEntity == entity)
    {
        primaryCameraEntity = invalidEntity;
    }
}

bool Faye::Scene::isValid(EntityId entity) const
{
    return entity != invalidEntity &&
           std::find(entities.begin(), entities.end(), entity) != entities.end();
}

Faye::TransformComponent &Faye::Scene::addTransform(EntityId entity)
{
    requireEntity(entity);
    return transforms[entity];
}

Faye::RigidBody2dComponent &Faye::Scene::addRigidBody2d(EntityId entity)
{
    requireEntity(entity);
    return rigidBody2dComponents[entity];
}

Faye::Scene::MeshComponent &Faye::Scene::addMesh(EntityId entity, ModelHandle modelHandle)
{
    requireEntity(entity);
    auto &mesh = meshComponents[entity];
    mesh.modelHandle = modelHandle;
    return mesh;
}

Faye::Scene::CameraComponent &Faye::Scene::addCamera(EntityId entity, bool primary)
{
    requireEntity(entity);
    auto &camera = cameraComponents[entity];
    camera.primary = primary;

    if (primary || primaryCameraEntity == invalidEntity)
    {
        setPrimaryCamera(entity);
    }

    return camera;
}

Faye::TransformComponent *Faye::Scene::tryGetTransform(EntityId entity)
{
    auto iterator = transforms.find(entity);
    return iterator != transforms.end() ? &iterator->second : nullptr;
}

const Faye::TransformComponent *Faye::Scene::tryGetTransform(EntityId entity) const
{
    auto iterator = transforms.find(entity);
    return iterator != transforms.end() ? &iterator->second : nullptr;
}

Faye::RigidBody2dComponent *Faye::Scene::tryGetRigidBody2d(EntityId entity)
{
    auto iterator = rigidBody2dComponents.find(entity);
    return iterator != rigidBody2dComponents.end() ? &iterator->second : nullptr;
}

const Faye::RigidBody2dComponent *Faye::Scene::tryGetRigidBody2d(EntityId entity) const
{
    auto iterator = rigidBody2dComponents.find(entity);
    return iterator != rigidBody2dComponents.end() ? &iterator->second : nullptr;
}

Faye::Scene::MeshComponent *Faye::Scene::tryGetMesh(EntityId entity)
{
    auto iterator = meshComponents.find(entity);
    return iterator != meshComponents.end() ? &iterator->second : nullptr;
}

const Faye::Scene::MeshComponent *Faye::Scene::tryGetMesh(EntityId entity) const
{
    auto iterator = meshComponents.find(entity);
    return iterator != meshComponents.end() ? &iterator->second : nullptr;
}

Faye::Scene::CameraComponent *Faye::Scene::tryGetCamera(EntityId entity)
{
    auto iterator = cameraComponents.find(entity);
    return iterator != cameraComponents.end() ? &iterator->second : nullptr;
}

const Faye::Scene::CameraComponent *Faye::Scene::tryGetCamera(EntityId entity) const
{
    auto iterator = cameraComponents.find(entity);
    return iterator != cameraComponents.end() ? &iterator->second : nullptr;
}

void Faye::Scene::setPrimaryCamera(EntityId entity)
{
    requireEntity(entity);

    for (auto &[cameraEntity, camera] : cameraComponents)
    {
        camera.primary = false;
    }

    auto *camera = tryGetCamera(entity);
    if (camera == nullptr)
    {
        throw std::runtime_error("Cannot set primary camera on entity without a camera component");
    }

    camera->primary = true;
    primaryCameraEntity = entity;
}

Faye::Scene::CameraComponent *Faye::Scene::getPrimaryCamera()
{
    return tryGetCamera(primaryCameraEntity);
}

const Faye::Scene::CameraComponent *Faye::Scene::getPrimaryCamera() const
{
    return tryGetCamera(primaryCameraEntity);
}

std::vector<Faye::Scene::RenderableView> Faye::Scene::getRenderableViews() const
{
    std::vector<RenderableView> renderables;
    renderables.reserve(meshComponents.size());

    for (auto entity : entities)
    {
        auto transformIterator = transforms.find(entity);
        auto meshIterator = meshComponents.find(entity);

        if (transformIterator == transforms.end() || meshIterator == meshComponents.end())
        {
            continue;
        }

        renderables.push_back(RenderableView{
            entity,
            &transformIterator->second,
            &meshIterator->second});
    }

    return renderables;
}

void Faye::Scene::requireEntity(EntityId entity) const
{
    if (!isValid(entity))
    {
        throw std::runtime_error("Attempted to access an entity that does not exist in the scene");
    }
}