#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Scene/Entities/Entity.hpp"
#include "Scene/Scene.hpp"
#include "Scripting/IScript.hpp"

using namespace Faye;

class RotatorScript final : public IScript
{
public:
    void onStart(Entity entity, Scene * /*scene*/) override
    {
        (void)entity;
    }

    void onUpdate(Entity entity, Scene * /*scene*/, float dt) override
    {
        if (!entity.isValid())
            return;

        auto *transform = entity.tryGetTransform();
        if (transform == nullptr)
            return;

        // Rotate 90 degrees per second around the Y axis.
        constexpr float kDegreesPerSecond = 90.0f;
        transform->rotation.y += kDegreesPerSecond * dt;
    }

    void onDestroy(Entity entity, Scene * /*scene*/) override
    {
        (void)entity;
    }
};

FAYE_REGISTER_SCRIPT(RotatorScript)
