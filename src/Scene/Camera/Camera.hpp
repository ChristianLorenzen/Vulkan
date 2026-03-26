#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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

        glm::vec3 rotation;

        glm::mat4 getViewMatrix();
        glm::mat4 getRotationMatrix();

        void update();

        void setOrthographicProjection(float left, float right, float bottom, float top, float near, float far);
        void setPerspectiveProjection(float fov, float aspect, float near, float far);


        void setViewDirection(glm::vec3 pos, glm::vec3 dir, glm::vec3 up = glm::vec3(0.f, 1.f, 0.f)); 
        void setViewTarget(glm::vec3 pos, glm::vec3 target, glm::vec3 up = glm::vec3(0.f, 1.f, 0.f)); 
        void setViewYXZ(glm::vec3 pos, glm::vec3 rotation);

        const glm::mat4 &getProjection() const { return projectionMatrix; }
        const glm::mat4 &getView() const { return viewMatrix; }

        private:
        glm::mat4 projectionMatrix{1.0f};
        glm::mat4 viewMatrix{1.f};
    };
}