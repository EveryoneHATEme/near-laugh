#ifndef CORE_PLATFORM_INPUT_HPP
#define CORE_PLATFORM_INPUT_HPP

#include <array>
#include <cstddef>

enum class PhysicalKey : std::size_t {
  W,
  A,
  S,
  D,
  Space,
  LeftShift,
  LeftControl,
  Escape,
  E,
  Count
};

enum class PhysicalMouseButton : std::size_t { Left, Right, Middle, Count };

struct PhysicalInputSnapshot {
  std::array<bool, static_cast<std::size_t>(PhysicalKey::Count)> keys{};
  std::array<bool, static_cast<std::size_t>(PhysicalMouseButton::Count)>
      mouse_buttons{};
  double cursor_delta_x{};
  double cursor_delta_y{};

  [[nodiscard]] bool isKeyDown(PhysicalKey key) const noexcept;
  [[nodiscard]] bool isMouseButtonDown(
      PhysicalMouseButton button) const noexcept;
};

class InputAccumulator {
 public:
  void beginEventBatch() noexcept;
  void setKey(PhysicalKey key, bool down) noexcept;
  void setMouseButton(PhysicalMouseButton button, bool down) noexcept;
  void addCursorPosition(double x, double y) noexcept;
  void resetCursorTracking() noexcept;

  [[nodiscard]] const PhysicalInputSnapshot& snapshot() const noexcept;

 private:
  PhysicalInputSnapshot snapshot_{};
  bool has_cursor_position_{};
  double cursor_x_{};
  double cursor_y_{};
};

#endif
