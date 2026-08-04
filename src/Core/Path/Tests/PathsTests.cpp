#include <doctest/doctest.h>

#include <string>

#include "Core/Path/Paths.hpp"

using Faye::Paths;

TEST_CASE("toProjectRelative rebases paths inside the repo")
{
    const std::string absolute = (Paths::root() / "assets" / "models" / "ship.fbx").generic_string();
    CHECK(Paths::toProjectRelative(absolute) == "assets/models/ship.fbx");
}

TEST_CASE("toProjectRelative leaves already-relative paths alone")
{
    CHECK(Paths::toProjectRelative("assets/models/ship.fbx") == "assets/models/ship.fbx");
    CHECK(Paths::toProjectRelative("bin/libscript.so") == "bin/libscript.so");
    CHECK(Paths::toProjectRelative("") == "");
}

TEST_CASE("toProjectRelative keeps paths outside the repo absolute")
{
    // No portable form exists for these, so they must not be rewritten into
    // a "../.." chain that breaks the moment the repo moves.
    const std::string outside = (Paths::root().parent_path() / "elsewhere" / "ship.fbx").generic_string();
    CHECK(Paths::toProjectRelative(outside) == outside);
}

TEST_CASE("fromProjectRelative resolves against the repo root")
{
    const std::string expected = (Paths::root() / "assets" / "models" / "ship.fbx").generic_string();
    CHECK(Paths::fromProjectRelative("assets/models/ship.fbx") == expected);
    CHECK(Paths::fromProjectRelative(expected) == expected);
    CHECK(Paths::fromProjectRelative("") == "");
}

TEST_CASE("project-relative conversion round trips")
{
    const std::string relative = "src/textures/waternormal1.jpg";
    CHECK(Paths::toProjectRelative(Paths::fromProjectRelative(relative)) == relative);
}
