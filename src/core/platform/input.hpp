#ifndef CORE_PLATFORM_INPUT_HPP
#define CORE_PLATFORM_INPUT_HPP

#include <array>
#include <cstddef>

enum class Key : std::size_t {
  MoveForward,
  MoveBackward,
  MoveLeft,
  MoveRight,
  Jump,
  Sprint,
  Crouch,
  Escape,
  Count
};

enum class MouseButton : std::size_t { Left, Right, Middle, Count };

struct InputSnapshot {
  std::array<bool, static_cast<std::size_t>(Key::Count)> keys{};
  std::array<bool, static_cast<std::size_t>(MouseButton::Count)>
      mouse_buttons{};
  double cursor_delta_x{};
  double cursor_delta_y{};

  [[nodiscard]] bool isKeyDown(Key key) const noexcept;
  [[nodiscard]] bool isMouseButtonDown(MouseButton button) const noexcept;
};

class InputAccumulator {
 public:
  void beginEventBatch() noexcept;
  void setKey(Key key, bool down) noexcept;
  void setMouseButton(MouseButton button, bool down) noexcept;
  void addCursorPosition(double x, double y) noexcept;
  void resetCursorTracking() noexcept;

  [[nodiscard]] const InputSnapshot& snapshot() const noexcept;

 private:
  InputSnapshot snapshot_{};
  bool has_cursor_position_{};
  double cursor_x_{};
  double cursor_y_{};
};

#endif
