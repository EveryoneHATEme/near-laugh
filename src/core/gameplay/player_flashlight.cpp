#include "core/gameplay/player_flashlight.hpp"

#include <cmath>

namespace {
constexpr float flashlight_range = 16.0F;
constexpr float flashlight_intensity = 1.35F;
constexpr float flashlight_inner_angle_degrees = 13.0F;
constexpr float flashlight_outer_angle_degrees = 22.0F;
constexpr float degrees_to_radians = 3.14159265358979323846F / 180.0F;
}  // namespace

void PlayerFlashlight::samplePrimaryAction(bool primary_down,
                                           bool controls_active) noexcept {
  if (!primary_down) {
    armed_ = true;
    return;
  }
  if (!armed_) {
    return;
  }
  armed_ = false;
  if (controls_active) {
    enabled_ = !enabled_;
  }
}

SpotLightFrame PlayerFlashlight::spotLight(
    const PlayerViewPose& view) const noexcept {
  if (!enabled_) {
    return {};
  }
  SpotLightFrame light{};
  light.position_and_range = {view.position.x, view.position.y, view.position.z,
                              flashlight_range};
  light.direction_and_inner_cosine = {
      view.direction.x, view.direction.y, view.direction.z,
      std::cos(flashlight_inner_angle_degrees * degrees_to_radians)};
  light.color_and_intensity = {0.92F, 0.96F, 1.0F, flashlight_intensity};
  light.outer_cosine_and_enabled = {
      std::cos(flashlight_outer_angle_degrees * degrees_to_radians), 1.0F, 0.0F,
      0.0F};
  return light;
}
