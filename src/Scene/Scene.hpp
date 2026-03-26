#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Assets/ModelRegistry.hpp"
#include "Scene/Camera/Camera.hpp"
#include "Scene/Entities/GameObject.hpp"

namespace Faye
{
    class Scene
    {
    public:
        using EntityId = uint32_t;
        static constexpr EntityId invalidEntity = 0;

        struct MeshComponent
        {
            ModelHandle modelHandle{};
            glm::vec3 color{};
        };

        struct CameraComponent
        {
            Camera camera{};
            bool primary = false;
        };

        struct RenderableView
        {
            EntityId entity = invalidEntity;
            const TransformComponent *transform = nullptr;
            const MeshComponent *mesh = nullptr;
        };

        explicit Scene(std::string sceneName = "Scene");

        EntityId createEntity();
        void destroyEntity(EntityId entity);
        bool isValid(EntityId entity) const;

        std::string_view getName() const { return name; }

        TransformComponent &addTransform(EntityId entity);
        RigidBody2dComponent &addRigidBody2d(EntityId entity);
        MeshComponent &addMesh(EntityId entity, ModelHandle modelHandle = {});
        CameraComponent &addCamera(EntityId entity, bool primary = false);

        TransformComponent *tryGetTransform(EntityId entity);
        const TransformComponent *tryGetTransform(EntityId entity) const;

        RigidBody2dComponent *tryGetRigidBody2d(EntityId entity);
        const RigidBody2dComponent *tryGetRigidBody2d(EntityId entity) const;

        MeshComponent *tryGetMesh(EntityId entity);
        const MeshComponent *tryGetMesh(EntityId entity) const;

        CameraComponent *tryGetCamera(EntityId entity);
        const CameraComponent *tryGetCamera(EntityId entity) const;

        void setPrimaryCamera(EntityId entity);
        EntityId getPrimaryCameraEntity() const { return primaryCameraEntity; }
        CameraComponent *getPrimaryCamera();
        const CameraComponent *getPrimaryCamera() const;

        std::vector<RenderableView> getRenderableViews() const;

    private:
        void requireEntity(EntityId entity) const;

        std::string name;
        EntityId nextEntityId = 1;
        EntityId primaryCameraEntity = invalidEntity;

        std::vector<EntityId> entities;
        std::unordered_map<EntityId, TransformComponent> transforms;
        std::unordered_map<EntityId, RigidBody2dComponent> rigidBody2dComponents;
        std::unordered_map<EntityId, MeshComponent> meshComponents;
        std::unordered_map<EntityId, CameraComponent> cameraComponents;
    };
}