#pragma once

// CPU-side vertex format. Headless by design (no Vulkan): the asset importer
// and mesh data (Assets/Import/) depend on it, so it must not pull in GPU
// headers. The GPU binding/attribute descriptions that used to live here were
// dead code (only referenced in a comment) and were removed.

#include <glm/glm.hpp>
#include <array>
#include <vector>

#include <glm/gtx/hash.hpp>

namespace Faye
{
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};

        Vertex() = default;
        Vertex(glm::vec3 pos) : pos{pos} {}
        Vertex(glm::vec3 pos, glm::vec3 color) : pos{pos}, color{color} {}
        bool operator==(const Vertex &other) const
        {
            return pos == other.pos && color == other.color && normal == other.normal && uv == other.uv && tangent == other.tangent;
        }
    };
} // namespace Faye
