#include <doctest/doctest.h>

#include <string>

#include "Assets/AssetDatabase.hpp"

using Faye::AssetDatabase;
using Faye::AssetId;
using Faye::AssetPersistenceMode;
using Faye::AssetRecord;
using Faye::AssetType;
using Faye::PrimitiveType;

TEST_CASE("idForSourceUri is deterministic and distinct per uri")
{
    const AssetId a1 = AssetDatabase::idForSourceUri("assets/projects/models/ship.fbx");
    const AssetId a2 = AssetDatabase::idForSourceUri("assets/projects/models/ship.fbx");
    const AssetId b = AssetDatabase::idForSourceUri("assets/projects/models/rock.fbx");

    CHECK(a1 == a2);                       // stable across runs/machines
    CHECK(a1 != b);
    CHECK_FALSE(a1.isNull());
}

TEST_CASE("idForPrimitive is deterministic and distinct per type")
{
    const AssetId cube1 = AssetDatabase::idForPrimitive(PrimitiveType::Cube);
    const AssetId cube2 = AssetDatabase::idForPrimitive(PrimitiveType::Cube);
    const AssetId plane = AssetDatabase::idForPrimitive(PrimitiveType::Plane);

    CHECK(cube1 == cube2);
    CHECK(cube1 != plane);
    // Primitive ids live in a different namespace than source-uri ids.
    CHECK(cube1 != AssetDatabase::idForSourceUri("Cube"));
}

TEST_CASE("idForBuiltIn is deterministic and distinct per name")
{
    const AssetId a1 = AssetDatabase::idForBuiltIn("Default Material");
    const AssetId a2 = AssetDatabase::idForBuiltIn("Default Material");
    const AssetId water = AssetDatabase::idForBuiltIn("Water Material");

    CHECK(a1 == a2);
    CHECK(a1 != water);
}

TEST_CASE("registerAsset/findByAssetId round-trips")
{
    AssetDatabase db;
    const AssetId id = AssetDatabase::idForBuiltIn("Test");

    db.registerAsset(AssetRecord{
        .id = id,
        .type = AssetType::Material,
        .name = "Test",
        .persistence = AssetPersistenceMode::BuiltIn,
    });

    CHECK(db.contains(id));
    const AssetRecord *record = db.findByAssetId(id);
    REQUIRE(record != nullptr);
    CHECK(record->name == "Test");
    CHECK(record->type == AssetType::Material);
    CHECK(record->persistence == AssetPersistenceMode::BuiltIn);
}

TEST_CASE("findBySourceUri resolves an imported asset record")
{
    AssetDatabase db;
    const std::string uri = "assets/projects/models/ship.fbx";
    const AssetId id = AssetDatabase::idForSourceUri(uri);

    db.registerAsset(AssetRecord{
        .id = id,
        .type = AssetType::Model,
        .name = "ship.fbx",
        .sourceUri = uri,
        .persistence = AssetPersistenceMode::Imported,
    });

    const AssetRecord *record = db.findBySourceUri(uri);
    REQUIRE(record != nullptr);
    CHECK(record->id == id);
    CHECK(record->persistence == AssetPersistenceMode::Imported);
}

TEST_CASE("re-registering an id upserts without duplicating registration order")
{
    AssetDatabase db;
    const AssetId id = AssetDatabase::idForBuiltIn("Default Material");

    db.registerAsset(AssetRecord{.id = id, .type = AssetType::Material, .name = "Default Material"});
    db.registerAsset(AssetRecord{
        .id = id,
        .type = AssetType::Material,
        .name = "Default Material (enriched)",
        .persistence = AssetPersistenceMode::BuiltIn,
    });

    const AssetRecord *record = db.findByAssetId(id);
    REQUIRE(record != nullptr);
    CHECK(record->name == "Default Material (enriched)");
    CHECK(record->persistence == AssetPersistenceMode::BuiltIn);

    const auto ids = db.getAllAssetIds();
    CHECK(ids.size() == 1);               // upsert, not append
    CHECK(ids[0] == id);
}

TEST_CASE("setDirty toggles the dirty flag")
{
    AssetDatabase db;
    const AssetId id = AssetDatabase::idForBuiltIn("Test");
    db.registerAsset(AssetRecord{.id = id, .type = AssetType::Model, .name = "Test"});

    db.setDirty(id, true);
    CHECK(db.findByAssetId(id)->dirty);
    db.setDirty(id, false);
    CHECK_FALSE(db.findByAssetId(id)->dirty);
}
