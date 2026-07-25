#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>    //vec3, vec4, ivec4, mat4
#include <glm/common.hpp> //vec3, vec4, ivec4, mat4
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Faye
{

    struct CameraRay
    {
        glm::vec3 origin{};
        glm::vec3 direction{0.0f, 0.0f, 1.0f};
    };

    class Camera
    {
    public:
        Camera() : position{2.0f, 2.0f, 2.0f}, velocity{0.0f, 0.0f, 0.0f} {}
        ~Camera() {}

        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

        // Movable (but still not copyable): CameraComponent lives in a
        // sparse-set pool, whose swap-and-pop removal and vector growth
        // relocate components by move.
        Camera(Camera &&) = default;
        Camera &operator=(Camera &&) = default;

        glm::vec3 position;
        glm::vec3 velocity;

        float pitch{0.0f};
        float yaw{0.0f};

        glm::vec3 rotation;

        glm::mat4 getViewMatrix() const;
        glm::mat4 getRotationMatrix() const;

        void update();

        void setOrthographicProjection(float left, float right, float bottom, float top, float near, float far);
        void setPerspectiveProjection(float fov, float aspect, float near, float far);

        static glm::vec3 rightFromRotation(const glm::vec3 &rotation);
        static glm::vec3 upFromRotation(const glm::vec3 &rotation);
        static glm::vec3 forwardFromRotation(const glm::vec3 &rotation);

        void setViewDirection(glm::vec3 pos, glm::vec3 dir, glm::vec3 up = glm::vec3(0.f, 1.f, 0.f));
        void setViewTarget(glm::vec3 pos, glm::vec3 target, glm::vec3 up = glm::vec3(0.f, 1.f, 0.f));
        void setViewYXZ(glm::vec3 pos, glm::vec3 rotation);

        glm::vec3 getRightDirection() const { return glm::vec3(inverseViewMatrix[0]); }
        glm::vec3 getUpDirection() const { return glm::vec3(inverseViewMatrix[1]); }
        glm::vec3 getForwardDirection() const { return glm::vec3(inverseViewMatrix[2]); }

        CameraRay rayFromNdc(const glm::vec2 &ndc) const;

        const glm::mat4 &getProjection() const { return projectionMatrix; }
        const glm::mat4 &getView() const { return viewMatrix; }
        const glm::mat4 &getInverseProjection() const { return inverseProjectionMatrix; }
        const glm::mat4 &getInverseView() const { return inverseViewMatrix; }
        glm::mat4 getViewProjection() const { return projectionMatrix * viewMatrix; }
        void saveViewProjectionMatrix() { priorViewProjectionMatrix = projectionMatrix * viewMatrix; }
        glm::mat4 getPriorViewProjection() const { return priorViewProjectionMatrix; }

    private:
        glm::mat4 projectionMatrix{1.0f};
        glm::mat4 inverseProjectionMatrix{1.0f};
        glm::mat4 viewMatrix{1.f};
        glm::mat4 inverseViewMatrix{1.f};
        glm::mat4 priorViewProjectionMatrix{1.f};
    };
}