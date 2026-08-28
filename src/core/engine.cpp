#include "core/engine.hpp"

#include <chrono>
#include <utility>

#include "core/render/validation_diagnostics.hpp"

Engine::Engine(const near_laugh::RuntimeConfig& config,
               RuntimeResources resources, ValidationDiagnostics& diagnostics)
    : platform_(),
      window_(platform_, config.window_width, config.window_height,
              config.window_title),
      renderer_(window_, window_.framebufferExtent(),
                {std::move(resources.scene_vertex_shader),
                 std::move(resources.scene_fragment_shader)},
                diagnostics) {
  window_.setCursorCaptured(true);
  frame_clock_.reset();
}

void Engine::run() {
  while (tick()) {
  }
}

bool Engine::tick() {
  window_.pollEvents();
  input_ = input_mapper_.map(window_.input());
  const FramebufferExtent framebuffer = window_.framebufferExtent();
  const LoopDecision decision = decideLoopAction(
      window_.shouldClose(), framebuffer, window_.consumeFramebufferResize());
  switch (decision.action) {
    case LoopAction::Stop:
      return false;
    case LoopAction::WaitForEvents:
      window_.waitEvents();
      input_ = input_mapper_.map(window_.input());
      frame_clock_.reset();
      return !window_.shouldClose();
    case LoopAction::Render: {
      const bool was_captured = window_.cursorCaptured();
      const CursorCaptureTransition capture_transition =
          cursorCaptureTransition(was_captured, input_);
      switch (capture_transition) {
        case CursorCaptureTransition::Release:
          window_.setCursorCaptured(false);
          frame_clock_.reset();
          break;
        case CursorCaptureTransition::Capture:
          window_.setCursorCaptured(true);
          frame_clock_.reset();
          break;
        case CursorCaptureTransition::None:
          break;
      }

      if (freeFlyNavigationActive(was_captured, capture_transition)) {
        camera_.update(input_, frame_clock_.sample(FrameClock::Clock::now()));
      } else {
        frame_clock_.reset();
      }

      FrameRequest frame = decision.frame;
      const float aspect = static_cast<float>(framebuffer.width) /
                           static_cast<float>(framebuffer.height);
      frame.camera = camera_.frame(aspect);
      const FrameOutcome outcome = renderer_.renderFrame(frame);
      return runtimeContinuesAfter(outcome) && !window_.shouldClose();
    }
  }
  return false;
}
