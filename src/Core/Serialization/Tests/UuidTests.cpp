#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "Core/Serialization/Uuid.hpp"

using Faye::Uuid;

TEST_CASE("default Uuid is the null uuid")
{
    const Uuid uuid;
    CHECK(uuid.isNull());
    CHECK(uuid.toString() == "00000000-0000-0000-0000-000000000000");
}

TEST_CASE("generateV4 yields non-null, distinct uuids")
{
    const Uuid a = Uuid::generateV4();
    const Uuid b = Uuid::generateV4();
    CHECK_FALSE(a.isNull());
    CHECK_FALSE(b.isNull());
    CHECK(a != b);
}

TEST_CASE("uuid round-trips through its canonical string form")
{
    const Uuid uuid = Uuid::generateV4();
    const std::string text = uuid.toString();

    // canonical shape: 8-4-4-4-12 lowercase hex
    CHECK(text.size() == 36);
    CHECK(text[8] == '-');
    CHECK(text[13] == '-');
    CHECK(text[18] == '-');
    CHECK(text[23] == '-');

    const Uuid parsed = Uuid::fromString(text);
    CHECK(parsed == uuid);
}

TEST_CASE("fromString rejects malformed input")
{
    CHECK_THROWS_AS(Uuid::fromString("not-a-uuid"), std::runtime_error);
    CHECK_THROWS_AS(Uuid::fromString(""), std::runtime_error);
    CHECK_THROWS_AS(Uuid::fromString("f47ac10b-58cc-4372-a567"), std::runtime_error);
}

TEST_CASE("nameBased ids are deterministic and distinct per name")
{
    const Uuid a1 = Uuid::nameBased("assets/projects/models/ship.fbx");
    const Uuid a2 = Uuid::nameBased("assets/projects/models/ship.fbx");
    const Uuid b = Uuid::nameBased("assets/projects/models/rock.fbx");

    CHECK(a1 == a2);                       // same name -> same id (stable across runs)
    CHECK(a1 != b);                        // different name -> different id
    CHECK_FALSE(a1.isNull());
}

TEST_CASE("Uuid works as a hash key")
{
    std::unordered_map<Uuid, int> map;
    const Uuid key = Uuid::generateV4();
    map[key] = 42;
    CHECK(map[key] == 42);
    CHECK(map.find(Uuid::generateV4()) == map.end());
}
