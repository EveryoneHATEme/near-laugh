#ifndef CORE_CAMERA_FREE_FLY_CAMERA_HPP
#define CORE_CAMERA_FREE_FLY_CAMERA_HPP

#include <chrono>
#include <optional>

#include "core/frame.hpp"
#include "core/input/fps_input.hpp"

struct CameraPosition {
  float x{};
  float y{};
  float z{};
};

class FreeFlyCamera {
 public:
  void update(const FpsActionSnapshot& actions, double elapsed_seconds);

  [[nodiscard]] CameraFrame frame(float framebuffer_aspect) const;
  [[nodiscard]] CameraPosition position() const noexcept { return position_; }
  [[nodiscard]] float yawDegrees() const noexcept { return yaw_degrees_; }
  [[nodiscard]] float pitchDegrees() const noexcept { return pitch_degrees_; }

 private:
  CameraPosition position_{0.0F, 2.0F, 8.0F};
  float yaw_degrees_{-90.0F};
  float pitch_degrees_{};
};

[[nodiscard]] constexpr double boundedElapsedSeconds(
    double elapsed_seconds) noexcept {
  if (elapsed_seconds <= 0.0) {
    return 0.0;
  }
  return elapsed_seconds < 0.1 ? elapsed_seconds : 0.1;
}

class FrameClock {
 public:
  using Clock = std::chrono::steady_clock;

  [[nodiscard]] double sample(Clock::time_point now) noexcept;
  void reset() noexcept { previous_.reset(); }

 private:
  std::optional<Clock::time_point> previous_{};
};

enum class CursorCaptureTransition { None, Release, Capture };

[[nodiscard]] constexpr CursorCaptureTransition cursorCaptureTransition(
    bool cursor_captured, const FpsActionSnapshot& actions) noexcept {
  if (actions.menu) {
    return cursor_captured ? CursorCaptureTransition::Release
                           : CursorCaptureTransition::None;
  }
  if (!cursor_captured && actions.primary_action) {
    return CursorCaptureTransition::Capture;
  }
  return CursorCaptureTransition::None;
}

[[nodiscard]] constexpr bool freeFlyNavigationActive(
    bool cursor_captured, CursorCaptureTransition transition) noexcept {
  return cursor_captured && transition == CursorCaptureTransition::None;
}

#endif
