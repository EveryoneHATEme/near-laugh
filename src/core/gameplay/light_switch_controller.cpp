#include "core/gameplay/light_switch_controller.hpp"

LightSwitchController::LightSwitchController(
    const std::optional<PrototypeLightSwitch>& definition) noexcept
    : definition_(definition), enabled_(initialPointLightEnabled(definition)) {}

void LightSwitchController::toggle() noexcept {
  if (definition_) {
    const auto index = definition_->point_light_index;
    enabled_[index] = !enabled_[index];
  }
}
