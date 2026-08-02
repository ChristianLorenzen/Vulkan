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

    void onUpdate(Entity entity, Scene * /*scene*/, const EngineContext &ctx) override
    {
        if (!entity.isValid())
            return;

        auto *transform = entity.tryGet<TransformComponent>();
        if (transform == nullptr)
            return;

        // Rotate 90 degrees per second around the Y axis, framerate-independent.
        // rotation.y is in radians — convert speed from degrees first.
        constexpr float kSpeed = glm::radians(90.0f);  // ~1.57 rad/s
        transform->rotation.y += kSpeed * ctx.dt;
    }

    void onDestroy(Entity entity, Scene * /*scene*/) override
    {
        (void)entity;
    }
};

FAYE_REGISTER_SCRIPT(RotatorScript)
