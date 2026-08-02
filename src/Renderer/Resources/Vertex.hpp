#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
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

        static std::vector<VkVertexInputBindingDescription> getBindingDescription()
        {
            std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
            bindingDescriptions[0].binding = 0;
            bindingDescriptions[0].stride = sizeof(Vertex);
            bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescriptions;
        }

        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
        {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, normal);

            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[3].offset = offsetof(Vertex, uv);

            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[4].offset = offsetof(Vertex, tangent);

            return attributeDescriptions;
        }
    };
}

namespace std
{
    template <>
    struct hash<Faye::Vertex>
    {
        size_t operator()(Faye::Vertex const &vertex) const
        {
            size_t seed = hash<glm::vec3>()(vertex.pos);
            seed ^= hash<glm::vec3>()(vertex.color) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<glm::vec3>()(vertex.normal) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<glm::vec2>()(vertex.uv) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<glm::vec4>()(vertex.tangent) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}