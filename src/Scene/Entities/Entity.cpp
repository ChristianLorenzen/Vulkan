#include "Scene/Entities/Entity.hpp"

#include <stdexcept>

#include "Scene/Scene.hpp"

namespace Faye
{
    bool Entity::isValid() const
    {
        return scene != nullptr && scene->isValid(entityHandle);
    }

    std::optional<Uuid> Entity::guid() const
    {
        if (scene == nullptr || !scene->getWorld().alive(entityHandle))
            return std::nullopt;
        return scene->getWorld().guidOf(entityHandle);
    }

    std::string_view Entity::getName() const
    {
        return scene != nullptr ? scene->getEntityName(entityHandle) : std::string_view{};
    }

    void Entity::setName(std::string name) const
    {
        requireScene().setEntityName(entityHandle, std::move(name));
    }

    void Entity::destroy()
    {
        requireScene().destroyEntity(entityHandle);
        entityHandle = Ecs::Entity::null();
    }

    void Entity::setPrimaryCamera() const
    {
        requireScene().setPrimaryCamera(*this);
    }

    MeshRendererComponent &Entity::addMesh(ModelHandle modelHandle) const
    {
        return requireScene().addMesh(entityHandle, modelHandle);
    }

    CameraComponent &Entity::addCamera(bool primary) const
    {
        return requireScene().addCamera(entityHandle, primary);
    }

    Scene &Entity::requireScene() const
    {
        if (scene == nullptr)
        {
            throw std::runtime_error("Attempted to use an entity facade without an owning scene");
        }

        return *scene;
    }
}
