#pragma once

#include <cstdint>
#include <functional>

#include "Core/EngineContext.hpp"
#include "Scripting/IScript.hpp"
#include "Scene/Entities/Entity.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    /// Built-in script that watches WaterComponent.subdivisions on its entity
    /// and rebuilds the water plane mesh whenever the value changes.
    ///
    /// The rebuildMesh callback is provided by Engine at entity-creation time.
    /// It accepts the desired subdivision count and returns a ModelHandle for
    /// the new mesh, which the script assigns back to the entity's mesh component.
    ///
    /// Usage (Engine.hpp):
    ///   scriptSystem.attachBuiltinScript(waterEntity,
    ///       new WaterSubdivisionScript([this](uint32_t divs) {
    ///           return modelRegistry->registerModel(
    ///               Model::createPrimitive(*vkData->getVkDevice(),
    ///                   PrimitiveType::WaterPlane, divs));
    ///       }), "WaterSubdivision");
    class WaterSubdivisionScript final : public IScript
    {
    public:
        using RebuildFn = std::function<ModelHandle(uint32_t subdivisions)>;

        explicit WaterSubdivisionScript(RebuildFn rebuildFn)
            : rebuildFn(std::move(rebuildFn))
        {
        }

        void onStart(Entity entity, Scene * /*scene*/) override
        {
            auto *water = entity.tryGet<WaterComponent>();
            if (water != nullptr)
            {
                lastSubdivisions = water->subdivisions;
            }
        }

        void onUpdate(Entity entity, Scene * /*scene*/, const EngineContext & /*ctx*/) override
        {
            if (!entity.isValid())
                return;

            auto *water = entity.tryGet<WaterComponent>();
            if (water == nullptr)
                return;

            // Only rebuild when the value actually changes.
            if (water->subdivisions == lastSubdivisions)
                return;

            lastSubdivisions = water->subdivisions;

            if (!rebuildFn)
                return;

            ModelHandle newHandle = rebuildFn(lastSubdivisions);
            if (!newHandle.isValid())
                return;

            auto *mesh = entity.tryGet<MeshRendererComponent>();
            if (mesh != nullptr)
            {
                mesh->modelHandle = newHandle;
            }
        }

    private:
        RebuildFn rebuildFn;
        uint32_t  lastSubdivisions = 0;
    };

} // namespace Faye
