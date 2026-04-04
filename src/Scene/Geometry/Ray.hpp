#pragma once

#include <glm/glm.hpp>

/*
 Basic Ray class P(t) = A + tB.
 A is the Ray origin, B is the Ray direction.
*/
class Ray {
    public:
        Ray() {}
        Ray(const glm::vec3& origin, const glm::vec3& direction) : origin(origin), direction(direction) {}
        
        glm::vec3 getOrigin() const { return origin; }
        glm::vec3 getDirection() const { return direction; }

        glm::vec3 getPoint(float t) const { return origin + t * direction; }


    private:
        glm::vec3 origin;
        glm::vec3 direction;
}