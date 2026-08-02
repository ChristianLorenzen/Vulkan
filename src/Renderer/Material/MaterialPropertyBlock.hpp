#pragma once

#include <string>
#include <unordered_map>

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
        void setFloat(const std::string& name, float v)       { floats[name] = v; }
        void setInt  (const std::string& name, int v)         { ints[name]   = v; }
        void setVec2 (const std::string& name, glm::vec2 v)   { vec2s[name]  = v; }
        void setVec3 (const std::string& name, glm::vec3 v)   { vec3s[name]  = v; }
        void setVec4 (const std::string& name, glm::vec4 v)   { vec4s[name]  = v; }

        bool hasFloat(const std::string& name) const { return floats.count(name) > 0; }
        bool hasInt  (const std::string& name) const { return ints.count(name)   > 0; }
        bool hasVec2 (const std::string& name) const { return vec2s.count(name)  > 0; }
        bool hasVec3 (const std::string& name) const { return vec3s.count(name)  > 0; }
        bool hasVec4 (const std::string& name) const { return vec4s.count(name)  > 0; }

        float getFloat(const std::string& name) const
        {
            const auto it = floats.find(name);
            return it != floats.end() ? it->second : 0.0f;
        }

        int getInt(const std::string& name) const
        {
            const auto it = ints.find(name);
            return it != ints.end() ? it->second : 0;
        }

        glm::vec2 getVec2(const std::string& name) const
        {
            const auto it = vec2s.find(name);
            return it != vec2s.end() ? it->second : glm::vec2{0.0f};
        }

        glm::vec3 getVec3(const std::string& name) const
        {
            const auto it = vec3s.find(name);
            return it != vec3s.end() ? it->second : glm::vec3{0.0f};
        }

        glm::vec4 getVec4(const std::string& name) const
        {
            const auto it = vec4s.find(name);
            return it != vec4s.end() ? it->second : glm::vec4{0.0f};
        }

        bool empty() const
        {
            return floats.empty() && ints.empty() && vec2s.empty() && vec3s.empty() && vec4s.empty();
        }

    private:
        std::unordered_map<std::string, float>     floats;
        std::unordered_map<std::string, int>       ints;
        std::unordered_map<std::string, glm::vec2> vec2s;
        std::unordered_map<std::string, glm::vec3> vec3s;
        std::unordered_map<std::string, glm::vec4> vec4s;
    };

} // namespace Faye
