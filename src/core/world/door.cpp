#include "core/world/door.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "core/world/prototype_level.hpp"

namespace {
double radians(float angle) noexcept {
  return std::remainder(double(angle), 360.0) * std::numbers::pi / 180.0;
}
bool finite(WorldPosition p) noexcept {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool range(float v, float lo, float hi) noexcept {
  return std::isfinite(v) && v >= lo && v <= hi;
}
WorldPosition rotate(WorldPosition p, float angle) noexcept {
  const double c = std::cos(radians(angle)), s = std::sin(radians(angle));
  return {float(c * p.x + s * p.z), p.y, float(-s * p.x + c * p.z)};
}
using V = std::array<double, 3>;
V minus(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V cross(V a, V b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
double dot(V a, V b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
bool triangleOverlapsBox(const std::array<V, 3>& p, V h) {
  const std::array<V, 3> edges{minus(p[1], p[0]), minus(p[2], p[1]),
                               minus(p[0], p[2])};
  const std::array<V, 3> axes{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
  const auto separates = [&](V axis) {
    const double length = std::sqrt(dot(axis, axis));
    if (length < 1e-12) return false;
    const double r = h[0] * std::abs(axis[0]) + h[1] * std::abs(axis[1]) +
                     h[2] * std::abs(axis[2]);
    const std::array<double, 3> projection{dot(p[0], axis), dot(p[1], axis),
                                           dot(p[2], axis)};
    return *std::min_element(projection.begin(), projection.end()) >=
               r - 0.0001 * length ||
           *std::max_element(projection.begin(), projection.end()) <=
               -r + 0.0001 * length;
  };
  for (V axis : axes)
    if (separates(axis)) return false;
  if (separates(cross(edges[0], edges[1]))) return false;
  for (V edge : edges)
    for (V axis : axes)
      if (separates(cross(edge, axis))) return false;
  return true;
}
}  // namespace

float doorInitialAngle(const DoorDefinition& door) noexcept {
  return door.initially_open ? door.open_angle_degrees : 0.0F;
}

WorldPosition doorWorldPoint(const DoorDefinition& door, float angle,
                             WorldPosition local) noexcept {
  const auto p = rotate(
      local,
      float(std::remainder(double(door.closed_yaw_degrees), 360.0)) + angle);
  return {door.hinge_position.x + p.x, door.hinge_position.y + p.y,
          door.hinge_position.z + p.z};
}

WorldPosition doorLocalPoint(const DoorDefinition& door, float angle,
                             WorldPosition world) noexcept {
  return rotate(
      {world.x - door.hinge_position.x, world.y - door.hinge_position.y,
       world.z - door.hinge_position.z},
      -float(std::remainder(double(door.closed_yaw_degrees), 360.0)) - angle);
}

DoorLeafPose doorLeafPose(const DoorDefinition& door, float angle) noexcept {
  return {
      doorWorldPoint(door, angle, {door.width / 2, door.height / 2, 0}),
      {door.width / 2, door.height / 2, door.thickness / 2},
      float(std::remainder(double(door.closed_yaw_degrees), 360.0)) + angle};
}

std::array<WorldPosition, 8> doorCorners(const DoorDefinition& door,
                                         float angle) noexcept {
  std::array<WorldPosition, 8> corners{};
  for (std::size_t i = 0; i < corners.size(); ++i)
    corners[i] =
        doorWorldPoint(door, angle,
                       {i & 1 ? door.width : 0.0F, i & 2 ? door.height : 0.0F,
                        (i & 4 ? 0.5F : -0.5F) * door.thickness});
  return corners;
}

bool doorGeometryIsValid(const DoorDefinition& door) noexcept {
  if (!finite(door.hinge_position) || !std::isfinite(door.closed_yaw_degrees) ||
      !range(door.width, 0.4F, 2.5F) || !range(door.height, 1.0F, 3.5F) ||
      !range(door.thickness, 0.04F, 0.30F) ||
      !range(std::abs(door.open_angle_degrees), 15.0F, 170.0F) ||
      !range(door.speed_degrees_per_second, 15.0F, 180.0F))
    return false;
  for (float angle : {0.0F, door.open_angle_degrees})
    for (auto p : doorCorners(door, angle))
      if (!finite(p)) return false;
  return true;
}

std::string doorFieldError(const DoorDefinition& door) {
  if (!levelEntryIdIsValid(door.id))
    return "id must match [a-z][a-z0-9-]{0,63}";
  if (!finite(door.hinge_position)) return "hinge_position must be finite";
  if (!std::isfinite(door.closed_yaw_degrees))
    return "closed_yaw_degrees must be finite";
  if (!range(door.width, 0.4F, 2.5F))
    return "width must be between 0.4 and 2.5 metres";
  if (!range(door.height, 1.0F, 3.5F))
    return "height must be between 1 and 3.5 metres";
  if (!range(door.thickness, 0.04F, 0.30F))
    return "thickness must be between 0.04 and 0.30 metres";
  if (!range(std::abs(door.open_angle_degrees), 15, 170))
    return "open_angle_degrees magnitude must be between 15 and 170";
  if (!range(door.speed_degrees_per_second, 15, 180))
    return "speed_degrees_per_second must be between 15 and 180";
  if (door.lock_side != DoorLockSide::None &&
      door.lock_side != DoorLockSide::PositiveZ &&
      door.lock_side != DoorLockSide::NegativeZ)
    return "lock_side is unsupported";
  if (!doorGeometryIsValid(door))
    return "derived door geometry must remain finite";
  return {};
}

bool doorPointInside(const DoorDefinition& door, float angle,
                     WorldPosition point) noexcept {
  const auto p = doorLocalPoint(door, angle, point);
  return p.x >= 0 && p.x <= door.width && p.y >= 0 && p.y <= door.height &&
         std::abs(p.z) <= door.thickness / 2;
}

std::optional<float> doorRayDistance(const DoorDefinition& door, float angle,
                                     WorldPosition origin,
                                     WorldPosition direction) noexcept {
  if (!doorGeometryIsValid(door) || !std::isfinite(angle) || !finite(origin) ||
      !finite(direction) || doorPointInside(door, angle, origin))
    return std::nullopt;
  const double length =
      std::hypot(double(direction.x), double(direction.y), double(direction.z));
  if (!(length > 0)) return std::nullopt;
  const auto o = doorLocalPoint(door, angle, origin);
  const auto d = rotate(
      direction,
      -float(std::remainder(double(door.closed_yaw_degrees), 360.0)) - angle);
  const std::array<double, 3> pos{o.x - door.width / 2, o.y - door.height / 2,
                                  o.z};
  const std::array<double, 3> dir{d.x / length, d.y / length, d.z / length};
  const std::array<double, 3> half{door.width / 2, door.height / 2,
                                   door.thickness / 2};
  double near = 0, far = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < 3; ++i) {
    if (std::abs(dir[i]) < 1e-12) {
      if (std::abs(pos[i]) > half[i]) return std::nullopt;
      continue;
    }
    double a = (-half[i] - pos[i]) / dir[i], b = (half[i] - pos[i]) / dir[i];
    if (a > b) std::swap(a, b);
    near = std::max(near, a);
    far = std::min(far, b);
    if (near > far) return std::nullopt;
  }
  if (!std::isfinite(near) || near > std::numeric_limits<float>::max())
    return std::nullopt;
  return float(near);
}

bool yawedBoxesOverlap(const DoorLeafPose& a, const DoorLeafPose& b,
                       float tolerance) noexcept {
  if (!finite(a.center) || !finite(b.center)) return false;
  if (std::abs(double(a.center.y) - b.center.y) >=
      double(a.half_extent.y) + b.half_extent.y - tolerance)
    return false;
  const double ac = std::cos(radians(a.yaw_degrees)),
               as = std::sin(radians(a.yaw_degrees));
  const double bc = std::cos(radians(b.yaw_degrees)),
               bs = std::sin(radians(b.yaw_degrees));
  const std::array<std::array<double, 2>, 4> axes{
      {{ac, -as}, {as, ac}, {bc, -bs}, {bs, bc}}};
  for (const auto& axis : axes) {
    const double distance =
        std::abs((double(a.center.x) - b.center.x) * axis[0] +
                 (double(a.center.z) - b.center.z) * axis[1]);
    const double ar = a.half_extent.x * std::abs(ac * axis[0] - as * axis[1]) +
                      a.half_extent.z * std::abs(as * axis[0] + ac * axis[1]);
    const double br = b.half_extent.x * std::abs(bc * axis[0] - bs * axis[1]) +
                      b.half_extent.z * std::abs(bs * axis[0] + bc * axis[1]);
    if (distance >= ar + br - tolerance) return false;
  }
  return true;
}

bool doorOverlapsTerrain(const DoorDefinition& door, float angle,
                         const PrototypeTerrain& terrain) {
  if (!doorGeometryIsValid(door)) return false;
  const auto corners = doorCorners(door, angle);
  for (auto p : corners)
    if (prototypeTerrainContains(terrain, p.x, p.z) &&
        p.y < prototypeTerrainHeightAt(terrain, p.x, p.z) - 0.0001F)
      return true;
  const auto local = [&](WorldPosition p) -> V {
    const auto q = doorLocalPoint(door, angle, p);
    return {double(q.x) - door.width / 2, double(q.y) - door.height / 2, q.z};
  };
  float xmin = corners[0].x, xmax = xmin, zmin = corners[0].z, zmax = zmin;
  for (auto p : corners) {
    xmin = std::min(xmin, p.x);
    xmax = std::max(xmax, p.x);
    zmin = std::min(zmin, p.z);
    zmax = std::max(zmax, p.z);
  }
  for (std::size_t z = 0; z < prototype_terrain_cell_count; ++z)
    for (std::size_t x = 0; x < prototype_terrain_cell_count; ++x) {
      const auto p00 = prototypeTerrainSamplePosition(terrain, x, z);
      const auto p11 = prototypeTerrainSamplePosition(terrain, x + 1, z + 1);
      if (p11.x < xmin || p00.x > xmax || p11.z < zmin || p00.z > zmax)
        continue;
      const auto p01 = prototypeTerrainSamplePosition(terrain, x, z + 1);
      const auto p10 = prototypeTerrainSamplePosition(terrain, x + 1, z);
      const V h{door.width / 2, door.height / 2, door.thickness / 2};
      if (triangleOverlapsBox({local(p00), local(p01), local(p11)}, h) ||
          triangleOverlapsBox({local(p00), local(p11), local(p10)}, h))
        return true;
    }
  return false;
}

std::array<OpaqueBoxFrame, 6> doorPresentationBoxes(
    const DoorDefinition& door, float angle, bool locked,
    float handle_depression, float knock_pulse, int feedback_side) noexcept {
  const auto box = [&](WorldPosition local, WorldExtent extent,
                       WorldColor color) {
    const auto p = doorWorldPoint(door, angle, local);
    return OpaqueBoxFrame{{p.x, p.y, p.z},
                          {extent.x, extent.y, extent.z},
                          doorLeafPose(door, angle).yaw_degrees,
                          color,
                          2};
  };
  const float h = std::min(1.0F, door.height * 0.55F), x = door.width - 0.12F;
  const float press = std::clamp(handle_depression, 0.0F, 1.0F);
  const float pulse = std::clamp(knock_pulse, 0.0F, 1.0F);
  const float bolt_side =
      door.lock_side == DoorLockSide::NegativeZ ? -1.0F : 1.0F;
  const WorldColor handle = press > 0 ? WorldColor{235, 110, 70, 255}
                                      : WorldColor{190, 190, 180, 255};
  const auto plate = [&](int side) {
    const bool active = feedback_side == side && pulse > 0;
    return box(
        {door.width * 0.5F, h + 0.18F, side * (door.thickness / 2 + 0.003F)},
        {0.09F, 0.07F, 0.003F},
        active ? WorldColor{230, 220, 100, 255} : WorldColor{85, 65, 40, 255});
  };
  return {
      box({door.width / 2, door.height / 2, 0},
          {door.width / 2, door.height / 2, door.thickness / 2},
          {135, 103, 67, 255}),
      box({x, h - 0.04F * press, door.thickness / 2 + 0.02F},
          {0.07F, 0.025F, 0.02F}, handle),
      box({x, h - 0.04F * press, -door.thickness / 2 - 0.02F},
          {0.07F, 0.025F, 0.02F}, handle),
      box({x + (locked ? 0.045F : -0.025F), h - 0.13F,
           bolt_side * (door.thickness / 2 + 0.012F)},
          {0.045F, 0.02F, 0.012F},
          door.lock_side == DoorLockSide::None ? WorldColor{135, 103, 67, 255}
          : locked ? WorldColor{190, 85, 60, 255}
                   : WorldColor{160, 180, 150, 255}),
      plate(1),
      plate(-1)};
}
