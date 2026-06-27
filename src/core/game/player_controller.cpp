#include "player_controller.hpp"

#include <SDL3/SDL.h>

PlayerController::PlayerController() {
  camera.setPosition({0.0f, eye_height, 5.5f});
}

math::AABB PlayerController::boundsAt(const math::Vec3& eye_position) const {
  return {{eye_position.x - radius, 0.0f, eye_position.z - radius},
          {eye_position.x + radius, body_height, eye_position.z + radius}};
}

bool PlayerController::collidesAt(
    const math::Vec3& eye_position,
    const std::vector<math::AABB>& colliders) const {
  const math::AABB player_bounds = boundsAt(eye_position);
  for (const math::AABB& collider : colliders) {
    if (math::intersects(player_bounds, collider)) {
      return true;
    }
  }
  return false;
}

math::Vec3 PlayerController::moveWithCollision(
    const math::Vec3& start, const math::Vec3& delta,
    const std::vector<math::AABB>& colliders) const {
  math::Vec3 result = start;

  math::Vec3 candidate = result;
  candidate.x += delta.x;
  if (!collidesAt(candidate, colliders)) {
    result.x = candidate.x;
  }

  candidate = result;
  candidate.z += delta.z;
  if (!collidesAt(candidate, colliders)) {
    result.z = candidate.z;
  }

  return result;
}

void PlayerController::update(float dt, const InputState& input,
                              const std::vector<math::AABB>& colliders) {
  camera.setYaw(camera.getYaw() + input.mouseDeltaX() * mouse_sensitivity);
  camera.setPitch(math::clamp(camera.getPitch() -
                                  input.mouseDeltaY() * mouse_sensitivity,
                              math::radians(-88.0f), math::radians(88.0f)));

  const math::Vec3 forward = camera.forward();
  const math::Vec3 flat_forward =
      math::normalize({forward.x, 0.0f, forward.z});
  const math::Vec3 flat_right =
      math::normalize({camera.right().x, 0.0f, camera.right().z});

  math::Vec3 movement{};
  if (input.isKeyDown(SDL_SCANCODE_W)) {
    movement += flat_forward;
  }
  if (input.isKeyDown(SDL_SCANCODE_S)) {
    movement -= flat_forward;
  }
  if (input.isKeyDown(SDL_SCANCODE_D)) {
    movement += flat_right;
  }
  if (input.isKeyDown(SDL_SCANCODE_A)) {
    movement -= flat_right;
  }

  if (math::lengthSquared(movement) > 0.0f) {
    movement = math::normalize(movement) * move_speed * dt;
    camera.setPosition(
        moveWithCollision(camera.getPosition(), movement, colliders));
  }
}
