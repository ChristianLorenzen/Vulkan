#pragma once

#include <string>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Faye
{
    // Stores per-draw overrides for named material properties, analogous to
    // Unity's MaterialPropertyBlock.  Values set here can be consumed by a
    // render system to patch push-constants or a per-draw UBO before each
    // draw call without modifying the shared Material asset.
    class MaterialPropertyBlock
    {
    public:
        void setFloat(const std::string &name, float v)
        {
            floats[name] = v;
        }

        void setVec4(const std::string &name, glm::vec4 v)
        {
            vec4s[name] = v;
        }

        bool hasFloat(const std::string &name) const
        {
            return floats.count(name) > 0;
        }

        float getFloat(const std::string &name) const
        {
            const auto it = floats.find(name);
            return it != floats.end() ? it->second : 0.0f;
        }

        glm::vec4 getVec4(const std::string &name) const
        {
            const auto it = vec4s.find(name);
            return it != vec4s.end() ? it->second : glm::vec4{0.0f};
        }

        bool empty() const
        {
            return floats.empty() && vec4s.empty();
        }

    private:
        std::unordered_map<std::string, float>     floats;
        std::unordered_map<std::string, glm::vec4> vec4s;
    };
} // namespace Faye
