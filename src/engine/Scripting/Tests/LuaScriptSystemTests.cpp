// Phase 5 risk-register tests (docs/ecs/07-integration-and-migration.md
// §Phase 5) for the Lua path: "destroy-while-script-attached, reload-then-
// destroy". Real sol2 execution against a temp .lua file — no dlopen needed,
// since Lua scripts are plain text.
//
// Each script phase appends a marker line to a plain text file via Lua's
// `io` library (already opened in LuaScriptSystem's constructor), rather than
// poking a component: World::destroy sweeps component pools in
// registration order, so by the time the LuaScriptComponent pool's remove
// hook runs onDestroy, an earlier-registered component like Transform may
// already be gone. A marker file has no such ordering dependency.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "engine/Scene/Scene.hpp"
#include "engine/Scripting/LuaScriptSystem.hpp"
using namespace Faye;

namespace
{
    class MarkerFile
    {
    public:
        MarkerFile()
            : path(std::filesystem::temp_directory_path() /
                   ("faye_lua_test_" + std::to_string(std::random_device{}()) + ".log"))
        {
        }
        ~MarkerFile() { std::error_code ec; std::filesystem::remove(path, ec); }

        MarkerFile(const MarkerFile &) = delete;
        MarkerFile &operator=(const MarkerFile &) = delete;

        std::string generic() const { return path.generic_string(); }

        std::vector<std::string> lines() const
        {
            std::vector<std::string> result;
            std::ifstream in(path);
            std::string line;
            while (std::getline(in, line))
                if (!line.empty())
                    result.push_back(line);
            return result;
        }

    private:
        std::filesystem::path path;
    };

    class TempLuaScript
    {
    public:
        explicit TempLuaScript(const std::string &markerPathGeneric)
            : path(std::filesystem::temp_directory_path() /
                   ("faye_lua_script_" + std::to_string(std::random_device{}()) + ".lua"))
        {
            std::ofstream out(path);
            out << "local function mark(event)\n"
                   "    local f = io.open(\""
                << markerPathGeneric << "\", \"a\")\n"
                   "    if f then f:write(event .. \"\\n\") f:close() end\n"
                   "end\n"
                   "function onStart(entity) mark(\"START\") end\n"
                   "function onUpdate(entity, dt) mark(\"UPDATE\") end\n"
                   "function onDestroy(entity) mark(\"DESTROY\") end\n";
        }
        ~TempLuaScript() { std::error_code ec; std::filesystem::remove(path, ec); }

        TempLuaScript(const TempLuaScript &) = delete;
        TempLuaScript &operator=(const TempLuaScript &) = delete;

        std::string generic() const { return path.generic_string(); }

    private:
        std::filesystem::path path;
    };
}

TEST_CASE("Lua loadScript/update/unloadScript run onStart/onUpdate/onDestroy in order")
{
    MarkerFile marker;
    TempLuaScript script(marker.generic());

    Scene scene;
    LuaScriptSystem lua;
    lua.bindScene(&scene);
    lua.bindEngineAPI();

    Entity entity = scene.createEntity();
    lua.loadScript(entity, script.generic(), &scene);
    CHECK(lua.isLoaded(entity));
    CHECK(marker.lines() == std::vector<std::string>{"START"});

    EngineContext ctx;
    ctx.dt = 1.0f / 60.0f;
    lua.update(ctx, &scene);
    CHECK(marker.lines() == std::vector<std::string>{"START", "UPDATE"});

    lua.unloadScript(entity, &scene);
    CHECK_FALSE(lua.isLoaded(entity));
    CHECK(marker.lines() == std::vector<std::string>{"START", "UPDATE", "DESTROY"});
}

TEST_CASE("Lua reload-then-destroy: reload tears down the old sandbox once, "
          "destroy tears down the reloaded one")
{
    MarkerFile marker;
    TempLuaScript script(marker.generic());

    Scene scene;
    LuaScriptSystem lua;
    lua.bindScene(&scene);
    lua.bindEngineAPI();

    Entity entity = scene.createEntity();
    lua.loadScript(entity, script.generic(), &scene);
    CHECK(marker.lines() == std::vector<std::string>{"START"});

    lua.reloadScript(entity, &scene);   // unload (DESTROY) + load (START)
    CHECK(marker.lines() == std::vector<std::string>{"START", "DESTROY", "START"});

    entity.destroy();                  // World::destroy sweeps LuaScriptComponent
    CHECK(marker.lines() == std::vector<std::string>{"START", "DESTROY", "START", "DESTROY"});
    CHECK_FALSE(scene.getWorld().has<LuaScriptComponent>(entity.handle()));
}

TEST_CASE("destroy-while-script-attached (Lua): entity destruction tears the script down exactly once")
{
    MarkerFile marker;
    TempLuaScript script(marker.generic());

    Scene scene;
    LuaScriptSystem lua;
    lua.bindScene(&scene);
    lua.bindEngineAPI();

    Entity entity = scene.createEntity();
    lua.loadScript(entity, script.generic(), &scene);
    REQUIRE(lua.isLoaded(entity));

    entity.destroy();

    CHECK(marker.lines() == std::vector<std::string>{"START", "DESTROY"});
    CHECK_FALSE(entity.isValid());
}

TEST_CASE("Lua unloadAll tears down every attached script")
{
    MarkerFile markerA;
    MarkerFile markerB;
    TempLuaScript scriptA(markerA.generic());
    TempLuaScript scriptB(markerB.generic());

    Scene scene;
    LuaScriptSystem lua;
    lua.bindScene(&scene);
    lua.bindEngineAPI();

    Entity ea = scene.createEntity();
    Entity eb = scene.createEntity();
    lua.loadScript(ea, scriptA.generic(), &scene);
    lua.loadScript(eb, scriptB.generic(), &scene);

    lua.unloadAll();

    CHECK(markerA.lines() == std::vector<std::string>{"START", "DESTROY"});
    CHECK(markerB.lines() == std::vector<std::string>{"START", "DESTROY"});
    CHECK_FALSE(scene.getWorld().has<LuaScriptComponent>(ea.handle()));
    CHECK_FALSE(scene.getWorld().has<LuaScriptComponent>(eb.handle()));
}
