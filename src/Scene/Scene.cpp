#include "Scene.hpp"

#include <algorithm>
#include <stdexcept>

namespace Faye
{
    Scene::Scene(std::string sceneName) : name(std::move(sceneName))
    {
    }

    Entity Scene::createEntity(std::string name)
    {
        return Entity{this, entityManager.createEntity(std::move(name))};
    }

    Entity Scene::getEntity(EntityId entity)
    {
        return Entity{this, isValid(entity) ? entity : invalidEntity};
    }

    Entity Scene::getPrimaryCameraEntity()
    {
        return getEntity(primaryCameraEntity);
    }

    void Scene::destroyEntity(EntityId entity)
    {
        if (!isValid(entity))
        {
            return;
        }

        entityManager.destroyEntity(entity);
        if (primaryCameraEntity == entity)
        {
            primaryCameraEntity = invalidEntity;
        }
    }

    void Scene::destroyEntity(Entity entity)
    {
        destroyEntity(entity.id());
    }

    bool Scene::isValid(EntityId entity) const
    {
        return entityManager.isValid(entity);
    }

    void Scene::setEntityName(EntityId entity, std::string name)
    {
        entityManager.setEntityName(entity, std::move(name));
    }

    std::string_view Scene::getEntityName(EntityId entity) const
    {
        return entityManager.getEntityName(entity);
    }

    const Scene::EntityMetadata *Scene::tryGetEntityMetadata(EntityId entity) const
    {
        return entityManager.tryGetEntityMetadata(entity);
    }

    Scene::ComponentMask Scene::getComponentMask(EntityId entity) const
    {
        return entityManager.getComponentMask(entity);
    }

    bool Scene::hasComponent(EntityId entity, ComponentKind kind) const
    {
        return entityManager.hasComponent(entity, kind);
    }

    std::vector<Scene::ComponentKind> Scene::getComponentKinds(EntityId entity) const
    {
        return entityManager.getComponentKinds(entity);
    }

    TransformComponent &Scene::addTransform(EntityId entity)
    {
        return entityManager.addTransform(entity);
    }

    RigidBody2dComponent &Scene::addRigidBody2d(EntityId entity)
    {
        return entityManager.addRigidBody2d(entity);
    }

    MeshRendererComponent &Scene::addMesh(EntityId entity, ModelHandle modelHandle)
    {
        return entityManager.addMesh(entity, modelHandle);
    }

    CameraComponent &Scene::addCamera(EntityId entity, bool primary)
    {
        auto &camera = entityManager.addCamera(entity);
        camera.primary = primary;

        if (primary || primaryCameraEntity == invalidEntity)
        {
            setPrimaryCamera(getEntity(entity));
        }

        return camera;
    }

    PointLightComponent &Scene::addPointLight(EntityId entity)
    {
        return entityManager.addPointLight(entity);
    }

    PostProcessStackComponent &Scene::addPostProcessStack(EntityId entity)
    {
        return entityManager.addPostProcessStack(entity);
    }

    void Scene::removeTransform(EntityId entity)
    {
        entityManager.removeTransform(entity);
    }

    void Scene::removeRigidBody2d(EntityId entity)
    {
        entityManager.removeRigidBody2d(entity);
    }

    void Scene::removeMesh(EntityId entity)
    {
        entityManager.removeMesh(entity);
    }

    void Scene::removeCamera(EntityId entity)
    {
        entityManager.removeCamera(entity);
        if (primaryCameraEntity == entity)
        {
            primaryCameraEntity = invalidEntity;
        }
    }

    void Scene::removePointLight(EntityId entity)
    {
        entityManager.removePointLight(entity);
    }

    void Scene::removePostProcessStack(EntityId entity)
    {
        entityManager.removePostProcessStack(entity);
    }

    TransformComponent *Scene::tryGetTransform(EntityId entity)
    {
        return entityManager.tryGetTransform(entity);
    }

    const TransformComponent *Scene::tryGetTransform(EntityId entity) const
    {
        return entityManager.tryGetTransform(entity);
    }

    RigidBody2dComponent *Scene::tryGetRigidBody2d(EntityId entity)
    {
        return entityManager.tryGetRigidBody2d(entity);
    }

    const RigidBody2dComponent *Scene::tryGetRigidBody2d(EntityId entity) const
    {
        return entityManager.tryGetRigidBody2d(entity);
    }

    MeshRendererComponent *Scene::tryGetMesh(EntityId entity)
    {
        return entityManager.tryGetMesh(entity);
    }

    const MeshRendererComponent *Scene::tryGetMesh(EntityId entity) const
    {
        return entityManager.tryGetMesh(entity);
    }

    CameraComponent *Scene::tryGetCamera(EntityId entity)
    {
        return entityManager.tryGetCamera(entity);
    }

    const CameraComponent *Scene::tryGetCamera(EntityId entity) const
    {
        return entityManager.tryGetCamera(entity);
    }

    PointLightComponent *Scene::tryGetPointLight(EntityId entity)
    {
        return entityManager.tryGetPointLight(entity);
    }

    const PointLightComponent *Scene::tryGetPointLight(EntityId entity) const
    {
        return entityManager.tryGetPointLight(entity);
    }

    PostProcessStackComponent *Scene::tryGetPostProcessStack(EntityId entity)
    {
        return entityManager.tryGetPostProcessStack(entity);
    }

    const PostProcessStackComponent *Scene::tryGetPostProcessStack(EntityId entity) const
    {
        return entityManager.tryGetPostProcessStack(entity);
    }

    void Scene::setPrimaryCamera(EntityId entity)
    {
        auto *camera = tryGetCamera(entity);
        if (camera == nullptr)
        {
            throw std::runtime_error("Cannot set primary camera on entity without a camera component");
        }

        for (EntityId cameraEntity : getEntities())
        {
            if (auto *cameraComponent = tryGetCamera(cameraEntity))
            {
                cameraComponent->primary = false;
            }
        }

        camera->primary = true;
        primaryCameraEntity = entity;
    }

    void Scene::setPrimaryCamera(Entity entity)
    {
        setPrimaryCamera(entity.id());
    }

    CameraComponent *Scene::getPrimaryCamera()
    {
        return tryGetCamera(primaryCameraEntity);
    }

    const CameraComponent *Scene::getPrimaryCamera() const
    {
        return tryGetCamera(primaryCameraEntity);
    }

    std::vector<Scene::RenderableView> Scene::getRenderableViews() const
    {
        std::vector<RenderableView> renderables;

        for (EntityId entity : getEntities())
        {
            const auto *transform = tryGetTransform(entity);
            const auto *mesh = tryGetMesh(entity);
            if (transform == nullptr || mesh == nullptr)
            {
                continue;
            }

            renderables.push_back(RenderableView{entity, transform, mesh});
        }

        return renderables;
    }

    std::vector<Scene::PointLightView> Scene::getPointLightViews() const
    {
        std::vector<PointLightView> pointLights;

        for (EntityId entity : getEntities())
        {
            const auto *transform = tryGetTransform(entity);
            const auto *pointLight = tryGetPointLight(entity);
            if (transform == nullptr || pointLight == nullptr)
            {
                continue;
            }

            pointLights.push_back(PointLightView{entity, transform, pointLight});
        }

        return pointLights;
    }
}