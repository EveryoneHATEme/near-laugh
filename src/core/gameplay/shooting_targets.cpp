#include "core/gameplay/shooting_targets.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

ShootingTargets::ShootingTargets(const PrototypeLevel& level) {
  if (!prototypeLevelIsValid(level)) {
    throw std::invalid_argument(
        "Shooting targets require a valid immutable prototype level");
  }
  for (std::size_t index = 0; index < targets_.size(); ++index) {
    targets_[index] = {level.targetDescriptions()[index].solid_index,
                       level.targetStartingHealth(), 0.0F};
  }
}

bool ShootingTargets::applyHit(std::size_t solid_index, int damage) {
  if (damage < 0) {
    throw std::invalid_argument("Shooting target damage cannot be negative");
  }
  const auto target = std::find_if(
      targets_.begin(), targets_.end(),
      [solid_index](const ShootingTargetState& candidate) {
        return candidate.solid_index == solid_index;
      });
  if (target == targets_.end() || target->destroyed() || damage == 0) {
    return false;
  }
  target->health = std::max(0, target->health - damage);
  target->highlight_seconds_remaining = prototype_target_highlight_seconds;
  return true;
}

void ShootingTargets::fixedStep(float delta_seconds) {
  if (!(delta_seconds > 0.0F) || !std::isfinite(delta_seconds)) {
    throw std::invalid_argument(
        "Shooting target fixed step requires a finite positive delta");
  }
  for (ShootingTargetState& target : targets_) {
    target.highlight_seconds_remaining =
        std::max(0.0F, target.highlight_seconds_remaining - delta_seconds);
  }
}

PrototypeScenePresentation ShootingTargets::presentation() const noexcept {
  PrototypeScenePresentation result{};
  for (const ShootingTargetState& target : targets_) {
    const std::uint32_t solid_mask = std::uint32_t{1} << target.solid_index;
    if (target.highlight_seconds_remaining > 0.0F) {
      result.highlighted_solid_mask |= solid_mask;
    }
    if (target.destroyed()) {
      result.dimmed_solid_mask |= solid_mask;
    }
  }
  return result;
}

const ShootingTargetState& ShootingTargets::target(std::size_t index) const {
  if (index >= targets_.size()) {
    throw std::out_of_range("Shooting target index is out of range");
  }
  return targets_[index];
}
