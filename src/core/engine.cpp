#include "core/engine.hpp"

#include <chrono>
#include <utility>

#include "core/render/validation_diagnostics.hpp"

Engine::Engine(const near_laugh::RuntimeConfig& config,
               RuntimeResources resources, ValidationDiagnostics& diagnostics)
    : platform_(),
      window_(platform_, config.window_width, config.window_height,
              config.window_title),
      physics_(level_),
      player_(physics_, level_.playerSpawn().yaw_degrees),
      targets_(level_),
      renderer_(window_, window_.framebufferExtent(), level_,
                {std::move(resources.scene_vertex_shader),
                 std::move(resources.scene_fragment_shader)},
                diagnostics) {
  window_.setCursorCaptured(true);
  fixed_step_.reset();
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
      samplePlayerInput(input_);
      fixed_step_.reset();
      return !window_.shouldClose();
    case LoopAction::Render: {
      samplePlayerInput(input_);
      const FixedStepBatch simulation =
          fixed_step_.sample(FixedStepAccumulator::Clock::now());
      for (int step = 0; step < simulation.complete_steps; ++step) {
        static_cast<void>(coordinateShootingRangeFixedStep(
            player_, rifle_, physics_, targets_,
            static_cast<float>(FixedStepAccumulator::step_seconds)));
      }

      FrameRequest frame = decision.frame;
      const float aspect = static_cast<float>(framebuffer.width) /
                           static_cast<float>(framebuffer.height);
      frame.camera = player_.cameraFrame(aspect, simulation.interpolation_alpha,
                                         rifle_.recoilPitchDegrees());
      frame.scene_presentation = targets_.presentation();
      const FrameOutcome outcome = renderer_.renderFrame(frame);
      return runtimeContinuesAfter(outcome) && !window_.shouldClose();
    }
  }
  return false;
}

void Engine::samplePlayerInput(const FpsActionSnapshot& input) {
  const bool was_captured = window_.cursorCaptured();
  const PlayerCursorCaptureTransition transition =
      playerCursorTransition(was_captured, input);
  switch (transition) {
    case PlayerCursorCaptureTransition::Release:
      window_.setCursorCaptured(false);
      break;
    case PlayerCursorCaptureTransition::Capture:
      window_.setCursorCaptured(true);
      break;
    case PlayerCursorCaptureTransition::None:
      break;
  }
  const bool controls_active =
      playerControlsActive(was_captured, transition);
  player_.sampleInput(input, controls_active);
  rifle_.sampleTrigger(input.primary_action, controls_active);
}
