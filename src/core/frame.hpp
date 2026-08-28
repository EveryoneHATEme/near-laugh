#ifndef CORE_FRAME_HPP
#define CORE_FRAME_HPP

#include <array>
#include <cstdint>
#include <type_traits>

struct FramebufferExtent {
  std::uint32_t width{};
  std::uint32_t height{};

  [[nodiscard]] constexpr bool isZero() const noexcept {
    return width == 0 || height == 0;
  }
};

// Column-major view-projection matrix. The fixed scalar storage keeps the
// runtime-to-render boundary independent of math and graphics libraries.
struct CameraFrame {
  std::array<float, 16> view_projection{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                        0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                        0.0F, 0.0F, 0.0F, 1.0F};
};

static_assert(std::is_standard_layout_v<CameraFrame>);
static_assert(sizeof(CameraFrame) == sizeof(float) * 16);

struct FrameRequest {
  FramebufferExtent framebuffer{};
  bool framebuffer_resized{};
  CameraFrame camera{};
};

[[nodiscard]] constexpr bool frameRequestCanSubmit(
    const FrameRequest& request) noexcept {
  return !request.framebuffer.isZero();
}

enum class FrameOutcome { Rendered, Skipped, Recovered };

[[nodiscard]] constexpr bool runtimeContinuesAfter(
    FrameOutcome outcome) noexcept {
  switch (outcome) {
    case FrameOutcome::Rendered:
    case FrameOutcome::Skipped:
    case FrameOutcome::Recovered:
      return true;
  }
  return false;
}

enum class LoopAction { Stop, WaitForEvents, Render };

struct LoopDecision {
  LoopAction action{LoopAction::Stop};
  FrameRequest frame{};
};

[[nodiscard]] constexpr LoopDecision decideLoopAction(
    bool close_requested, FramebufferExtent framebuffer,
    bool framebuffer_resized, CameraFrame camera = {}) noexcept {
  if (close_requested) {
    return {};
  }
  if (framebuffer.isZero()) {
    return {LoopAction::WaitForEvents,
            {framebuffer, framebuffer_resized, camera}};
  }
  return {LoopAction::Render, {framebuffer, framebuffer_resized, camera}};
}

#endif
