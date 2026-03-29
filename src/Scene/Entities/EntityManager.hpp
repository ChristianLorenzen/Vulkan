#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Scene/Entities/Components.hpp"

namespace Faye
{
    using EntityId = uint32_t;
    inline constexpr EntityId invalidEntity = 0;
    using ComponentMask = uint64_t;

    struct EntityMetadata
    {
        std::string name{};
    };

    enum class ComponentKind : uint8_t
    {
        Transform = 0,
        RigidBody2d,
        Mesh,
        Camera,
        PointLight,
        Count,
    };

    constexpr ComponentMask componentBit(ComponentKind kind)
    {
        return ComponentMask{1} << static_cast<uint8_t>(kind);
    }

    inline constexpr const char *componentKindName(ComponentKind kind)
    {
        switch (kind)
        {
        case ComponentKind::Transform:
            return "Transform";
        case ComponentKind::RigidBody2d:
            return "RigidBody2D";
        case ComponentKind::Mesh:
            return "Mesh";
        case ComponentKind::Camera:
            return "Camera";
        case ComponentKind::PointLight:
            return "Point Light";
        case ComponentKind::Count:
            break;
        }

        return "Unknown";
    }

    class EntityManager
    {
    public:
        EntityId createEntity(std::string name = {});
        void destroyEntity(EntityId entity);
        bool isValid(EntityId entity) const;

        const std::vector<EntityId> &getEntities() const { return entities; }

        void setEntityName(EntityId entity, std::string name);
        std::string_view getEntityName(EntityId entity) const;
        const EntityMetadata *tryGetEntityMetadata(EntityId entity) const;

        ComponentMask getComponentMask(EntityId entity) const;
        bool hasComponent(EntityId entity, ComponentKind kind) const;
        std::vector<ComponentKind> getComponentKinds(EntityId entity) const;

        TransformComponent &addTransform(EntityId entity);
        RigidBody2dComponent &addRigidBody2d(EntityId entity);
        MeshComponent &addMesh(EntityId entity, ModelHandle modelHandle = {});
        CameraComponent &addCamera(EntityId entity);
        PointLightComponent &addPointLight(EntityId entity);

        void removeTransform(EntityId entity);
        void removeRigidBody2d(EntityId entity);
        void removeMesh(EntityId entity);
        void removeCamera(EntityId entity);
        void removePointLight(EntityId entity);

        TransformComponent *tryGetTransform(EntityId entity);
        const TransformComponent *tryGetTransform(EntityId entity) const;

        RigidBody2dComponent *tryGetRigidBody2d(EntityId entity);
        const RigidBody2dComponent *tryGetRigidBody2d(EntityId entity) const;

        MeshComponent *tryGetMesh(EntityId entity);
        const MeshComponent *tryGetMesh(EntityId entity) const;

        CameraComponent *tryGetCamera(EntityId entity);
        const CameraComponent *tryGetCamera(EntityId entity) const;

        PointLightComponent *tryGetPointLight(EntityId entity);
        const PointLightComponent *tryGetPointLight(EntityId entity) const;

    private:
        void requireEntity(EntityId entity) const;
        std::string makeDefaultEntityName(EntityId entity) const;
        void setComponent(EntityId entity, ComponentKind kind);
        void clearComponent(EntityId entity, ComponentKind kind);

        EntityId nextEntityId = 1;
        std::vector<EntityId> entities;
        std::unordered_set<EntityId> entityLookup;
        std::unordered_map<EntityId, EntityMetadata> entityMetadata;
        std::unordered_map<EntityId, ComponentMask> componentMasks;
        std::unordered_map<EntityId, TransformComponent> transforms;
        std::unordered_map<EntityId, RigidBody2dComponent> rigidBody2dComponents;
        std::unordered_map<EntityId, MeshComponent> meshComponents;
        std::unordered_map<EntityId, CameraComponent> cameraComponents;
        std::unordered_map<EntityId, PointLightComponent> pointLightComponents;
    };
}