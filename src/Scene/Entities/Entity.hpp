#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Scene/Entities/Components.hpp"
#include "Scene/Entities/EntityManager.hpp"

namespace Faye
{
    class Scene;

    class Entity
    {
    public:
        Entity() = default;
        Entity(Scene *scene, EntityId entityId) : scene(scene), entityId(entityId) {}

        EntityId id() const { return entityId; }
        bool isValid() const;
        explicit operator bool() const { return isValid(); }

        std::string_view getName() const;
        void setName(std::string name) const;
        void destroy();
        void setPrimaryCamera() const;

        ComponentMask getComponentMask() const;
        std::vector<ComponentKind> getComponentKinds() const;
        bool hasComponent(ComponentKind kind) const;

        TransformComponent &addTransform() const;
        RigidBody2dComponent &addRigidBody2d() const;
        MeshRendererComponent &addMesh(ModelHandle modelHandle = {}) const;
        CameraComponent &addCamera(bool primary = false) const;
        PointLightComponent &addPointLight() const;
        PostProcessStackComponent &addPostProcessStack() const;
        WaterComponent &addWater() const;

        void removeTransform() const;
        void removeRigidBody2d() const;
        void removeMesh() const;
        void removeCamera() const;
        void removePointLight() const;
        void removePostProcessStack() const;

        TransformComponent *tryGetTransform() const;
        RigidBody2dComponent *tryGetRigidBody2d() const;
        MeshRendererComponent *tryGetMesh() const;
        CameraComponent *tryGetCamera() const;
        PointLightComponent *tryGetPointLight() const;
        PostProcessStackComponent *tryGetPostProcessStack() const;
        WaterComponent *tryGetWater() const;

    private:
        Scene &requireScene() const;

        Scene *scene = nullptr;
        EntityId entityId = invalidEntity;
    };
}