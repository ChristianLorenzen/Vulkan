#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Scene/Entities/Entity.hpp"
#include "Scene/Entities/EntityManager.hpp"

namespace Faye
{
    class Scene
    {
    public:
        using EntityId = Faye::EntityId;
        static constexpr EntityId invalidEntity = Faye::invalidEntity;
        using EntityMetadata = Faye::EntityMetadata;
        using ComponentKind = Faye::ComponentKind;
        using ComponentMask = Faye::ComponentMask;

        struct RenderableView
        {
            EntityId entity = invalidEntity;
            const TransformComponent *transform = nullptr;
            const MeshRendererComponent *mesh = nullptr;
        };

        struct PointLightView
        {
            EntityId entity = invalidEntity;
            const TransformComponent *transform = nullptr;
            const PointLightComponent *pointLight = nullptr;
        };

        explicit Scene(std::string sceneName = "Scene");

        Entity createEntity(std::string name = {});
        Entity getEntity(EntityId entity);
        Entity getPrimaryCameraEntity();
        void destroyEntity(EntityId entity);
        void destroyEntity(Entity entity);
        bool isValid(EntityId entity) const;

        std::string_view getName() const { return name; }
        const std::vector<EntityId> &getEntities() const { return entityManager.getEntities(); }

        void setEntityName(EntityId entity, std::string name);
        std::string_view getEntityName(EntityId entity) const;
        const EntityMetadata *tryGetEntityMetadata(EntityId entity) const;
        ComponentMask getComponentMask(EntityId entity) const;
        bool hasComponent(EntityId entity, ComponentKind kind) const;
        std::vector<ComponentKind> getComponentKinds(EntityId entity) const;

        TransformComponent &addTransform(EntityId entity);
        RigidBody2dComponent &addRigidBody2d(EntityId entity);
        MeshRendererComponent &addMesh(EntityId entity, ModelHandle modelHandle = {});
        CameraComponent &addCamera(EntityId entity, bool primary = false);
        PointLightComponent &addPointLight(EntityId entity);
        PostProcessStackComponent &addPostProcessStack(EntityId entity);
        WaterComponent &addWater(EntityId entity);

        void removeTransform(EntityId entity);
        void removeRigidBody2d(EntityId entity);
        void removeMesh(EntityId entity);
        void removeCamera(EntityId entity);
        void removePointLight(EntityId entity);
        void removePostProcessStack(EntityId entity);
        void removeWater(EntityId entity);

        TransformComponent *tryGetTransform(EntityId entity);
        const TransformComponent *tryGetTransform(EntityId entity) const;

        RigidBody2dComponent *tryGetRigidBody2d(EntityId entity);
        const RigidBody2dComponent *tryGetRigidBody2d(EntityId entity) const;

        MeshRendererComponent *tryGetMesh(EntityId entity);
        const MeshRendererComponent *tryGetMesh(EntityId entity) const;

        CameraComponent *tryGetCamera(EntityId entity);
        const CameraComponent *tryGetCamera(EntityId entity) const;

        PointLightComponent *tryGetPointLight(EntityId entity);
        const PointLightComponent *tryGetPointLight(EntityId entity) const;

        PostProcessStackComponent *tryGetPostProcessStack(EntityId entity);
        const PostProcessStackComponent *tryGetPostProcessStack(EntityId entity) const;
        WaterComponent *tryGetWater(EntityId entity);
        const WaterComponent *tryGetWater(EntityId entity) const;

        void setPrimaryCamera(EntityId entity);
        void setPrimaryCamera(Entity entity);
        EntityId getPrimaryCameraEntityId() const { return primaryCameraEntity; }
        CameraComponent *getPrimaryCamera();
        const CameraComponent *getPrimaryCamera() const;

        std::vector<RenderableView> getRenderableViews() const;
        std::vector<PointLightView> getPointLightViews() const;

    private:
        std::string name;
        EntityManager entityManager;
        EntityId primaryCameraEntity = invalidEntity;
    };
}