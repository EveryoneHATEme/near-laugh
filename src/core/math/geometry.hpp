#ifndef CORE_MATH_GEOMETRY_HPP
#define CORE_MATH_GEOMETRY_HPP

#include <algorithm>
#include <cmath>
#include <limits>

#include "math.hpp"

namespace math {

struct Ray {
  Vec3 origin{};
  Vec3 direction{0.0f, 0.0f, -1.0f};
};

struct RayHit {
  bool hit{false};
  float distance{0.0f};
};

struct AABB {
  Vec3 min{};
  Vec3 max{};

  Vec3 center() const { return (min + max) * 0.5f; }
};

inline bool intersects(const AABB& lhs, const AABB& rhs) {
  return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x &&
         lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y &&
         lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

inline float axisValue(const Vec3& value, int axis) {
  if (axis == 0) {
    return value.x;
  }
  if (axis == 1) {
    return value.y;
  }
  return value.z;
}

inline RayHit intersectRay(const Ray& ray, const AABB& box,
                           float max_distance =
                               std::numeric_limits<float>::infinity()) {
  float t_min = 0.0f;
  float t_max = max_distance;

  for (int axis = 0; axis < 3; ++axis) {
    const float origin = axisValue(ray.origin, axis);
    const float direction = axisValue(ray.direction, axis);
    const float box_min = axisValue(box.min, axis);
    const float box_max = axisValue(box.max, axis);

    if (std::fabs(direction) < 0.00001f) {
      if (origin < box_min || origin > box_max) {
        return {};
      }
      continue;
    }

    float t1 = (box_min - origin) / direction;
    float t2 = (box_max - origin) / direction;
    if (t1 > t2) {
      std::swap(t1, t2);
    }

    t_min = std::max(t_min, t1);
    t_max = std::min(t_max, t2);
    if (t_min > t_max) {
      return {};
    }
  }

  return {true, t_min};
}

}  // namespace math

#endif
