#pragma once

#include <memory>
#include <string>

#include "Scene.hpp"

namespace Faye
{
    class SceneManager
    {
    public:
        SceneManager() = default;

        Scene &createScene(const std::string &sceneName = "Scene");
        bool hasActiveScene() const { return activeScene != nullptr; }

        Scene &getActiveScene();
        const Scene &getActiveScene() const;

    private:
        std::unique_ptr<Scene> activeScene;
    };
}