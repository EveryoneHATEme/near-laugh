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
                           const LevelAudio& audio, const CaptionFont& font,
                           const LevelCharacters& characters,
                           const LevelHousehold& household) {
  validateHouseholdText(household, font);
  auto content = prepareAudioContent(root, audio);
  validateAudioCaptions(content, audio, font);
  validateCharacterAudio(content, audio, characters);
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
               AudioOutput output, FrameTimings* timings,
               bool character_fixture, FrameCapture* capture)
    : platform_(),
      window_(platform_, config.window_width, config.window_height,
              config.window_title),
      resources_(
          resolveRuntimeResources(config.resource_root, config.level_path)),
      level_(loadPrototypeLevel(resources_.prototype_level)),
      entry_(selectedEntry(level_, config, resources_.prototype_level)),
      caption_font_(std::make_shared<CaptionFont>(resources_.root)),
      character_assets_(
          prepareCharacterAssets(resources_.root, level_.characters())),
      audio_(level_.audio(), level_.doors(),
             preparedAudio(resources_.root, level_.audio(), *caption_font_,
                           level_.characters(), level_.household()),
             level_.audio().sources.empty() ? AudioOutput::Silent : output,
             audioNow()),
      physics_(level_, entry_),
      player_(physics_, entry_.pose.yaw_degrees),
      light_switch_(level_.environmentLight(), level_.lightSwitches()),
      doors_(level_.doors()),
      characters_(level_.characters(), character_assets_, physics_, audio_),
      household_(level_, physics_, audio_),
      renderer_(
          window_, window_.framebufferExtent(), level_,
          {std::move(resources_.scene_vertex_shader),
           std::move(resources_.scene_fragment_shader), resources_.root,
           caption_font_, timings, capture, characters_.renderInstances()},
          diagnostics),
      character_fixture_(character_fixture) {
  window_.setCursorCaptured(true);
  fixed_step_.reset();
  audio_.update(audioNow());
  if (audio_fixture) {
    // The explicit P04 sequence cannot take sources reserved by other owners.
    for (const auto id : {"radio", "phone-ring", "footsteps",
                          "phone-conversation", "invitation"})
      if (characters_.ownsSource(id) || household_.ownsSource(id))
        throw std::runtime_error("Audio fixture cannot start owned source '" +
                                 std::string(id) + "'");
    audio_fixture_.emplace(audio_);
    audio_fixture_->restart();
  } else
    audio_.autoplay();
  characters_.handoffAudio();
}

void Engine::run() {
  while (tick()) {
  }
}

bool Engine::tick(const PlayerActionSnapshot* development_input,
                  const CharacterDevelopmentInput* character_input,
                  const HouseholdDevelopmentInput* household_input) {
  window_.pollEvents();
  input_ = input_mapper_.map(window_.input());
  if (development_input) input_ = *development_input;
  const FramebufferExtent framebuffer = window_.framebufferExtent();
  const LoopDecision decision = decideLoopAction(
      window_.shouldClose(), framebuffer, window_.consumeFramebufferResize());
  switch (decision.action) {
    case LoopAction::Stop:
      household_.stop();
      for (const auto& actor : level_.characters().actors)
        (void)characters_.cancel(actor.id);
      audio_.cancelAll();
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_, &household_);
      return false;
    case LoopAction::WaitForEvents:
      suspendWorld(true, audioNow());
      sampleFixtureControls(false, audioNow(), character_input,
                            household_input);
      (void)samplePlayerInput(input_);
      (void)interaction_.update(exploration_input_, false, {}, level_, physics_,
                                doors_, light_switch_, &household_);
      window_.waitEvents();
      input_ = input_mapper_.map(window_.input());
      sampleFixtureControls(false, audioNow(), character_input,
                            household_input);
      samplePlayerInput(input_);
      (void)interaction_.update(exploration_input_, false, {}, level_, physics_,
                                doors_, light_switch_, &household_);
      fixed_step_.reset();
      return !window_.shouldClose();
    case LoopAction::Render: {
      const double now = audioNow();
      sampleFixtureControls(window_.cursorCaptured() && !input_.menu, now,
                            character_input, household_input);
      suspendWorld(character_paused_ || household_paused_ ||
                       (audio_fixture_ && audio_fixture_->paused()),
                   now);
      const bool controls_active = samplePlayerInput(input_);
      const FixedStepBatch simulation =
          suspended_ ? FixedStepBatch{0, 1.F}
                     : fixed_step_.sample(FixedStepAccumulator::Clock::now());
      for (int step = 0; step < simulation.complete_steps; ++step) {
        household_.beforeFixedStep(player_.viewPose(1.F));
        physics_.advanceWorld(
            static_cast<float>(FixedStepAccumulator::step_seconds));
        player_.fixedStep(
            static_cast<float>(FixedStepAccumulator::step_seconds));
        characters_.fixedStep(
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
      characters_.handoffAudio();
      if (audio_fixture_) audio_fixture_->update();
      if (audio_.playback().warning() != audio_warning_) {
        audio_warning_ = audio_.playback().warning();
        if (!audio_warning_.empty()) std::cerr << audio_warning_ << '\n';
      }
      const bool was_reading = household_.readingDocument().has_value();
      (void)interaction_.update(exploration_input_, controls_active, view,
                                level_, physics_, doors_, light_switch_,
                                &household_);
      if (was_reading != household_.readingDocument().has_value())
        window_.resetLookInput();
      frame.captions = audio_.captions();
      frame.household_text = household_.text();
      frame.point_light_enabled = light_switch_.pointLightEnabled();
      frame.opaque_boxes = household_.presentation(doors_.presentation());
      frame.characters = characters_.presentation();
      const FrameOutcome outcome = renderer_.renderFrame(frame);
      return runtimeContinuesAfter(outcome) && !window_.shouldClose();
    }
  }
  return false;
}

void Engine::suspendWorld(bool suspended, double now) {
  audio_.suspend(suspended, now);
  if (suspended) household_.suspend();
  if (suspended_ != suspended || suspended) fixed_step_.reset();
  suspended_ = suspended;
}

void Engine::sampleFixtureControls(
    bool active, double now, const CharacterDevelopmentInput* injected,
    const HouseholdDevelopmentInput* household_input) {
  const auto& physical = window_.input();
  if (audio_fixture_)
    audio_fixture_->controls(physical.isKeyDown(PhysicalKey::F5),
                             physical.isKeyDown(PhysicalKey::M),
                             physical.isKeyDown(PhysicalKey::P), active, now);
  if (!character_fixture_ && !audio_fixture_ &&
      (!level_.household().boxes.empty() ||
       !level_.household().documents.empty() ||
       !level_.household().radios.empty())) {
    const HouseholdDevelopmentInput controls =
        household_input
            ? *household_input
            : HouseholdDevelopmentInput{physical.isKeyDown(PhysicalKey::P),
                                        physical.isKeyDown(PhysicalKey::M)};
    if (active) {
      if (controls.pause && !previous_household_input_.pause)
        household_paused_ = !household_paused_;
      if (controls.mute && !previous_household_input_.mute)
        audio_.mute(!audio_.muted());
    }
    previous_household_input_ = controls;
  }
  if (!character_fixture_) return;
  const CharacterDevelopmentInput input =
      injected ? *injected
               : CharacterDevelopmentInput{physical.isKeyDown(PhysicalKey::F5),
                                           physical.isKeyDown(PhysicalKey::F6),
                                           physical.isKeyDown(PhysicalKey::P),
                                           physical.isKeyDown(PhysicalKey::M)};
  if (active) {
    if (input.pause && !previous_character_input_.pause)
      character_paused_ = !character_paused_;
    if (input.mute && !previous_character_input_.mute)
      audio_.mute(!audio_.muted());
    if ((input.cancel && !previous_character_input_.cancel) ||
        (input.restart && !previous_character_input_.restart)) {
      for (const auto& actor : level_.characters().actors)
        (void)characters_.cancel(actor.id);
      if (input.restart && !previous_character_input_.restart)
        for (const auto& actor : level_.characters().actors)
          if (actor.initial_route)
            (void)characters_.start(actor.id, *actor.initial_route);
    }
  }
  previous_character_input_ = input;
}

bool Engine::samplePlayerInput(const PlayerActionSnapshot& input) {
  const bool was_captured = window_.cursorCaptured();
  auto cursor_input = input;
  if (household_.consumesEscape()) cursor_input.menu = false;
  const PlayerCursorCaptureTransition transition =
      playerCursorTransition(was_captured, cursor_input);
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
      !suspended_ && playerControlsActive(was_captured, transition);
  const bool was_reading = household_.readingDocument().has_value();
  exploration_input_ =
      household_.sampleInput(input, controls_active, suspended_);
  if (was_reading != household_.readingDocument().has_value())
    window_.resetLookInput();
  const bool exploration_active = household_.worldActionsAllowed();
  player_.sampleInput(exploration_input_, exploration_active,
                      household_.preservesStance());
  flashlight_.samplePrimaryAction(exploration_input_.primary_action,
                                  exploration_active);
  return exploration_active;
}
