#include "camera.hpp"

#include <cmath>

math::Vec3 Camera::forward() const {
  return math::normalize({std::cos(pitch) * std::cos(yaw), std::sin(pitch),
                          std::cos(pitch) * std::sin(yaw)});
}

math::Vec3 Camera::right() const {
  return math::normalize(math::cross(forward(), {0.0f, 1.0f, 0.0f}));
}

math::Vec3 Camera::up() const {
  return math::normalize(math::cross(right(), forward()));
}

math::Mat4 Camera::viewProjection(float aspect_ratio) const {
  return math::Mat4::perspective(fov_y, aspect_ratio, near_plane, far_plane) *
         math::Mat4::lookAt(position, position + forward(), {0.0f, 1.0f, 0.0f});
}
