#include "core/gameplay/prototype_rifle.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
bool finite(PhysicsVector vector) noexcept {
  return std::isfinite(vector.x) && std::isfinite(vector.y) &&
         std::isfinite(vector.z);
}
}  // namespace

void PrototypeRifle::sampleTrigger(bool trigger_down,
                                   bool controls_active) noexcept {
  if (!controls_active) {
    trigger_held_ = false;
    trigger_pending_ = false;
    suppress_until_release_ = suppress_until_release_ || trigger_down;
    trigger_was_down_ = trigger_down;
    return;
  }

  if (suppress_until_release_) {
    if (!trigger_down) {
      suppress_until_release_ = false;
    }
    trigger_held_ = false;
    trigger_was_down_ = trigger_down;
    return;
  }

  if (trigger_down && !trigger_was_down_) {
    trigger_pending_ = true;
  }
  trigger_held_ = trigger_down;
  trigger_was_down_ = trigger_down;
}

void PrototypeRifle::advanceFixedStep(float delta_seconds) {
  if (!(delta_seconds > 0.0F) || !std::isfinite(delta_seconds)) {
    throw std::invalid_argument(
        "Rifle fixed step requires a finite positive delta");
  }

  constexpr float timer_tolerance_seconds = 0.000001F;
  cooldown_seconds_ =
      cooldown_seconds_ <= delta_seconds + timer_tolerance_seconds
          ? 0.0F
          : cooldown_seconds_ - delta_seconds;
  recoil_pitch_degrees_ =
      std::max(0.0F, recoil_pitch_degrees_ -
                         prototype_rifle_recovery_degrees_per_second *
                             delta_seconds);
}

std::optional<PhysicsStaticRay> PrototypeRifle::tryFire(
    PhysicsVector shot_origin, PhysicsVector shot_direction) {
  if (cooldown_seconds_ > 0.0F || (!trigger_held_ && !trigger_pending_)) {
    return std::nullopt;
  }
  const float direction_length_squared =
      shot_direction.x * shot_direction.x +
      shot_direction.y * shot_direction.y +
      shot_direction.z * shot_direction.z;
  if (!finite(shot_origin) || !finite(shot_direction) ||
      !std::isfinite(direction_length_squared) ||
      !(direction_length_squared > 0.0F)) {
    throw std::invalid_argument(
        "Rifle shot requires finite origin and finite non-zero direction");
  }

  trigger_pending_ = false;
  cooldown_seconds_ = prototype_rifle_fire_interval_seconds;
  const PhysicsStaticRay shot{shot_origin, shot_direction,
                              prototype_rifle_maximum_range};
  recoil_pitch_degrees_ =
      std::min(prototype_rifle_maximum_recoil_degrees,
               recoil_pitch_degrees_ + prototype_rifle_recoil_kick_degrees);
  return shot;
}
