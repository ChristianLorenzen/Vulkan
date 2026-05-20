#include "SceneManager.hpp"

#include <stdexcept>

using namespace Faye;

Faye::Scene &Faye::SceneManager::createScene(const std::string &sceneName)
{
    activeScene = std::make_unique<Scene>(sceneName);
    return *activeScene;
}

Faye::Scene &Faye::SceneManager::getActiveScene()
{
    if (activeScene == nullptr)
    {
        throw std::runtime_error("No active scene exists");
    }

    return *activeScene;
}

const Faye::Scene &Faye::SceneManager::getActiveScene() const
{
    if (activeScene == nullptr)
    {
        throw std::runtime_error("No active scene exists");
    }

    return *activeScene;
}