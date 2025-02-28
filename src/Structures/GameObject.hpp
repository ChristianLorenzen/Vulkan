#pragma once

#include <glm/gtc/matrix_transform.hpp>

#include "Model.hpp"
#include <memory>

namespace Faye {

    struct RigidBody2dComponent {
      glm::vec2 velocity;
      float mass{1.0f};
    };

    struct TransformComponent {
        glm::vec3 translation{};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 rotation{};

        glm::mat4 mat4();
        glm::mat3 normalMatrix();
    };

    class GameObject {
        public:
            using id_t = unsigned int;

            static GameObject createGameObject() {
                static id_t currentId = 0;
                return GameObject{currentId++};
            }

            GameObject(const GameObject&) = delete;
            GameObject& operator=(const GameObject&) = delete;
            GameObject(GameObject&&) = default;
            GameObject& operator=(GameObject&&) = default;

            id_t getId() const { return id; }

            std::shared_ptr<Model> model{};
            glm::vec3 color{};
            TransformComponent transform{};
            RigidBody2dComponent rigidBody2d{};
        private:
            GameObject(id_t id) : id{id} {}

            id_t id;
    };
}