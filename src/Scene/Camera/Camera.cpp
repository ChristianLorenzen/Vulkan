#include "Camera.hpp"

#include <cassert>
#include <limits>

using namespace Faye;

namespace
{
  constexpr float kMaxPitch = 1.5f;

  glm::vec3 safeNormalize(const glm::vec3 &value, const glm::vec3 &fallback)
  {
    if (glm::dot(value, value) <= std::numeric_limits<float>::epsilon())
    {
      return fallback;
    }

    return glm::normalize(value);
  }

  void setViewBasis(
    glm::mat4 &viewMatrix,
    glm::mat4 &inverseViewMatrix,
    const glm::vec3 &position,
    const glm::vec3 &right,
    const glm::vec3 &up,
    const glm::vec3 &forward)
  {
    viewMatrix = glm::mat4{1.0f};
    viewMatrix[0][0] = right.x;
    viewMatrix[1][0] = right.y;
    viewMatrix[2][0] = right.z;
    viewMatrix[0][1] = up.x;
    viewMatrix[1][1] = up.y;
    viewMatrix[2][1] = up.z;
    viewMatrix[0][2] = forward.x;
    viewMatrix[1][2] = forward.y;
    viewMatrix[2][2] = forward.z;
    viewMatrix[3][0] = -glm::dot(right, position);
    viewMatrix[3][1] = -glm::dot(up, position);
    viewMatrix[3][2] = -glm::dot(forward, position);

    inverseViewMatrix = glm::mat4{1.0f};
    inverseViewMatrix[0][0] = right.x;
    inverseViewMatrix[0][1] = right.y;
    inverseViewMatrix[0][2] = right.z;
    inverseViewMatrix[1][0] = up.x;
    inverseViewMatrix[1][1] = up.y;
    inverseViewMatrix[1][2] = up.z;
    inverseViewMatrix[2][0] = forward.x;
    inverseViewMatrix[2][1] = forward.y;
    inverseViewMatrix[2][2] = forward.z;
    inverseViewMatrix[3][0] = position.x;
    inverseViewMatrix[3][1] = position.y;
    inverseViewMatrix[3][2] = position.z;
  }
}

void Camera::update()
{
  rotation.x = glm::clamp(pitch, -kMaxPitch, kMaxPitch);
  rotation.y = yaw;

  position +=
    rightFromRotation(rotation) * velocity.x +
    upFromRotation(rotation) * velocity.y +
    forwardFromRotation(rotation) * velocity.z;

    velocity = glm::vec3(0.f);
  setViewYXZ(position, rotation);
}

void Faye::Camera::setOrthographicProjection(float left, float right, float bottom, float top, float near, float far)
{
  assert(right != left && top != bottom && far != near);

    projectionMatrix = glm::mat4{1.0f};
    projectionMatrix[0][0] = 2.f / (right - left);
    projectionMatrix[1][1] = 2.f / (bottom - top);
    projectionMatrix[2][2] = 1.f / (far - near);
    projectionMatrix[3][0] = -(right + left) / (right - left);
    projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
    projectionMatrix[3][2] = -near / (far - near);
  inverseProjectionMatrix = glm::inverse(projectionMatrix);
}

void Faye::Camera::setPerspectiveProjection(float fov, float aspect, float near, float far)
{
  assert(glm::abs(aspect) > std::numeric_limits<float>::epsilon());
  assert(glm::abs(fov) > std::numeric_limits<float>::epsilon());
  assert(far > near);

    const float tanHalfFovy = tan(fov / 2.f);
    projectionMatrix = glm::mat4{0.0f};
    projectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
    projectionMatrix[1][1] = 1.f / (tanHalfFovy);
    projectionMatrix[2][2] = far / (far - near);
    projectionMatrix[2][3] = 1.f;
    projectionMatrix[3][2] = -(far * near) / (far - near);
  inverseProjectionMatrix = glm::inverse(projectionMatrix);
}

glm::vec3 Camera::rightFromRotation(const glm::vec3 &rotation)
{
  const float c3 = glm::cos(rotation.z);
  const float s3 = glm::sin(rotation.z);
  const float c2 = glm::cos(rotation.x);
  const float s2 = glm::sin(rotation.x);
  const float c1 = glm::cos(rotation.y);
  const float s1 = glm::sin(rotation.y);

  return glm::normalize(glm::vec3{
    c1 * c3 + s1 * s2 * s3,
    c2 * s3,
    c1 * s2 * s3 - c3 * s1,
  });
}

glm::vec3 Camera::upFromRotation(const glm::vec3 &rotation)
{
  const float c3 = glm::cos(rotation.z);
  const float s3 = glm::sin(rotation.z);
  const float c2 = glm::cos(rotation.x);
  const float s2 = glm::sin(rotation.x);
  const float c1 = glm::cos(rotation.y);
  const float s1 = glm::sin(rotation.y);

  return glm::normalize(glm::vec3{
    c3 * s1 * s2 - c1 * s3,
    c2 * c3,
    c1 * c3 * s2 + s1 * s3,
  });
}

glm::vec3 Camera::forwardFromRotation(const glm::vec3 &rotation)
{
  const float c2 = glm::cos(rotation.x);
  const float s2 = glm::sin(rotation.x);
  const float c1 = glm::cos(rotation.y);
  const float s1 = glm::sin(rotation.y);

  return glm::normalize(glm::vec3{
    c2 * s1,
    -s2,
    c1 * c2,
  });
}

void Camera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up)
{
  this->position = position;

  const glm::vec3 forward = safeNormalize(direction, getForwardDirection());
  glm::vec3 right = glm::cross(up, forward);
  if (glm::dot(right, right) <= std::numeric_limits<float>::epsilon())
  {
    const glm::vec3 fallbackUp = glm::abs(forward.y) > 0.999f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f};
    right = glm::cross(fallbackUp, forward);
  }

  right = glm::normalize(right);
  const glm::vec3 cameraUp = glm::normalize(glm::cross(forward, right));
  setViewBasis(viewMatrix, inverseViewMatrix, position, right, cameraUp, forward);
}
  
void Camera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
    setViewDirection(position, target - position, up);
  }
  
void Camera::setViewYXZ(glm::vec3 position, glm::vec3 rotation) {
  this->position = position;
  this->rotation = rotation;
  pitch = rotation.x;
  yaw = rotation.y;

  const glm::vec3 right = rightFromRotation(rotation);
  const glm::vec3 up = upFromRotation(rotation);
  const glm::vec3 forward = forwardFromRotation(rotation);
  setViewBasis(viewMatrix, inverseViewMatrix, position, right, up, forward);
}


glm::mat4 Camera::getRotationMatrix() const {
  const glm::vec3 right = rightFromRotation(rotation);
  const glm::vec3 up = upFromRotation(rotation);
  const glm::vec3 forward = forwardFromRotation(rotation);

  return glm::mat4{
    {right.x, right.y, right.z, 0.0f},
    {up.x, up.y, up.z, 0.0f},
    {forward.x, forward.y, forward.z, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f}};
}

glm::mat4 Camera::getViewMatrix() const
{
  return viewMatrix;
}

CameraRay Camera::rayFromNdc(const glm::vec2 &ndc) const
{
  const glm::vec4 nearClip{ndc.x, ndc.y, 0.0f, 1.0f};
  const glm::vec4 farClip{ndc.x, ndc.y, 1.0f, 1.0f};

  glm::vec4 nearView = inverseProjectionMatrix * nearClip;
  glm::vec4 farView = inverseProjectionMatrix * farClip;
  nearView /= nearView.w;
  farView /= farView.w;

  glm::vec4 nearWorld = inverseViewMatrix * nearView;
  glm::vec4 farWorld = inverseViewMatrix * farView;
  nearWorld /= nearWorld.w;
  farWorld /= farWorld.w;

  CameraRay ray{};
  ray.origin = position;
  ray.direction = glm::normalize(glm::vec3(farWorld - nearWorld));
  return ray;
}