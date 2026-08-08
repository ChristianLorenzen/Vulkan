#pragma once

// CPU-side mesh data shared by the asset importer and the GPU Model.
// Headless by design: no Vulkan, no GLFW, no ImGui — unit-testable without a
// device. This is the seam a future cooked-asset pipeline will write to and
// read from (import in the editor, load pre-cooked data at runtime).

#include <cstdint>
#include <string>
#include <vector>

#include "Renderer/Material/Material.hpp"
#include "Renderer/Resources/Vertex.hpp"

namespace Faye
{
    // One drawable mesh: vertex/index data plus the imported material slot.
    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        uint32_t materialIndex = 0;
    };

    // Node in the imported model's transform hierarchy. Submesh indices map
    // 1:1 to ModelMeshData::meshes entries.
    struct NodeData
    {
        std::string name;
        std::vector<uint32_t> meshDataIndices;
        std::vector<uint32_t> childNodeIndices;
    };

    // Container for an imported (or procedurally generated) model in CPU space.
    // Produced by Assets/Import/ModelImporter, consumed by Renderer/Resources/Model.
    struct ModelMeshData
    {
        std::vector<Mesh> meshes;
        std::vector<MaterialData> materials;
        std::string directory;
        std::vector<NodeData> nodes;
        uint32_t rootNodeIndex = ~0u;
    };
} // namespace Faye
