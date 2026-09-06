#ifndef CORE_GAMEPLAY_AUTHORED_INTERACTION_HPP
#define CORE_GAMEPLAY_AUTHORED_INTERACTION_HPP

#include <array>
#include <optional>

#include "core/gameplay/door_controller.hpp"
#include "core/gameplay/light_switch_controller.hpp"
#include "core/input/player_input.hpp"
#include "core/player/player_controller.hpp"

class AuthoredInteraction {
 public:
  // Consumes every sampled batch, including inactive and minimized batches.
  [[nodiscard]] std::optional<DoorResult> update(
      const PlayerActionSnapshot& input, bool active,
      const PlayerViewPose& view, const PrototypeLevel& level,
      const PhysicsWorld& physics, DoorController& doors,
      LightSwitchController& light_switch);

 private:
  std::array<bool, 3> armed_{};
};

#endif
