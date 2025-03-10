
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "GameObject.hpp"

// namespace Faye {
//
// class GravityPhysicsSystem {
//     public:
//      GravityPhysicsSystem(float strength) : strengthGravity{strength} {}
    
//      const float strengthGravity;
    
//      // dt stands for delta time, and specifies the amount of time to advance the simulation
//      // substeps is how many intervals to divide the forward time step in. More substeps result in a
//      // more stable simulation, but takes longer to compute
//      void update(std::vector<GameObject>& objs, float dt, unsigned int substeps = 1) {
//        const float stepDelta = dt / substeps;
//        for (int i = 0; i < substeps; i++) {
//          stepSimulation(objs, stepDelta);
//        }
//      }
    
//      glm::vec2 computeForce(GameObject& fromObj, GameObject& toObj) const {
//        auto offset = fromObj.transform.translation - toObj.transform.translation;
//        float distanceSquared = glm::dot(offset, offset);
    
//        // clown town - just going to return 0 if objects are too close together...
//        if (glm::abs(distanceSquared) < 1e-10f) {
//          return {.0f, .0f};
//        }
    
//        float force =
//            strengthGravity * toObj.rigidBody2d.mass * fromObj.rigidBody2d.mass / distanceSquared;
//        return force * offset / glm::sqrt(distanceSquared);
//      }
    
//     private:
//      void stepSimulation(std::vector<GameObject>& physicsObjs, float dt) {
//        // Loops through all pairs of objects and applies attractive force between them
//        for (auto iterA = physicsObjs.begin(); iterA != physicsObjs.end(); ++iterA) {
//          auto& objA = *iterA;
//          for (auto iterB = iterA; iterB != physicsObjs.end(); ++iterB) {
//            if (iterA == iterB) continue;
//            auto& objB = *iterB;
    
//            auto force = computeForce(objA, objB);
//            objA.rigidBody2d.velocity += dt * -force / objA.rigidBody2d.mass;
//            objB.rigidBody2d.velocity += dt * force / objB.rigidBody2d.mass;
//          }
//        }
    
//        // update each objects position based on its final velocity
//        for (auto& obj : physicsObjs) {
//          obj.transform.translation += dt * obj.rigidBody2d.velocity;
//        }
//      }
//    };


// class Vec2FieldSystem {
// public:
//     void update(
//         const GravityPhysicsSystem& physicsSystem,
//         std::vector<GameObject>& physicsObjs,
//         std::vector<GameObject>& vectorField) {
//     // For each field line we caluclate the net graviation force for that point in space
//     for (auto& vf : vectorField) {
//         glm::vec2 direction{};
//         for (auto& obj : physicsObjs) {
//         direction += physicsSystem.computeForce(obj, vf);
//         }

//         // This scales the length of the field line based on the log of the length
//         // values were chosen just through trial and error based on what i liked the look
//         // of and then the field line is rotated to point in the direction of the field
//         vf.transform.scale.x =
//             0.005f + 0.045f * glm::clamp(glm::log(glm::length(direction) + 1) / 3.f, 0.f, 1.f);
//         vf.transform.rotation = atan2(direction.y, direction.x);
//     }
//     }
// };

// std::unique_ptr<Model> createSquareModel(VulkanDevice& device, glm::vec2 offset) {
//     std::vector<Vertex> vertices = {
//         {{-0.5f, -0.5f}},
//         {{0.5f, 0.5f}},
//         {{-0.5f, 0.5f}},
//         {{-0.5f, -0.5f}},
//         {{0.5f, -0.5f}},
//         {{0.5f, 0.5f}},  //
//     };
//     for (auto& v : vertices) {
//       v.pos += offset;
//     }
//     return std::make_unique<Model>(device, vertices);
//   }
   
//   std::unique_ptr<Model> createCircleModel(VulkanDevice& device, unsigned int numSides) {
//     std::vector<Vertex> uniqueVertices{};
//     for (int i = 0; i < numSides; i++) {
//       float angle = i * glm::two_pi<float>() / numSides;
//       uniqueVertices.push_back({{glm::cos(angle), glm::sin(angle)}});
//     }
//     uniqueVertices.push_back({});  // adds center vertex at 0, 0
   
//     std::vector<Vertex> vertices{};
//     for (int i = 0; i < numSides; i++) {
//       vertices.push_back(uniqueVertices[i]);
//       vertices.push_back(uniqueVertices[(i + 1) % numSides]);
//       vertices.push_back(uniqueVertices[numSides]);
//     }
//     return std::make_unique<Model>(device, vertices);
//   }

// }