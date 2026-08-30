#ifndef CORE_GAMEPLAY_SHOOTING_TARGETS_HPP
#define CORE_GAMEPLAY_SHOOTING_TARGETS_HPP

#include <array>
#include <cstddef>

#include "core/frame.hpp"
#include "core/world/prototype_level.hpp"

inline constexpr int prototype_target_damage = 25;
inline constexpr float prototype_target_highlight_seconds = 0.15F;

struct ShootingTargetState {
  std::size_t solid_index{};
  int health{};
  float highlight_seconds_remaining{};

  [[nodiscard]] bool destroyed() const noexcept { return health == 0; }
};

class ShootingTargets {
 public:
  explicit ShootingTargets(const PrototypeLevel& level);

  [[nodiscard]] bool applyHit(std::size_t solid_index,
                              int damage = prototype_target_damage);
  void fixedStep(float delta_seconds);
  [[nodiscard]] PrototypeScenePresentation presentation() const noexcept;
  [[nodiscard]] const ShootingTargetState& target(std::size_t index) const;
  [[nodiscard]] std::size_t size() const noexcept { return targets_.size(); }

 private:
  std::array<ShootingTargetState, prototype_target_count> targets_{};
};

#endif
