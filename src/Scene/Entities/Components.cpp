#include "Scene/Entities/Components.hpp"

namespace Faye
{
    glm::mat4 TransformComponent::mat4() const
    {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        return glm::mat4{
            {
                scale.x * (c1 * c3 + s1 * s2 * s3),
                scale.x * (c2 * s3),
                scale.x * (c1 * s2 * s3 - c3 * s1),
                0.0f,
            },
            {
                scale.y * (c3 * s1 * s2 - c1 * s3),
                scale.y * (c2 * c3),
                scale.y * (c1 * c3 * s2 + s1 * s3),
                0.0f,
            },
            {
                scale.z * (c2 * s1),
                scale.z * (-s2),
                scale.z * (c1 * c2),
                0.0f,
            },
            {translation.x, translation.y, translation.z, 1.0f}};
    }

    glm::mat3 TransformComponent::normalMatrix() const
    {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        const glm::vec3 inverseScale = 1.0f / scale;

        return glm::mat3{
            {
                inverseScale.x * (c1 * c3 + s1 * s2 * s3),
                inverseScale.x * (c2 * s3),
                inverseScale.x * (c1 * s2 * s3 - c3 * s1),
            },
            {
                inverseScale.y * (c3 * s1 * s2 - c1 * s3),
                inverseScale.y * (c2 * c3),
                inverseScale.y * (c1 * c3 * s2 + s1 * s3),
            },
            {
                inverseScale.z * (c2 * s1),
                inverseScale.z * (-s2),
                inverseScale.z * (c1 * c2),
            },
        };
    }

    // The primary-camera invariant has TWO halves, and only one of them used to
    // live here. The hand-written inspector drawer enforced the other by never
    // offering demotion at all -- it wired the checkbox to a local copy and
    // acted only on promotion. A generic drawer cannot know that, so the rule
    // moves to where it belongs: every path that writes `primary` (inspector,
    // deserializer, script) now gets both halves.
    void onCameraFieldChanged(void *component, const Ecs::FieldDescriptor &field, Ecs::World &world, Ecs::Entity entity)
    {
        if (field.offset != offsetof(CameraComponent, primary)) return;

        auto *edited = static_cast<CameraComponent *>(component);

        if (edited->primary)
        {
            // Promotion: exactly one camera holds the role, so demote the rest.
            for (const uint32_t cameraEntityId : Ecs::denseEntitiesOf<CameraComponent>(world))
            {
                const Ecs::Entity cam = world.entityAt(cameraEntityId);
                if (cam == entity) continue;
                if (auto *c = world.tryGet<CameraComponent>(cam)) c->primary = false;
            }
            return;
        }

        // Demotion: allowed only if some other camera already holds the role.
        // Otherwise the scene would be left with cameras but nothing to render
        // through, and there is no UI gesture that gets it back -- so refuse
        // the edit rather than repair it after the fact.
        for (const uint32_t cameraEntityId : Ecs::denseEntitiesOf<CameraComponent>(world))
        {
            const Ecs::Entity cam = world.entityAt(cameraEntityId);
            if (cam == entity) continue;
            const auto *c = world.tryGet<CameraComponent>(cam);
            if (c != nullptr && c->primary) return;
        }
        edited->primary = true;
    }
}