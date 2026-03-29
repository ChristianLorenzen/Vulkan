#include "Scene/Entities/EntityManager.hpp"

#include <algorithm>
#include <stdexcept>

namespace Faye
{
    EntityId EntityManager::createEntity(std::string name)
    {
        const EntityId entity = nextEntityId++;
        entities.push_back(entity);
        entityLookup.insert(entity);
        entityMetadata.emplace(entity, EntityMetadata{std::move(name)});

        if (entityMetadata[entity].name.empty())
        {
            entityMetadata[entity].name = makeDefaultEntityName(entity);
        }

        componentMasks[entity] = 0;
        return entity;
    }

    void EntityManager::destroyEntity(EntityId entity)
    {
        if (!isValid(entity))
        {
            return;
        }

        entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
        entityLookup.erase(entity);
        entityMetadata.erase(entity);
        componentMasks.erase(entity);
        transforms.erase(entity);
        rigidBody2dComponents.erase(entity);
        meshComponents.erase(entity);
        cameraComponents.erase(entity);
        pointLightComponents.erase(entity);
    }

    bool EntityManager::isValid(EntityId entity) const
    {
        return entity != invalidEntity && entityLookup.contains(entity);
    }

    void EntityManager::setEntityName(EntityId entity, std::string name)
    {
        requireEntity(entity);

        if (name.empty())
        {
            name = makeDefaultEntityName(entity);
        }

        entityMetadata[entity].name = std::move(name);
    }

    std::string_view EntityManager::getEntityName(EntityId entity) const
    {
        auto iterator = entityMetadata.find(entity);
        return iterator != entityMetadata.end() ? iterator->second.name : std::string_view{};
    }

    const EntityMetadata *EntityManager::tryGetEntityMetadata(EntityId entity) const
    {
        auto iterator = entityMetadata.find(entity);
        return iterator != entityMetadata.end() ? &iterator->second : nullptr;
    }

    ComponentMask EntityManager::getComponentMask(EntityId entity) const
    {
        auto iterator = componentMasks.find(entity);
        return iterator != componentMasks.end() ? iterator->second : 0;
    }

    bool EntityManager::hasComponent(EntityId entity, ComponentKind kind) const
    {
        return (getComponentMask(entity) & componentBit(kind)) != 0;
    }

    std::vector<ComponentKind> EntityManager::getComponentKinds(EntityId entity) const
    {
        std::vector<ComponentKind> componentKinds;
        const ComponentMask mask = getComponentMask(entity);

        for (uint8_t index = 0; index < static_cast<uint8_t>(ComponentKind::Count); ++index)
        {
            const auto kind = static_cast<ComponentKind>(index);
            if ((mask & componentBit(kind)) != 0)
            {
                componentKinds.push_back(kind);
            }
        }

        return componentKinds;
    }

    TransformComponent &EntityManager::addTransform(EntityId entity)
    {
        requireEntity(entity);
        setComponent(entity, ComponentKind::Transform);
        return transforms[entity];
    }

    RigidBody2dComponent &EntityManager::addRigidBody2d(EntityId entity)
    {
        requireEntity(entity);
        setComponent(entity, ComponentKind::RigidBody2d);
        return rigidBody2dComponents[entity];
    }

    MeshComponent &EntityManager::addMesh(EntityId entity, ModelHandle modelHandle)
    {
        requireEntity(entity);
        setComponent(entity, ComponentKind::Mesh);

        auto &mesh = meshComponents[entity];
        mesh.modelHandle = modelHandle;
        return mesh;
    }

    CameraComponent &EntityManager::addCamera(EntityId entity)
    {
        requireEntity(entity);
        setComponent(entity, ComponentKind::Camera);
        return cameraComponents[entity];
    }

    PointLightComponent &EntityManager::addPointLight(EntityId entity)
    {
        requireEntity(entity);
        setComponent(entity, ComponentKind::PointLight);
        return pointLightComponents[entity];
    }

    void EntityManager::removeTransform(EntityId entity)
    {
        transforms.erase(entity);
        clearComponent(entity, ComponentKind::Transform);
    }

    void EntityManager::removeRigidBody2d(EntityId entity)
    {
        rigidBody2dComponents.erase(entity);
        clearComponent(entity, ComponentKind::RigidBody2d);
    }

    void EntityManager::removeMesh(EntityId entity)
    {
        meshComponents.erase(entity);
        clearComponent(entity, ComponentKind::Mesh);
    }

    void EntityManager::removeCamera(EntityId entity)
    {
        cameraComponents.erase(entity);
        clearComponent(entity, ComponentKind::Camera);
    }

    void EntityManager::removePointLight(EntityId entity)
    {
        pointLightComponents.erase(entity);
        clearComponent(entity, ComponentKind::PointLight);
    }

    TransformComponent *EntityManager::tryGetTransform(EntityId entity)
    {
        auto iterator = transforms.find(entity);
        return iterator != transforms.end() ? &iterator->second : nullptr;
    }

    const TransformComponent *EntityManager::tryGetTransform(EntityId entity) const
    {
        auto iterator = transforms.find(entity);
        return iterator != transforms.end() ? &iterator->second : nullptr;
    }

    RigidBody2dComponent *EntityManager::tryGetRigidBody2d(EntityId entity)
    {
        auto iterator = rigidBody2dComponents.find(entity);
        return iterator != rigidBody2dComponents.end() ? &iterator->second : nullptr;
    }

    const RigidBody2dComponent *EntityManager::tryGetRigidBody2d(EntityId entity) const
    {
        auto iterator = rigidBody2dComponents.find(entity);
        return iterator != rigidBody2dComponents.end() ? &iterator->second : nullptr;
    }

    MeshComponent *EntityManager::tryGetMesh(EntityId entity)
    {
        auto iterator = meshComponents.find(entity);
        return iterator != meshComponents.end() ? &iterator->second : nullptr;
    }

    const MeshComponent *EntityManager::tryGetMesh(EntityId entity) const
    {
        auto iterator = meshComponents.find(entity);
        return iterator != meshComponents.end() ? &iterator->second : nullptr;
    }

    CameraComponent *EntityManager::tryGetCamera(EntityId entity)
    {
        auto iterator = cameraComponents.find(entity);
        return iterator != cameraComponents.end() ? &iterator->second : nullptr;
    }

    const CameraComponent *EntityManager::tryGetCamera(EntityId entity) const
    {
        auto iterator = cameraComponents.find(entity);
        return iterator != cameraComponents.end() ? &iterator->second : nullptr;
    }

    PointLightComponent *EntityManager::tryGetPointLight(EntityId entity)
    {
        auto iterator = pointLightComponents.find(entity);
        return iterator != pointLightComponents.end() ? &iterator->second : nullptr;
    }

    const PointLightComponent *EntityManager::tryGetPointLight(EntityId entity) const
    {
        auto iterator = pointLightComponents.find(entity);
        return iterator != pointLightComponents.end() ? &iterator->second : nullptr;
    }

    void EntityManager::requireEntity(EntityId entity) const
    {
        if (!isValid(entity))
        {
            throw std::runtime_error("Attempted to access an entity that does not exist in the entity manager");
        }
    }

    std::string EntityManager::makeDefaultEntityName(EntityId entity) const
    {
        return "Entity " + std::to_string(entity);
    }

    void EntityManager::setComponent(EntityId entity, ComponentKind kind)
    {
        componentMasks[entity] |= componentBit(kind);
    }

    void EntityManager::clearComponent(EntityId entity, ComponentKind kind)
    {
        auto iterator = componentMasks.find(entity);
        if (iterator != componentMasks.end())
        {
            iterator->second &= ~componentBit(kind);
        }
    }
}