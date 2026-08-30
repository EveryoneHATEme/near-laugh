#ifndef CORE_GAMEPLAY_PROTOTYPE_RIFLE_HPP
#define CORE_GAMEPLAY_PROTOTYPE_RIFLE_HPP

#include <optional>

#include "core/physics/physics_world.hpp"

inline constexpr float prototype_rifle_fire_interval_seconds = 0.1F;
inline constexpr float prototype_rifle_maximum_range = 50.0F;
inline constexpr float prototype_rifle_recoil_kick_degrees = 2.0F;
inline constexpr float prototype_rifle_maximum_recoil_degrees = 8.0F;
inline constexpr float prototype_rifle_recovery_degrees_per_second = 6.0F;

class PrototypeRifle {
 public:
  void sampleTrigger(bool trigger_down, bool controls_active) noexcept;
  void advanceFixedStep(float delta_seconds);
  [[nodiscard]] std::optional<PhysicsStaticRay> tryFire(
      PhysicsVector shot_origin, PhysicsVector shot_direction);

  [[nodiscard]] float recoilPitchDegrees() const noexcept {
    return recoil_pitch_degrees_;
  }
  [[nodiscard]] float cooldownSeconds() const noexcept {
    return cooldown_seconds_;
  }
  [[nodiscard]] bool triggerPending() const noexcept {
    return trigger_pending_;
  }

 private:
  float cooldown_seconds_{};
  float recoil_pitch_degrees_{};
  bool trigger_held_{};
  bool trigger_was_down_{};
  bool trigger_pending_{};
  bool suppress_until_release_{};
};

#endif
