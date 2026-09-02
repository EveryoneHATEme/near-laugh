#ifndef EDITOR_EDITOR_CAMERA_HPP
#define EDITOR_EDITOR_CAMERA_HPP

#include <chrono>
#include <optional>

#include "core/frame.hpp"
#include "core/platform/input.hpp"

struct EditorCameraPosition {
  float x{};
  float y{};
  float z{};
};

struct EditorUiCaptureIntent {
  bool keyboard{};
  bool pointer{};
};

struct EditorNavigationInput {
  bool move_forward{};
  bool move_backward{};
  bool move_left{};
  bool move_right{};
  bool move_up{};
  bool move_down{};
  bool sprint{};
  double look_delta_x{};
  double look_delta_y{};
};

[[nodiscard]] EditorNavigationInput editorNavigationInput(
    const PhysicalInputSnapshot& physical, bool scene_navigation_active,
    EditorUiCaptureIntent capture) noexcept;

class EditorCamera {
 public:
  void update(const EditorNavigationInput& input, double elapsed_seconds);

  [[nodiscard]] CameraFrame frame(float framebuffer_aspect) const;
  [[nodiscard]] EditorCameraPosition position() const noexcept {
    return position_;
  }
  [[nodiscard]] float yawDegrees() const noexcept { return yaw_degrees_; }
  [[nodiscard]] float pitchDegrees() const noexcept { return pitch_degrees_; }

 private:
  EditorCameraPosition position_{0.0F, 2.0F, 8.0F};
  float yaw_degrees_{-90.0F};
  float pitch_degrees_{};
};

[[nodiscard]] constexpr double boundedEditorElapsedSeconds(
    double elapsed_seconds) noexcept {
  if (elapsed_seconds <= 0.0) {
    return 0.0;
  }
  return elapsed_seconds < 0.1 ? elapsed_seconds : 0.1;
}

class EditorFrameClock {
 public:
  using Clock = std::chrono::steady_clock;

  [[nodiscard]] double sample(Clock::time_point now) noexcept;
  void reset() noexcept { previous_.reset(); }

 private:
  std::optional<Clock::time_point> previous_{};
};

#endif
