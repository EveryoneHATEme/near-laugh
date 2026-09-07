#include "core/gameplay/light_switch_controller.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/world/prototype_level.hpp"

LightSwitchController::LightSwitchController(
    const PrototypeEnvironmentLight& lights,
    const std::vector<PrototypeLightSwitch>& switches)
    : enabled_(initialPointLightEnabled(lights)) {
  if (!prototypeEnvironmentLightIsValid(lights) ||
      switches.size() > level_maximum_light_switch_count)
    throw std::invalid_argument(
        "Light controller requires the supported authored light set");
  for (const auto& value : switches) {
    const auto found = std::find_if(
        lights.point_lights.begin(), lights.point_lights.end(),
        [&](const auto& light) { return light.id == value.light_id; });
    if (found == lights.point_lights.end())
      throw std::invalid_argument("Switch '" + value.id +
                                  "' references missing light '" +
                                  value.light_id + "'");
    links_.push_back(
        static_cast<std::size_t>(found - lights.point_lights.begin()));
  }
}

void LightSwitchController::toggle(std::size_t switch_index) {
  const auto light = links_.at(switch_index);
  enabled_[light] = !enabled_[light];
}
