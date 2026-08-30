#include "core/gameplay/shooting_range.hpp"

ShootingRangeStepResult coordinateShootingRangeFixedStep(
    PlayerController& player, PrototypeRifle& rifle, PhysicsWorld& physics,
    ShootingTargets& targets, float delta_seconds) {
  player.fixedStep(delta_seconds);
  rifle.advanceFixedStep(delta_seconds);
  targets.fixedStep(delta_seconds);

  const PlayerAim aim = player.currentAim(rifle.recoilPitchDegrees());
  const std::optional<PhysicsStaticRay> shot =
      rifle.tryFire(aim.eye_position, aim.direction);
  if (!shot) {
    return {};
  }

  ShootingRangeStepResult result;
  result.shot_emitted = true;
  result.static_hit = physics.closestStaticHit(*shot);
  if (result.static_hit) {
    result.target_damaged =
        targets.applyHit(result.static_hit->solid_index);
  }
  return result;
}
