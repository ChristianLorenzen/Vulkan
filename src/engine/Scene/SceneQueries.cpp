#include "SceneQueries.hpp"

#include <algorithm>
#include <limits>

namespace Faye
{
    namespace
    {
        std::optional<float> intersectRayAabb(
            const glm::vec3 &origin,
            const glm::vec3 &direction,
            const glm::vec3 &minBounds,
            const glm::vec3 &maxBounds)
        {
            float tMin = 0.0f;
            float tMax = std::numeric_limits<float>::max();

            for (int axis = 0; axis < 3; ++axis)
            {
                if (glm::abs(direction[axis]) <= std::numeric_limits<float>::epsilon())
                {
                    if (origin[axis] < minBounds[axis] || origin[axis] > maxBounds[axis])
                    {
                        return std::nullopt;
                    }

                    continue;
                }

                const float inverseDirection = 1.0f / direction[axis];
                float t0 = (minBounds[axis] - origin[axis]) * inverseDirection;
                float t1 = (maxBounds[axis] - origin[axis]) * inverseDirection;

                if (t0 > t1)
                {
                    std::swap(t0, t1);
                }

                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);

                if (tMin > tMax)
                {
                    return std::nullopt;
                }
            }

            return tMin;
        }

        std::optional<float> intersectRaySphere(
            const glm::vec3 &origin,
            const glm::vec3 &direction,
            const glm::vec3 &center,
            float radius)
        {
            const glm::vec3 oc = origin - center;
            const float a = glm::dot(direction, direction);
            const float b = 2.0f * glm::dot(oc, direction);
            const float c = glm::dot(oc, oc) - radius * radius;
            const float discriminant = b * b - 4.0f * a * c;

            if (discriminant < 0.0f)
            {
                return std::nullopt;
            }

            const float sqrtDiscriminant = glm::sqrt(discriminant);
            const float inverseDenominator = 1.0f / (2.0f * a);
            const float t0 = (-b - sqrtDiscriminant) * inverseDenominator;
            const float t1 = (-b + sqrtDiscriminant) * inverseDenominator;

            if (t0 > 0.0f)
            {
                return t0;
            }

            if (t1 > 0.0f)
            {
                return t1;
            }

            return std::nullopt;
        }

        float extractMaxScale(const glm::vec3 &scale)
        {
            return std::max({glm::abs(scale.x), glm::abs(scale.y), glm::abs(scale.z)});
        }
    }

    std::optional<SceneRaycastHit> raycastScene(
        const Scene &scene,
        const ModelRegistry &modelRegistry,
        const Camera &camera,
        const glm::vec2 &viewportUv)
    {
        const glm::vec2 ndc{
            viewportUv.x * 2.0f - 1.0f,
            1.0f - viewportUv.y * 2.0f};

        const CameraRay ray = camera.rayFromNdc(ndc);
        std::optional<SceneRaycastHit> closestHit;
        float closestDistance = std::numeric_limits<float>::max();

        for (const Ecs::Entity entity : scene.getEntities())
        {
            const auto *transform = scene.tryGet<TransformComponent>(entity);
            if (transform == nullptr)
            {
                continue;
            }

            if (const auto *mesh = scene.tryGet<MeshRendererComponent>(entity))
            {
                const Model *model = modelRegistry.getModel(mesh->modelHandle);
                if (model != nullptr)
                {
                    const auto &bounds = model->getLocalBounds();
                    const glm::mat4 inverseTransform = glm::inverse(transform->mat4());
                    const glm::vec3 localOrigin = glm::vec3(inverseTransform * glm::vec4(ray.origin, 1.0f));
                    const glm::vec3 localDirection = glm::normalize(glm::vec3(inverseTransform * glm::vec4(ray.direction, 0.0f)));

                    if (auto hitDistance = intersectRayAabb(localOrigin, localDirection, bounds.min, bounds.max);
                        hitDistance.has_value())
                    {
                        const glm::vec3 hitPosition = localOrigin + localDirection * *hitDistance;
                        const glm::vec3 worldHitPosition = glm::vec3(transform->mat4() * glm::vec4(hitPosition, 1.0f));
                        const float worldDistance = glm::length(worldHitPosition - ray.origin);

                        if (worldDistance < closestDistance)
                        {
                            closestDistance = worldDistance;
                            closestHit = SceneRaycastHit{
                                entity,
                                worldDistance,
                                worldHitPosition};
                        }
                    }
                }
            }

            if (const auto *pointLight = scene.tryGet<PointLightComponent>(entity))
            {
                const float worldRadius = std::max(pointLight->radius * extractMaxScale(transform->scale), 0.05f);

                if (auto hitDistance = intersectRaySphere(ray.origin, ray.direction, transform->translation, worldRadius);
                    hitDistance.has_value() && *hitDistance < closestDistance)
                {
                    closestDistance = *hitDistance;
                    closestHit = SceneRaycastHit{
                        entity,
                        *hitDistance,
                        ray.origin + ray.direction * *hitDistance};
                }
            }
        }

        return closestHit;
    }
}