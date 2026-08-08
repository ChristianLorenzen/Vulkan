#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "engine/Assets/Import/ModelImporter.hpp"
#include "Core/Handles/PrimitiveType.hpp"

using namespace Faye;

TEST_CASE("makePrimitiveMesh produces expected mesh topology")
{
    const ModelMeshData cube = Assets::makePrimitiveMesh(PrimitiveType::Cube);
    REQUIRE(cube.meshes.size() == 1);
    CHECK(cube.meshes[0].vertices.size() == 24);
    CHECK(cube.meshes[0].indices.size() == 36);

    const ModelMeshData plane = Assets::makePrimitiveMesh(PrimitiveType::Plane);
    REQUIRE(plane.meshes.size() == 1);
    CHECK(plane.meshes[0].vertices.size() == 4);
    CHECK(plane.meshes[0].indices.size() == 6);

    // Sphere: (sectors+1) * (stacks+1) vertices, sectors * stacks * 6 indices.
    const ModelMeshData sphere = Assets::makePrimitiveMesh(PrimitiveType::Sphere);
    REQUIRE(sphere.meshes.size() == 1);
    CHECK(sphere.meshes[0].vertices.size() == 25 * 17);
    CHECK(sphere.meshes[0].indices.size() == 24 * 16 * 6);

    // WaterPlane: subdivided (divisions+1)^2 vertices.
    const ModelMeshData water = Assets::makePrimitiveMesh(PrimitiveType::WaterPlane, 8);
    REQUIRE(water.meshes.size() == 1);
    CHECK(water.meshes[0].vertices.size() == 9 * 9);
    CHECK(water.meshes[0].indices.size() == 8 * 8 * 6);

    // Primitives carry no imported materials and no node hierarchy.
    CHECK(cube.materials.empty());
    CHECK(cube.nodes.empty());
}

TEST_CASE("importModelFromFile parses a simple OBJ headlessly")
{
    namespace fs = std::filesystem;

    const fs::path objPath = fs::temp_directory_path() / "faye_importer_triangle_test.obj";
    fs::remove(objPath); // clear any stale copy
    {
        std::ofstream out(objPath);
        out << "o Triangle\n"
            << "v 0 0 0\n"
            << "v 1 0 0\n"
            << "v 0 1 0\n"
            << "f 1 2 3\n";
    }

    const ModelMeshData meshData = Assets::importModelFromFile(objPath.string());

    CHECK(meshData.meshes.size() == 1);
    REQUIRE(!meshData.meshes.empty());
    CHECK(meshData.meshes[0].vertices.size() == 3);
    CHECK(meshData.meshes[0].indices.size() == 3);
    // A material-less OBJ still gets Assimp's default material (index 0).
    CHECK(meshData.meshes[0].materialIndex == 0);
    CHECK(meshData.materials.size() >= 1);
    CHECK(meshData.rootNodeIndex != ~0u);

    fs::remove(objPath);
}
