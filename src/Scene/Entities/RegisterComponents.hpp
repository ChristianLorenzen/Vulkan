#pragma once

namespace Faye
{
    namespace Ecs
    {
        class World;
    }

    // Fills the World's component-type registry. Called once per World, on the
    // main thread, before any entities exist — first-use componentId numbering
    // then follows registration order, deterministic per build.
    //
    // Adding a component type to the engine = one struct + one line here
    // (+ optionally a drawer in Editor/ImGui/InspectorDrawers.cpp).
    void registerEngineComponents(Ecs::World &world);
}
