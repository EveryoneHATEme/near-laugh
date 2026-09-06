#ifndef CORE_GAMEPLAY_LIGHT_SWITCH_CONTROLLER_HPP
#define CORE_GAMEPLAY_LIGHT_SWITCH_CONTROLLER_HPP

#include "core/player/player_controller.hpp"
#include "core/world/light_switch.hpp"

class LightSwitchController {
 public:
  explicit LightSwitchController(
      const std::optional<PrototypeLightSwitch>& definition) noexcept;
  // One call per sampled batch, after simulation with that batch's displayed
  // view. Inactive batches observe releases and consume presses as well.
  void update(bool interact_down, bool active, const PlayerViewPose& view,
              const PhysicsWorld& physics);
  [[nodiscard]] const std::array<bool, prototype_point_light_count>&
  pointLightEnabled() const noexcept {
    return enabled_;
  }

 private:
  const std::optional<PrototypeLightSwitch>& definition_;
  std::array<bool, prototype_point_light_count> enabled_;
  bool armed_{};
};

#endif
