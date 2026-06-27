#ifndef CORE_GAME_PLAYER_CONTROLLER_HPP
#define CORE_GAME_PLAYER_CONTROLLER_HPP

#include <vector>

#include "../input/input_state.hpp"
#include "../math/geometry.hpp"
#include "camera.hpp"

class PlayerController {
 private:
  Camera camera;
  float move_speed{4.5f};
  float mouse_sensitivity{0.0022f};
  float radius{0.28f};
  float body_height{1.75f};
  float eye_height{1.35f};

  bool collidesAt(const math::Vec3& eye_position,
                  const std::vector<math::AABB>& colliders) const;

 public:
  PlayerController();

  void update(float dt, const InputState& input,
              const std::vector<math::AABB>& colliders);
  math::Vec3 moveWithCollision(const math::Vec3& start,
                               const math::Vec3& delta,
                               const std::vector<math::AABB>& colliders) const;

  math::AABB boundsAt(const math::Vec3& eye_position) const;
  const Camera& getCamera() const { return camera; }
  Camera& getCamera() { return camera; }
  void setPosition(const math::Vec3& position) { camera.setPosition(position); }
};

#endif
