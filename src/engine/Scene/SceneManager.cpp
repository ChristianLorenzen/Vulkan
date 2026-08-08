#include "SceneManager.hpp"

#include <fstream>
#include <stdexcept>

#include "Core/Logging/Logger.hpp"
#include "engine/Scene/Serialization/SceneFileWriter.hpp"
#include "engine/Scripting/LuaScriptSystem.hpp"
#include "engine/Scripting/ScriptSystem.hpp"
#include "quill/LogMacros.h"

using namespace Faye;

Faye::SceneManager::SceneManager(ModelRegistry &modelRegistry, MaterialRegistry &materialRegistry, AssetDatabase &assetDatabase)
    : modelRegistry(modelRegistry), materialRegistry(materialRegistry), assetDatabase(assetDatabase),
      renderExtractionManager(modelRegistry, materialRegistry) {}

void Faye::SceneManager::OnInit()
{
    LOG_INFO(Logger::get(), "SceneManager OnInit");

    createScene("Main Scene");
}

void Faye::SceneManager::bindScriptEngines(ScriptSystem &scripts, LuaScriptSystem &luaScripts)
{
    scriptSystem = &scripts;
    luaScriptSystem = &luaScripts;
}

std::string Faye::SceneManager::saveSceneToFile(const std::string &path)
{
    const std::string yaml = SceneFileWriter::write(*activeScene, modelRegistry, materialRegistry, assetDatabase);
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file)
    {
        return "Failed to open '" + path + "' for writing";
    }
    file << yaml;
    if (!file)
    {
        return "Failed to write scene to '" + path + "'";
    }
    scenePath = path;
    return "";
}

std::string Faye::SceneManager::saveSceneAsToFile(const std::string &path)
{
    if (activeScene != nullptr && sceneSaveNeedsNewUuid(scenePath, path))
    {
        activeScene->regenerateSceneUuid();
    }
    return saveSceneToFile(path);
}

Faye::SceneFileLoadResult Faye::SceneManager::loadSceneFromFile(const std::string &path)
{
    if (scriptSystem == nullptr || luaScriptSystem == nullptr)
    {
        return SceneFileLoadResult{false, "SceneManager has no script engines bound"};
    }

    std::ifstream file(path);
    if (!file)
    {
        return SceneFileLoadResult{false, "Failed to open '" + path + "' for reading"};
    }
    const std::string yaml((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Fill-in-place: the Scene object survives, so panels / script engines /
    // extraction stay bound. All entities are destroyed through the normal
    // path (remove hooks fire) before the file is replayed.
    activeScene->clear();
    SceneFileLoadResult result = SceneFileReader::read(*activeScene, yaml, modelRegistry, materialRegistry,
                                                       assetDatabase, *scriptSystem, *luaScriptSystem);
    if (result.success)
    {
        scenePath = path;
    }
    else
    {
        LOG_ERROR(Logger::get(), "Scene load failed for {}: {}", path, result.error);
    }
    return result;
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
