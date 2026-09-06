#ifndef CORE_GAMEPLAY_LIGHT_SWITCH_CONTROLLER_HPP
#define CORE_GAMEPLAY_LIGHT_SWITCH_CONTROLLER_HPP

#include "core/world/light_switch.hpp"

class LightSwitchController {
 public:
  explicit LightSwitchController(
      const std::optional<PrototypeLightSwitch>& definition) noexcept;
  void toggle() noexcept;
  [[nodiscard]] const std::array<bool, prototype_point_light_count>&
  pointLightEnabled() const noexcept {
    return enabled_;
  }

 private:
  const std::optional<PrototypeLightSwitch>& definition_;
  std::array<bool, prototype_point_light_count> enabled_;
};

#endif
