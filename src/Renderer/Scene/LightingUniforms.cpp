#include "Renderer/Scene/LightingUniforms.hpp"

#include <algorithm>

#include "Scene/Camera/Camera.hpp"

namespace Faye
{
    void packSceneLighting(const RenderSceneSnapshot &snapshot, SceneLightingUBO &lighting)
    {
        // Point lights: position from the transform, colour.w carries intensity.
        // Clamp to the UBO capacity (extra lights are dropped, not overflowed).
        const int pointCount =
            std::min(static_cast<int>(snapshot.pointLights.size()), MAX_POINT_LIGHTS);
        for (int i = 0; i < pointCount; ++i)
        {
            const PointLightInstance &src = snapshot.pointLights[i];
            lighting.pointLights[i].position = glm::vec4(src.transform->translation, 1.0f);
            lighting.pointLights[i].color = glm::vec4(src.color, src.intensity);
        }
        lighting.numPointLights = pointCount;

        // Directional lights: direction is the entity's forward vector (the way
        // the light travels), derived from the Transform rotation. Shaders negate
        // it to get the direction toward the light.
        const int dirCount =
            std::min(static_cast<int>(snapshot.directionalLights.size()), MAX_DIRECTIONAL_LIGHTS);
        for (int i = 0; i < dirCount; ++i)
        {
            const DirectionalLightInstance &src = snapshot.directionalLights[i];
            const glm::vec3 direction = Camera::forwardFromRotation(src.transform->rotation);
            lighting.directionalLights[i].direction = glm::vec4(direction, 0.0f);
            lighting.directionalLights[i].color = glm::vec4(src.color, src.intensity);
        }
        lighting.numDirectionalLights = dirCount;
    }
}
