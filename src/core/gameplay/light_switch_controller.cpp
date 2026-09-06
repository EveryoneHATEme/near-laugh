#include "core/gameplay/light_switch_controller.hpp"

#include <cmath>

LightSwitchController::LightSwitchController(
    const std::optional<PrototypeLightSwitch>& definition) noexcept
    : definition_(definition), enabled_(initialPointLightEnabled(definition)) {}

void LightSwitchController::update(bool interact_down, bool active,
                                   const PlayerViewPose& view,
                                   const PhysicsWorld& physics) {
  if (!interact_down) {
    armed_ = true;
    return;
  }
  const bool pressed = armed_;
  armed_ = false;
  if (!pressed || !active || !definition_) return;
  const WorldPosition eye{view.position.x, view.position.y, view.position.z};
  const WorldPosition direction{view.direction.x, view.direction.y,
                                view.direction.z};
  const auto distance = lightSwitchRayDistance(*definition_, eye, direction);
  if (!distance || *distance > 2.0F) return;
  const float length = std::hypot(direction.x, direction.y, direction.z);
  const WorldPosition hit{eye.x + direction.x / length * *distance,
                          eye.y + direction.y / length * *distance,
                          eye.z + direction.z / length * *distance};
  if (!physics.staticSegmentBlocked(eye, hit)) {
    const auto index = definition_->point_light_index;
    enabled_[index] = !enabled_[index];
  }
}
