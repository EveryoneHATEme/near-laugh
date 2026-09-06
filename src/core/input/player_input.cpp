#include "core/input/player_input.hpp"

PlayerActionSnapshot PlayerInputMapper::map(
    const PhysicalInputSnapshot& physical) const noexcept {
  return {
      physical.isKeyDown(PhysicalKey::W),
      physical.isKeyDown(PhysicalKey::S),
      physical.isKeyDown(PhysicalKey::A),
      physical.isKeyDown(PhysicalKey::D),
      physical.isKeyDown(PhysicalKey::Space),
      physical.isKeyDown(PhysicalKey::LeftShift),
      physical.isKeyDown(PhysicalKey::LeftControl),
      physical.isKeyDown(PhysicalKey::Escape),
      physical.isKeyDown(PhysicalKey::E),
      physical.isMouseButtonDown(PhysicalMouseButton::Left),
      physical.isMouseButtonDown(PhysicalMouseButton::Right),
      physical.cursor_delta_x,
      physical.cursor_delta_y,
  };
}
