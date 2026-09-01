#ifndef CORE_GAMEPLAY_PLAYER_FLASHLIGHT_HPP
#define CORE_GAMEPLAY_PLAYER_FLASHLIGHT_HPP

#include "core/frame.hpp"
#include "core/player/player_controller.hpp"

class PlayerFlashlight {
 public:
  void samplePrimaryAction(bool primary_down, bool controls_active) noexcept;

  [[nodiscard]] SpotLightFrame spotLight(
      const PlayerViewPose& view) const noexcept;
  [[nodiscard]] bool enabled() const noexcept { return enabled_; }
  [[nodiscard]] bool armed() const noexcept { return armed_; }

 private:
  bool enabled_{};
  bool armed_{true};
};

#endif
