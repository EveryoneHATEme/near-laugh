#include "core/engine.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

#include "core/render/validation_diagnostics.hpp"

namespace {
const LevelEntry& selectedEntry(const PrototypeLevel& level,
                                const near_laugh::RuntimeConfig& config,
                                const std::filesystem::path& path) {
  const auto& id = config.entry_id ? *config.entry_id : level.defaultEntryId();
  if (const auto* entry = level.entry(id)) return *entry;
  const auto native_text = path.u8string();
  throw std::runtime_error("Selected level '" +
                           std::string(native_text.begin(), native_text.end()) +
                           "' has no entry '" + id + "'");
}
}  // namespace

Engine::Engine(const near_laugh::RuntimeConfig& config,
               ValidationDiagnostics& diagnostics)
    : platform_(),
      window_(platform_, config.window_width, config.window_height,
              config.window_title),
      resources_(
          resolveRuntimeResources(config.resource_root, config.level_path)),
      level_(loadPrototypeLevel(resources_.prototype_level)),
      entry_(selectedEntry(level_, config, resources_.prototype_level)),
      physics_(level_, entry_),
      player_(physics_, entry_.pose.yaw_degrees),
      light_switch_(level_.lightSwitch()),
      doors_(level_.doors()),
      renderer_(window_, window_.framebufferExtent(), level_,
                {std::move(resources_.scene_vertex_shader),
                 std::move(resources_.scene_fragment_shader), resources_.root},
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
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_);
      return false;
    case LoopAction::WaitForEvents:
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_);
      window_.waitEvents();
      input_ = input_mapper_.map(window_.input());
      samplePlayerInput(input_);
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_);
      fixed_step_.reset();
      return !window_.shouldClose();
    case LoopAction::Render: {
      const bool controls_active = samplePlayerInput(input_);
      const FixedStepBatch simulation =
          fixed_step_.sample(FixedStepAccumulator::Clock::now());
      for (int step = 0; step < simulation.complete_steps; ++step) {
        player_.fixedStep(
            static_cast<float>(FixedStepAccumulator::step_seconds));
        doors_.fixedStep(static_cast<float>(FixedStepAccumulator::step_seconds),
                         physics_);
      }

      FrameRequest frame = decision.frame;
      const float aspect = static_cast<float>(framebuffer.width) /
                           static_cast<float>(framebuffer.height);
      const PlayerViewPose view =
          player_.viewPose(simulation.interpolation_alpha);
      frame.camera = player_.cameraFrame(aspect, view);
      frame.spot_light = flashlight_.spotLight(view);
      (void)interaction_.update(input_, controls_active, view, level_, physics_,
                                doors_, light_switch_);
      frame.point_light_enabled = light_switch_.pointLightEnabled();
      frame.opaque_boxes = doors_.presentation();
      const FrameOutcome outcome = renderer_.renderFrame(frame);
      return runtimeContinuesAfter(outcome) && !window_.shouldClose();
    }
  }
  return false;
}

bool Engine::samplePlayerInput(const PlayerActionSnapshot& input) {
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
  const bool controls_active = playerControlsActive(was_captured, transition);
  player_.sampleInput(input, controls_active);
  flashlight_.samplePrimaryAction(input.primary_action, controls_active);
  return controls_active;
}
