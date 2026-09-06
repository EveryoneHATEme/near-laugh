#include "core/world/light_switch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {
bool finite(WorldPosition point) noexcept {
  return std::isfinite(point.x) && std::isfinite(point.y) &&
         std::isfinite(point.z);
}

float yawRadians(float degrees) noexcept {
  return std::remainder(degrees, 360.0F) * (std::numbers::pi_v<float> / 180.0F);
}
}  // namespace

WorldPosition lightSwitchWorldPoint(const PrototypeLightSwitch& light_switch,
                                    WorldPosition local) noexcept {
  const float yaw = yawRadians(light_switch.yaw_degrees);
  const float c = std::cos(yaw);
  const float s = std::sin(yaw);
  return {light_switch.position.x + c * local.x + s * local.z,
          light_switch.position.y + local.y,
          light_switch.position.z - s * local.x + c * local.z};
}

std::array<WorldPosition, 8> lightSwitchCorners(
    const PrototypeLightSwitch& light_switch) noexcept {
  std::array<WorldPosition, 8> result{};
  for (std::size_t i = 0; i < result.size(); ++i) {
    result[i] = lightSwitchWorldPoint(
        light_switch, {(i & 1 ? 1.0F : -1.0F) * light_switch_half_extent.x,
                       (i & 2 ? 1.0F : -1.0F) * light_switch_half_extent.y,
                       (i & 4 ? 1.0F : -1.0F) * light_switch_half_extent.z});
  }
  return result;
}

bool lightSwitchIsValid(const PrototypeLightSwitch& light_switch) noexcept {
  if (!finite(light_switch.position) ||
      !std::isfinite(light_switch.yaw_degrees) ||
      light_switch.point_light_index >= prototype_point_light_count) {
    return false;
  }
  const auto corners = lightSwitchCorners(light_switch);
  return std::all_of(corners.begin(), corners.end(), finite);
}

std::optional<float> lightSwitchRayDistance(
    const PrototypeLightSwitch& light_switch, WorldPosition origin,
    WorldPosition direction) noexcept {
  if (!lightSwitchIsValid(light_switch) || !finite(origin) ||
      !finite(direction))
    return std::nullopt;
  const double length =
      std::hypot(double(direction.x), double(direction.y), double(direction.z));
  if (!(length > 0.0)) return std::nullopt;
  const double yaw = yawRadians(light_switch.yaw_degrees);
  const double c = std::cos(yaw);
  const double s = std::sin(yaw);
  const double dx = double(origin.x) - light_switch.position.x;
  const double dz = double(origin.z) - light_switch.position.z;
  const std::array<double, 3> o{c * dx - s * dz,
                                double(origin.y) - light_switch.position.y,
                                s * dx + c * dz};
  const std::array<double, 3> d{(c * direction.x - s * direction.z) / length,
                                direction.y / length,
                                (s * direction.x + c * direction.z) / length};
  const std::array<double, 3> h{light_switch_half_extent.x,
                                light_switch_half_extent.y,
                                light_switch_half_extent.z};
  if (std::abs(o[0]) <= h[0] && std::abs(o[1]) <= h[1] &&
      std::abs(o[2]) <= h[2])
    return std::nullopt;
  double near = 0.0;
  double far = std::numeric_limits<double>::infinity();
  for (std::size_t axis = 0; axis < 3; ++axis) {
    if (std::abs(d[axis]) < 1.0e-12) {
      if (std::abs(o[axis]) > h[axis]) return std::nullopt;
      continue;
    }
    double first = (-h[axis] - o[axis]) / d[axis];
    double last = (h[axis] - o[axis]) / d[axis];
    if (first > last) std::swap(first, last);
    near = std::max(near, first);
    far = std::min(far, last);
    if (near > far) return std::nullopt;
  }
  if (!std::isfinite(near) || near > std::numeric_limits<float>::max())
    return std::nullopt;
  return static_cast<float>(near);
}

std::array<bool, prototype_point_light_count> initialPointLightEnabled(
    const std::optional<PrototypeLightSwitch>& light_switch) noexcept {
  std::array<bool, prototype_point_light_count> enabled{true, true};
  if (light_switch && lightSwitchIsValid(*light_switch))
    enabled[light_switch->point_light_index] = light_switch->initially_on;
  return enabled;
}
