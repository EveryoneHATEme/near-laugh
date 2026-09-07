#include "core/render/point_shadow.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
float dot(WorldPosition a, WorldPosition b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
void checkRadius(float radius) {
  if (!std::isfinite(radius) || radius < .25F || radius > 20)
    throw std::invalid_argument(
        "Point shadows require a radius from 0.25 to 20 metres");
}
}  // namespace
CameraFrame pointShadowCamera(WorldPosition source, float radius,
                              std::size_t face) {
  checkRadius(radius);
  const auto& b = point_shadow_bases.at(face);
  const float a = radius / (radius - point_shadow_near);
  const float z = -point_shadow_near * a;
  CameraFrame camera{{b.right.x, b.down.x, a * b.forward.x, b.forward.x,
                      b.right.y, b.down.y, a * b.forward.y, b.forward.y,
                      b.right.z, b.down.z, a * b.forward.z, b.forward.z,
                      -dot(b.right, source), -dot(b.down, source),
                      z - a * dot(b.forward, source), -dot(b.forward, source)}};
  for (float value : camera.view_projection)
    if (!std::isfinite(value))
      throw std::invalid_argument(
          "Point-shadow projection exceeds finite coordinates");
  return camera;
}
PointShadowProjection projectPointShadow(WorldPosition direction,
                                         float radius) {
  checkRadius(radius);
  const float major = std::max(
      {std::abs(direction.x), std::abs(direction.y), std::abs(direction.z)});
  if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
      !std::isfinite(direction.z) || major <= 0)
    throw std::invalid_argument(
        "Point-shadow direction must be finite and nonzero");
  const std::size_t face =
      major == std::abs(direction.x)   ? (direction.x >= 0 ? 0 : 1)
      : major == std::abs(direction.y) ? (direction.y >= 0 ? 2 : 3)
                                       : (direction.z >= 0 ? 4 : 5);
  const auto& b = point_shadow_bases[face];
  const float a = radius / (radius - point_shadow_near);
  return {face, .5F + .5F * dot(b.right, direction) / major,
          .5F + .5F * dot(b.down, direction) / major,
          a - a * point_shadow_near / major};
}
WorldPosition pointShadowDirection(std::size_t face, float u, float v) {
  const auto& b = point_shadow_bases.at(face);
  const float x = 2 * u - 1, y = 2 * v - 1;
  return {b.forward.x + x * b.right.x + y * b.down.x,
          b.forward.y + x * b.right.y + y * b.down.y,
          b.forward.z + x * b.right.z + y * b.down.z};
}
