#include "Scene/Entities/Entity.hpp"

#include <stdexcept>

#include "Scene/Scene.hpp"

namespace Faye
{
    bool Entity::isValid() const
    {
        return scene != nullptr && scene->isValid(entityId);
    }

    std::string_view Entity::getName() const
    {
        return scene != nullptr ? scene->getEntityName(entityId) : std::string_view{};
    }

    void Entity::setName(std::string name) const
    {
        requireScene().setEntityName(entityId, std::move(name));
    }

    void Entity::destroy()
    {
        requireScene().destroyEntity(entityId);
        entityId = invalidEntity;
    }

    void Entity::setPrimaryCamera() const
    {
        requireScene().setPrimaryCamera(*this);
    }

    ComponentMask Entity::getComponentMask() const
    {
        return scene != nullptr ? scene->getComponentMask(entityId) : 0;
    }

    std::vector<ComponentKind> Entity::getComponentKinds() const
    {
        return scene != nullptr ? scene->getComponentKinds(entityId) : std::vector<ComponentKind>{};
    }

    bool Entity::hasComponent(ComponentKind kind) const
    {
        return scene != nullptr && scene->hasComponent(entityId, kind);
    }

    TransformComponent &Entity::addTransform() const
    {
        return requireScene().addTransform(entityId);
    }

    RigidBody2dComponent &Entity::addRigidBody2d() const
    {
        return requireScene().addRigidBody2d(entityId);
    }

    MeshRendererComponent &Entity::addMesh(ModelHandle modelHandle) const
    {
        return requireScene().addMesh(entityId, modelHandle);
    }

    CameraComponent &Entity::addCamera(bool primary) const
    {
        return requireScene().addCamera(entityId, primary);
    }

    PointLightComponent &Entity::addPointLight() const
    {
        return requireScene().addPointLight(entityId);
    }

    PostProcessStackComponent &Entity::addPostProcessStack() const
    {
        return requireScene().addPostProcessStack(entityId);
    }

    void Entity::removeTransform() const
    {
        requireScene().removeTransform(entityId);
    }

    void Entity::removeRigidBody2d() const
    {
        requireScene().removeRigidBody2d(entityId);
    }

    void Entity::removeMesh() const
    {
        requireScene().removeMesh(entityId);
    }

    void Entity::removeCamera() const
    {
        requireScene().removeCamera(entityId);
    }

    void Entity::removePointLight() const
    {
        requireScene().removePointLight(entityId);
    }

    void Entity::removePostProcessStack() const
    {
        requireScene().removePostProcessStack(entityId);
    }

    TransformComponent *Entity::tryGetTransform() const
    {
        return scene != nullptr ? scene->tryGetTransform(entityId) : nullptr;
    }

    RigidBody2dComponent *Entity::tryGetRigidBody2d() const
    {
        return scene != nullptr ? scene->tryGetRigidBody2d(entityId) : nullptr;
    }

    MeshRendererComponent *Entity::tryGetMesh() const
    {
        return scene != nullptr ? scene->tryGetMesh(entityId) : nullptr;
    }

    CameraComponent *Entity::tryGetCamera() const
    {
        return scene != nullptr ? scene->tryGetCamera(entityId) : nullptr;
    }

    PointLightComponent *Entity::tryGetPointLight() const
    {
        return scene != nullptr ? scene->tryGetPointLight(entityId) : nullptr;
    }

    PostProcessStackComponent *Entity::tryGetPostProcessStack() const
    {
        return scene != nullptr ? scene->tryGetPostProcessStack(entityId) : nullptr;
    }

    Scene &Entity::requireScene() const
    {
        if (scene == nullptr)
        {
            throw std::runtime_error("Attempted to use an entity facade without an owning scene");
        }

        return *scene;
    }
}