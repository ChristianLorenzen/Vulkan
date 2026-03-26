#pragma once

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    struct RenderExtractionContext
    {
        const Scene &scene;
        ModelRegistry &modelRegistry;
        RenderSceneSnapshot &snapshot;
    };
}