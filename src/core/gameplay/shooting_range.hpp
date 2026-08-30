#ifndef CORE_GAMEPLAY_SHOOTING_RANGE_HPP
#define CORE_GAMEPLAY_SHOOTING_RANGE_HPP

#include <optional>

#include "core/gameplay/prototype_rifle.hpp"
#include "core/gameplay/shooting_targets.hpp"
#include "core/player/player_controller.hpp"

struct ShootingRangeStepResult {
  bool shot_emitted{};
  std::optional<PhysicsStaticRayHit> static_hit{};
  bool target_damaged{};
};

[[nodiscard]] ShootingRangeStepResult coordinateShootingRangeFixedStep(
    PlayerController& player, PrototypeRifle& rifle, PhysicsWorld& physics,
    ShootingTargets& targets, float delta_seconds);

#endif
