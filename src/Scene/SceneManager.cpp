#include "SceneManager.hpp"

#include <stdexcept>
#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

using namespace Faye;

Faye::SceneManager::SceneManager(ModelRegistry &modelRegistry, MaterialRegistry &materialRegistry)
    : modelRegistry(modelRegistry), materialRegistry(materialRegistry) {}

void Faye::SceneManager::OnInit()
{
    LOG_INFO(Logger::get(), "SceneManager OnInit");

    createScene("Main Scene");
}

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
