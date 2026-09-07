#ifndef CORE_GAMEPLAY_LIGHT_SWITCH_CONTROLLER_HPP
#define CORE_GAMEPLAY_LIGHT_SWITCH_CONTROLLER_HPP

#include "core/world/light_switch.hpp"

class LightSwitchController {
 public:
  explicit LightSwitchController(
      const PrototypeEnvironmentLight& lights,
      const std::vector<PrototypeLightSwitch>& switches);
  void toggle(std::size_t switch_index);
  [[nodiscard]] const std::vector<std::uint8_t>& pointLightEnabled()
      const noexcept {
    return enabled_;
  }

 private:
  std::vector<std::size_t> links_;
  std::vector<std::uint8_t> enabled_;
};

#endif
