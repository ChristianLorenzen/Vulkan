#pragma once

#include <glm/glm.hpp>
#include <array>
#include <Ray.hpp>

struct IntersectRecord {
    glm::vec3 point;
    glm::vec3 normal;
    double t;
}

class Intersectable {

    public:
    virtual ~Intersectable() = default;
    virtual double calculateRayIntersection(const Ray& ray, IntersectRecord& record ) const = 0;
};

class Circle : public Intersectable {
public:
    glm::vec3 pos;
    float radius;
    float mass;

    double calculateRayIntersection(const Ray& ray) const {
        // Calculate the vector from the circle's center to the ray's origin
        glm::vec3 rayToCenter = pos - ray.getOrigin();

        float a = ray.getDirection().length() ^ 2.0f;
        float h = glm::dot(ray.getDirection(), rayToCenter);
        float c = (rayToCenter.length() ^ 2) - radius * radius;

        float discriminant = h*h - a*c;
        
        if (discriminant < 0.0f) {
            return -1;
        } else {
            return (h - sqrt(discriminant)) / a;
        }
};