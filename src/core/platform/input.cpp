#include "core/platform/input.hpp"

namespace {
template <typename Enum>
constexpr std::size_t indexOf(Enum value) noexcept {
  return static_cast<std::size_t>(value);
}
}  // namespace

bool InputSnapshot::isKeyDown(Key key) const noexcept {
  return keys[indexOf(key)];
}

bool InputSnapshot::isMouseButtonDown(MouseButton button) const noexcept {
  return mouse_buttons[indexOf(button)];
}

void InputAccumulator::beginEventBatch() noexcept {
  snapshot_.cursor_delta_x = 0.0;
  snapshot_.cursor_delta_y = 0.0;
}

void InputAccumulator::setKey(Key key, bool down) noexcept {
  snapshot_.keys[indexOf(key)] = down;
}

void InputAccumulator::setMouseButton(MouseButton button, bool down) noexcept {
  snapshot_.mouse_buttons[indexOf(button)] = down;
}

void InputAccumulator::addCursorPosition(double x, double y) noexcept {
  if (has_cursor_position_) {
    snapshot_.cursor_delta_x += x - cursor_x_;
    snapshot_.cursor_delta_y += y - cursor_y_;
  }
  cursor_x_ = x;
  cursor_y_ = y;
  has_cursor_position_ = true;
}

void InputAccumulator::resetCursorTracking() noexcept {
  has_cursor_position_ = false;
  snapshot_.cursor_delta_x = 0.0;
  snapshot_.cursor_delta_y = 0.0;
}

const InputSnapshot& InputAccumulator::snapshot() const noexcept {
  return snapshot_;
}
