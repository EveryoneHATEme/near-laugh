#ifndef CORE_MATH_MATH_HPP
#define CORE_MATH_MATH_HPP

#include <algorithm>
#include <cmath>

namespace math {

constexpr float kPi = 3.14159265358979323846f;

inline float radians(float degrees) { return degrees * kPi / 180.0f; }

struct Vec2 {
  float x{};
  float y{};
};

struct Vec3 {
  float x{};
  float y{};
  float z{};

  Vec3() = default;
  Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

  Vec3& operator+=(const Vec3& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }

  Vec3& operator-=(const Vec3& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }

  Vec3& operator*=(float scale) {
    x *= scale;
    y *= scale;
    z *= scale;
    return *this;
  }
};

inline Vec3 operator+(Vec3 lhs, const Vec3& rhs) {
  lhs += rhs;
  return lhs;
}

inline Vec3 operator-(Vec3 lhs, const Vec3& rhs) {
  lhs -= rhs;
  return lhs;
}

inline Vec3 operator-(const Vec3& value) {
  return {-value.x, -value.y, -value.z};
}

inline Vec3 operator*(Vec3 value, float scale) {
  value *= scale;
  return value;
}

inline Vec3 operator*(float scale, Vec3 value) {
  value *= scale;
  return value;
}

inline Vec3 operator/(Vec3 value, float scale) {
  return {value.x / scale, value.y / scale, value.z / scale};
}

inline float dot(const Vec3& lhs, const Vec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.y * rhs.z - lhs.z * rhs.y,
          lhs.z * rhs.x - lhs.x * rhs.z,
          lhs.x * rhs.y - lhs.y * rhs.x};
}

inline float lengthSquared(const Vec3& value) { return dot(value, value); }

inline float length(const Vec3& value) {
  return std::sqrt(lengthSquared(value));
}

inline Vec3 normalize(const Vec3& value) {
  const float len = length(value);
  if (len <= 0.00001f) {
    return {};
  }
  return value / len;
}

inline float clamp(float value, float min_value, float max_value) {
  return std::max(min_value, std::min(value, max_value));
}

struct Mat4 {
  float m[16]{};

  static Mat4 identity() {
    Mat4 result{};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
  }

  static Mat4 perspective(float fov_y_radians, float aspect_ratio,
                          float near_plane, float far_plane) {
    Mat4 result{};
    const float f = 1.0f / std::tan(fov_y_radians * 0.5f);
    result.m[0] = f / aspect_ratio;
    result.m[5] = f;
    result.m[10] = far_plane / (far_plane - near_plane);
    result.m[11] = 1.0f;
    result.m[14] = -(near_plane * far_plane) / (far_plane - near_plane);
    return result;
  }

  static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 forward = normalize(target - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 camera_up = cross(right, forward);

    Mat4 result = identity();
    result.m[0] = right.x;
    result.m[1] = camera_up.x;
    result.m[2] = forward.x;
    result.m[4] = right.y;
    result.m[5] = camera_up.y;
    result.m[6] = forward.y;
    result.m[8] = right.z;
    result.m[9] = camera_up.z;
    result.m[10] = forward.z;
    result.m[12] = -dot(right, eye);
    result.m[13] = -dot(camera_up, eye);
    result.m[14] = -dot(forward, eye);
    return result;
  }
};

inline Mat4 operator*(const Mat4& lhs, const Mat4& rhs) {
  Mat4 result{};
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      for (int k = 0; k < 4; ++k) {
        result.m[row * 4 + col] += lhs.m[row * 4 + k] * rhs.m[k * 4 + col];
      }
    }
  }
  return result;
}

}  // namespace math

#endif
