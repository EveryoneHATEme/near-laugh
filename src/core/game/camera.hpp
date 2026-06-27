#ifndef CORE_GAME_CAMERA_HPP
#define CORE_GAME_CAMERA_HPP

#include "../math/math.hpp"

class Camera {
 private:
  math::Vec3 position{0.0f, 1.35f, 4.0f};
  float yaw{-math::kPi * 0.5f};
  float pitch{0.0f};
  float fov_y{math::radians(75.0f)};
  float near_plane{0.08f};
  float far_plane{40.0f};

 public:
  const math::Vec3& getPosition() const { return position; }
  void setPosition(const math::Vec3& value) { position = value; }

  float getYaw() const { return yaw; }
  void setYaw(float value) { yaw = value; }

  float getPitch() const { return pitch; }
  void setPitch(float value) { pitch = value; }

  float getFovY() const { return fov_y; }
  float getNearPlane() const { return near_plane; }
  float getFarPlane() const { return far_plane; }

  math::Vec3 forward() const;
  math::Vec3 right() const;
  math::Vec3 up() const;
  math::Mat4 viewProjection(float aspect_ratio) const;
};

#endif
