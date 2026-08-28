#ifndef CORE_FRAME_HPP
#define CORE_FRAME_HPP

#include <cstdint>

struct FramebufferExtent {
  std::uint32_t width{};
  std::uint32_t height{};

  [[nodiscard]] constexpr bool isZero() const noexcept {
    return width == 0 || height == 0;
  }
};

struct FrameRequest {
  FramebufferExtent framebuffer{};
  bool framebuffer_resized{};
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
    bool framebuffer_resized) noexcept {
  if (close_requested) {
    return {};
  }
  if (framebuffer.isZero()) {
    return {LoopAction::WaitForEvents, {framebuffer, framebuffer_resized}};
  }
  return {LoopAction::Render, {framebuffer, framebuffer_resized}};
}

#endif
