#pragma once

// Headless model import (Assimp) + procedural primitive generation.
// Lives under Assets/Import/ (not Renderer/) so it can be unit-tested without
// a Vulkan device — see Assets/Import/ModelImporter.cpp and the faye_tests
// target. GPU upload happens later in Renderer/Resources/Model.

#include <cstdint>
#include <string>

#include "engine/Assets/Import/ModelMeshData.hpp"
#include "Core/Handles/PrimitiveType.hpp"

namespace Faye::Assets
{
    // Imports an Assimp-supported model file into CPU mesh data. Headless.
    // Throws std::runtime_error on load failure.
    ModelMeshData importModelFromFile(const std::string &modelPath);

    // Generates a procedural primitive (cube, sphere, plane, capsule, water
    // plane). `subdivisions` only affects WaterPlane.
    ModelMeshData makePrimitiveMesh(PrimitiveType primitiveType, uint32_t subdivisions = 64);
} // namespace Faye::Assets
