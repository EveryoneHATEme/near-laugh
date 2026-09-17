#ifndef CORE_ROTATION_HPP
#define CORE_ROTATION_HPP

#include <array>
#include <cmath>
#include <numbers>

// Unit quaternion storage is XYZW, with identity {0,0,0,1}.
[[nodiscard]] inline std::array<float, 4> yawQuaternion(
    float degrees) noexcept {
  const double half =
      std::remainder(double(degrees), 360.) * std::numbers::pi / 360.;
  return {0, float(std::sin(half)), 0, float(std::cos(half))};
}

[[nodiscard]] inline bool quaternionIsValid(
    const std::array<float, 4>& q) noexcept {
  double length_squared = 0;
  for (float value : q) {
    if (!std::isfinite(value)) return false;
    length_squared += double(value) * value;
  }
  return std::abs(length_squared - 1.) <= .0001;
}

// Callers validate first. Divide by the squared length to preserve a pure
// rotation within the accepted floating-point unit-length tolerance.
[[nodiscard]] inline std::array<float, 3> rotateVector(
    const std::array<float, 4>& q, const std::array<float, 3>& v) noexcept {
  const double x = q[0], y = q[1], z = q[2], w = q[3];
  const double scale = 2. / (x * x + y * y + z * z + w * w);
  const double tx = y * v[2] - z * v[1];
  const double ty = z * v[0] - x * v[2];
  const double tz = x * v[1] - y * v[0];
  return {float(v[0] + scale * (w * tx + y * tz - z * ty)),
          float(v[1] + scale * (w * ty + z * tx - x * tz)),
          float(v[2] + scale * (w * tz + x * ty - y * tx))};
}

#endif
