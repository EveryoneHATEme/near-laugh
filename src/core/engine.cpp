#include "core/engine.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "core/render/validation_diagnostics.hpp"

namespace {
double audioNow() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
AudioContent preparedAudio(const std::filesystem::path& root,
                           const LevelAudio& audio, const CaptionFont& font) {
  auto content = prepareAudioContent(root, audio);
  validateAudioCaptions(content, audio, font);
  return content;
}
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
               ValidationDiagnostics& diagnostics, bool audio_fixture,
               AudioOutput output, FrameTimings* timings)
    : platform_(),
      window_(platform_, config.window_width, config.window_height,
              config.window_title),
      resources_(
          resolveRuntimeResources(config.resource_root, config.level_path)),
      level_(loadPrototypeLevel(resources_.prototype_level)),
      entry_(selectedEntry(level_, config, resources_.prototype_level)),
      caption_font_(std::make_shared<CaptionFont>(resources_.root)),
      audio_(level_.audio(), level_.doors(),
             preparedAudio(resources_.root, level_.audio(), *caption_font_),
             level_.audio().sources.empty() ? AudioOutput::Silent : output,
             audioNow()),
      physics_(level_, entry_),
      player_(physics_, entry_.pose.yaw_degrees),
      light_switch_(level_.environmentLight(), level_.lightSwitches()),
      doors_(level_.doors()),
      renderer_(window_, window_.framebufferExtent(), level_,
                {std::move(resources_.scene_vertex_shader),
                 std::move(resources_.scene_fragment_shader), resources_.root,
                 caption_font_, timings},
                diagnostics) {
  window_.setCursorCaptured(true);
  fixed_step_.reset();
  audio_.update(audioNow());
  if (audio_fixture) {
    audio_fixture_.emplace(audio_);
    audio_fixture_->restart();
  } else
    audio_.autoplay();
}

void Engine::run() {
  while (tick()) {
  }
}

bool Engine::tick(const PlayerActionSnapshot* development_input) {
  window_.pollEvents();
  input_ = input_mapper_.map(window_.input());
  if (development_input) input_ = *development_input;
  const FramebufferExtent framebuffer = window_.framebufferExtent();
  const LoopDecision decision = decideLoopAction(
      window_.shouldClose(), framebuffer, window_.consumeFramebufferResize());
  switch (decision.action) {
    case LoopAction::Stop:
      audio_.cancelAll();
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_);
      return false;
    case LoopAction::WaitForEvents:
      audio_.suspend(true, audioNow());
      sampleFixtureControls(false, audioNow());
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_);
      window_.waitEvents();
      input_ = input_mapper_.map(window_.input());
      sampleFixtureControls(false, audioNow());
      samplePlayerInput(input_);
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_);
      fixed_step_.reset();
      return !window_.shouldClose();
    case LoopAction::Render: {
      const bool controls_active = samplePlayerInput(input_);
      const double now = audioNow();
      audio_.suspend(audio_fixture_ && audio_fixture_->paused(), now);
      sampleFixtureControls(controls_active, now);
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
      audio_.listener({view.position.x, view.position.y, view.position.z},
                      {view.direction.x, view.direction.y, view.direction.z});
      std::vector<float> accepted_angles;
      accepted_angles.reserve(level_.doors().size());
      for (std::size_t i = 0; i < level_.doors().size(); ++i)
        accepted_angles.push_back(doors_.state(i).angle);
      audio_.acceptedDoors(accepted_angles);
      if (audio_fixture_) audio_fixture_->update();
      frame.captions = audio_.captions();
      if (audio_.playback().warning() != audio_warning_) {
        audio_warning_ = audio_.playback().warning();
        if (!audio_warning_.empty()) std::cerr << audio_warning_ << '\n';
      }
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

void Engine::sampleFixtureControls(bool active, double now) {
  if (!audio_fixture_) return;
  const auto& physical = window_.input();
  audio_fixture_->controls(physical.isKeyDown(PhysicalKey::F5),
                           physical.isKeyDown(PhysicalKey::M),
                           physical.isKeyDown(PhysicalKey::P), active, now);
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
