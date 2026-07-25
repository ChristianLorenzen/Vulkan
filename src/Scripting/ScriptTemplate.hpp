#pragma once

// Rename CustomScriptTemplate to your script's name, then replace the TODOs
// with the components and behavior the script owns.
//
// Expected project includes (adjust paths/names to match your codebase):
// #include "IScript.hpp"
// #include "Entity.hpp"
// #include "Scene.hpp"
// #include "EngineContext.hpp"
// #include "components/YourComponent.hpp"

namespace Faye
{
    /// A reusable starting point for an entity-attached built-in script.
    ///
    /// Typical attachment:
    ///   scriptSystem.attachBuiltinScript(entity,
    ///       new CustomScriptTemplate(/* optional dependencies */),
    ///       "CustomScript");
    class CustomScriptTemplate final : public IScript
    {
    public:
        // Add constructor parameters for dependencies supplied by Engine,
        // such as callbacks, registries, or shared configuration.
        CustomScriptTemplate() = default;

        void onStart(Entity entity, Scene * /*scene*/) override
        {
            if (!entity.isValid())
                return;

            // TODO: Read initial component state and cache it if needed.
            // if (auto *component = entity.tryGet<YourComponent>())
            //     lastValue = component->value;
        }

        void onUpdate(Entity entity, Scene * /*scene*/, const EngineContext & /*ctx*/) override
        {
            if (!entity.isValid())
                return;

            // TODO: Fetch the component this script watches or controls.
            // auto *component = entity.tryGet<YourComponent>();
            // if (component == nullptr)
            //     return;

            // TODO: Put per-frame behavior or change detection here.
            // if (component->value == lastValue)
            //     return;
            //
            // lastValue = component->value;
            // ...perform the script action...
        }

        // Uncomment only if your IScript interface supports lifecycle hooks
        // beyond start/update, and override their exact signatures.
        // void onDestroy(Entity entity, Scene *scene) override;

    private:
        // TODO: Store script-local state only. Component data belongs on Entity.
        // uint32_t lastValue = 0;
    };

} // namespace Faye