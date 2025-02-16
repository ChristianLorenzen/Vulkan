#pragma once

#include <glm/glm.hpp> //vec3, vec4, ivec4, mat4
#include <glm/common.hpp> //vec3, vec4, ivec4, mat4
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Faye {

    class Camera {
    public:
        Camera() : position { 2.0f, 2.0f, 2.0f }, velocity { 0.0f, 0.0f, 0.0f } {}
        ~Camera() {}

        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

        glm::vec3 position;
        glm::vec3 velocity;

        float pitch { 0.0f };
        float yaw { 0.0f };

        glm::mat4 getViewMatrix();
        glm::mat4 getRotationMatrix();

        void update();
    };
}