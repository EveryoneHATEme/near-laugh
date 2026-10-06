#include "core/engine.hpp"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "core/render/validation_diagnostics.hpp"
#include "core/development/frame_timings.hpp"

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
      timings_(timings),
      character_fixture_(character_fixture) {
  window_.setCursorCaptured(true);
  fixed_step_.reset();
  tick_time_ = audioNow();
  audio_.update(tick_time_);
  if (audio_fixture) {
    // The explicit P04 sequence cannot take sources reserved by other owners.
    for (const auto id : {"radio", "phone-ring", "footsteps",
                          "phone-conversation", "invitation"})
      if (characters_.ownsSource(id) || household_.ownsSource(id) ||
          std::any_of(level_.narrative().events.begin(), level_.narrative().events.end(),
              [&](const auto& event) {
                return std::any_of(event.steps.begin(), event.steps.end(), [&](const auto& step) {
                  const auto* cue = std::get_if<NarrativePlayCueStep>(&step);
                  return cue && cue->source == id;
                });
              }))
        throw std::runtime_error("Audio fixture cannot start owned source '" +
                                 std::string(id) + "'");
    audio_fixture_.emplace(audio_);
    audio_fixture_->restart();
  } else
    audio_.autoplay();
  characters_.handoffAudio();
  narrative_.emplace(level_.narrative(), narrativeObservation());
}

Engine::~Engine() {
  if (narrative_) cancelNarrative(narrative_->close());
  narrative_.reset();
}

NarrativeObservation Engine::narrativeObservation() const {
  NarrativeObservation result;
  result.active_time = audio_.activeTime();
  const auto feet = physics_.characterState().foot_position;
  result.feet = {feet.x, feet.y, feet.z};
  const auto& lights = level_.environmentLight().point_lights;
  for (std::size_t i = 0; i < lights.size(); ++i)
    result.lights.push_back({lights[i].id, light_switch_.pointLightEnabled()[i] != 0});
  for (std::size_t i = 0; i < level_.doors().size(); ++i) {
    const auto& definition = level_.doors()[i];
    const auto& state = doors_.state(i);
    result.doors.push_back({definition.id,
        !state.moving && state.angle == definition.open_angle_degrees,
        !state.moving && state.angle == 0, state.locked});
  }
  for (std::size_t i = 0; i < level_.household().radios.size(); ++i)
    result.radios.push_back({level_.household().radios[i].id, household_.radioOn(i)});
  for (std::size_t i = 0; i < level_.household().boxes.size(); ++i)
    result.boxes.push_back({level_.household().boxes[i].id, household_.heldBox() == i});
  for (std::size_t i = 0; i < level_.household().documents.size(); ++i)
    result.documents.push_back({level_.household().documents[i].id, household_.readingDocument() == i});
  for (std::size_t i = 0; i < level_.characters().actors.size(); ++i) {
    const auto& actor = characters_.result(i);
    NarrativeActorState state{};
    switch (actor.action) {
      case CharacterAction::Idle: state = NarrativeActorState::Idle; break;
      case CharacterAction::Turning: state = NarrativeActorState::Turning; break;
      case CharacterAction::Walking: state = NarrativeActorState::Walking; break;
      case CharacterAction::Blocked: state = NarrativeActorState::Blocked; break;
      case CharacterAction::Interacting: state = NarrativeActorState::Interacting; break;
      case CharacterAction::Completed: state = NarrativeActorState::Completed; break;
      case CharacterAction::Canceled: state = NarrativeActorState::Canceled; break;
    }
    result.actors.push_back({std::string(actor.actor), actor.instance, state});
  }
  for (const auto& source : level_.audio().sources)
    result.cues.push_back(
        {source.id, audio_.instance(source.id), audio_.status(source.id)});
  return result;
}

void Engine::cancelNarrative(std::span<const NarrativeOwnedAction> actions) {
  for (const auto& action : actions)
    if (action.kind == NarrativeOwnedKind::Cue)
      (void)audio_.cancel(action.target, action.instance);
    else
      (void)characters_.cancel(action.target, action.instance);
}

NarrativeCommandResult Engine::dispatchNarrative(const NarrativeStep& step) {
  const auto index = [](const auto& values, const auto& id) {
    return static_cast<std::size_t>(std::find_if(values.begin(), values.end(),
        [&](const auto& value) { return value.id == id; }) - values.begin());
  };
  const auto state = [](bool accepted) -> NarrativeCommandResult {
    return {accepted ? NarrativeCommandStatus::Accepted : NarrativeCommandStatus::Refused,
            0, accepted ? "" : "invalid_target"};
  };
  const auto door = [](DoorRequestResult result) -> NarrativeCommandResult {
    switch (result) {
      case DoorRequestResult::Accepted: return {NarrativeCommandStatus::Accepted, 0, {}};
      case DoorRequestResult::InvalidTarget: return {NarrativeCommandStatus::Refused, 0, "invalid_door"};
      case DoorRequestResult::Locked: return {NarrativeCommandStatus::Refused, 0, "door_locked"};
      case DoorRequestResult::NotLockable: return {NarrativeCommandStatus::Refused, 0, "door_not_lockable"};
      case DoorRequestResult::NotClosed: return {NarrativeCommandStatus::Refused, 0, "door_not_closed"};
    }
    return {};
  };
  return std::visit([&](const auto& action) -> NarrativeCommandResult {
    using T = std::decay_t<decltype(action)>;
    if constexpr (std::is_same_v<T, NarrativeSetLightStep>)
      return state(light_switch_.setEnabled(index(level_.environmentLight().point_lights, action.light), action.enabled));
    else if constexpr (std::is_same_v<T, NarrativeSetRadioStep>)
      return state(household_.setRadioEnabled(index(level_.household().radios, action.radio), action.enabled));
    else if constexpr (std::is_same_v<T, NarrativeSetDoorOpenStep>)
      return door(doors_.requestOpen(index(level_.doors(), action.door), action.open));
    else if constexpr (std::is_same_v<T, NarrativeSetDoorLockedStep>)
      return door(doors_.requestLocked(index(level_.doors(), action.door), action.locked));
    else if constexpr (std::is_same_v<T, NarrativePlayCueStep>) {
      const auto started = audio_.start(action.source);
      return {started == CueStart::Started ? NarrativeCommandStatus::Started : NarrativeCommandStatus::Busy,
              started == CueStart::Started ? audio_.instance(action.source) : 0,
              started == CueStart::AlreadyActive ? "source_owned" : "foreground_busy"};
    } else if constexpr (std::is_same_v<T, NarrativeRunRouteStep>) {
      const auto started = characters_.start(action.actor, action.route);
      const auto actor = index(level_.characters().actors, action.actor);
      return {started == CharacterStart::Started ? NarrativeCommandStatus::Started : NarrativeCommandStatus::Busy,
              started == CharacterStart::Started ? characters_.result(actor).instance : 0, "actor_owned"};
    } else return {NarrativeCommandStatus::Refused, 0, "not_an_external_command"};
  }, step);
}

void Engine::run() {
  while (tick()) {
  }
}

bool Engine::tick(const PlayerActionSnapshot* development_input,
                  const CharacterDevelopmentInput* character_input,
                  const HouseholdDevelopmentInput* household_input,
                  const NarrativeDevelopmentInput* narrative_input) {
  double now;
  if (narrative_input) {
    if (tick_clock_ == TickClock::Wall)
      throw std::logic_error(
          "Cannot mix wall and development clocks in one Engine");
    if (!std::isfinite(narrative_input->elapsed_seconds) ||
        narrative_input->elapsed_seconds < 0)
      throw std::invalid_argument(
          "Development elapsed time must be finite and nonnegative");
    now = tick_time_ + narrative_input->elapsed_seconds;
    if (!std::isfinite(now))
      throw std::invalid_argument("Development clock overflow");
    tick_clock_ = TickClock::Injected;
  } else {
    if (tick_clock_ == TickClock::Injected)
      throw std::logic_error(
          "Cannot mix development and wall clocks in one Engine");
    now = audioNow();
    tick_clock_ = TickClock::Wall;
  }
  tick_time_ = now;
  window_.pollEvents();
  input_ = input_mapper_.map(window_.input());
  if (development_input) input_ = *development_input;
  const FramebufferExtent framebuffer = window_.framebufferExtent();
  const LoopDecision decision = decideLoopAction(
      window_.shouldClose(), framebuffer, window_.consumeFramebufferResize());
  switch (decision.action) {
    case LoopAction::Stop:
      if (narrative_) cancelNarrative(narrative_->close());
      household_.stop();
      for (const auto& actor : level_.characters().actors)
        (void)characters_.cancel(actor.id);
      audio_.cancelAll();
      (void)interaction_.update(input_, false, {}, level_, physics_, doors_,
                                light_switch_, &household_);
      return false;
    case LoopAction::WaitForEvents:
      suspendWorld(true, now);
      sampleFixtureControls(false, now, character_input, household_input);
      (void)samplePlayerInput(input_);
      (void)interaction_.update(exploration_input_, false, {}, level_, physics_,
                                doors_, light_switch_, &household_);
      window_.waitEvents();
      input_ = input_mapper_.map(window_.input());
      sampleFixtureControls(false, now, character_input, household_input);
      samplePlayerInput(input_);
      (void)interaction_.update(exploration_input_, false, {}, level_, physics_,
                                doors_, light_switch_, &household_);
      fixed_step_.reset();
      return !window_.shouldClose();
    case LoopAction::Render: {
      auto* timing = timings_ ? &timings_->current() : nullptr;
      const auto stamp = [&] {
        return timing ? FrameTimings::Clock::now()
                      : FrameTimings::Clock::time_point{};
      };
      const auto record_world = [&](FrameTimings::Clock::time_point start) {
        if (timing)
          timing->world_player_doors_ms +=
              std::chrono::duration<double, std::milli>(
                  FrameTimings::Clock::now() - start)
                  .count();
      };
      const bool previously_suspended = suspended_;
      // Levels without events skip per-frame narrative observation entirely.
      const bool narrative_events = !level_.narrative().events.empty();
      sampleFixtureControls(window_.cursorCaptured() && !input_.menu, now,
                            character_input, household_input);
      suspendWorld(character_paused_ || household_paused_ ||
                       (audio_fixture_ && audio_fixture_->paused()),
                   now);
      const bool controls_active = samplePlayerInput(input_);
      const FixedStepBatch simulation =
          suspended_ ? FixedStepBatch{0, 1.F}
          : narrative_input
              ? fixed_step_.advance(
                    previously_suspended ? 0 : narrative_input->elapsed_seconds)
              : fixed_step_.sample(FixedStepAccumulator::Clock::now());
      for (int step = 0; step < simulation.complete_steps; ++step) {
        const auto world_start = stamp();
        household_.beforeFixedStep(player_.viewPose(1.F),
                                   &accepted_interactions_);
        physics_.advanceWorld(
            static_cast<float>(FixedStepAccumulator::step_seconds));
        player_.fixedStep(
            static_cast<float>(FixedStepAccumulator::step_seconds));
        record_world(world_start);
        characters_.fixedStep(
            static_cast<float>(FixedStepAccumulator::step_seconds), timing);
        const auto doors_start = stamp();
        doors_.fixedStep(static_cast<float>(FixedStepAccumulator::step_seconds),
                         physics_);
        record_world(doors_start);
        if (narrative_events) {
          const auto feet = physics_.characterState().foot_position;
          narrative_->samplePosition({feet.x, feet.y, feet.z});
        }
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
      if (audio_.playback().warning() != audio_warning_) {
        audio_warning_ = audio_.playback().warning();
        if (!audio_warning_.empty()) std::cerr << audio_warning_ << '\n';
      }
      const bool was_reading = household_.readingDocument().has_value();
      (void)interaction_.update(exploration_input_, controls_active, view,
                                level_, physics_, doors_, light_switch_,
                                &household_, &accepted_interactions_);
      if (was_reading != household_.readingDocument().has_value())
        window_.resetLookInput();
      if (!suspended_) {
        if (narrative_events)
          cancelNarrative(narrative_->beginBoundary(
              narrativeObservation(), accepted_interactions_.pending()));
        accepted_interactions_.consume();
        const auto audio_start = stamp();
        characters_.handoffAudio();
        if (timing)
          timing->character_audio_ms +=
              std::chrono::duration<double, std::milli>(
                  FrameTimings::Clock::now() - audio_start)
                  .count();
        if (audio_fixture_) audio_fixture_->update();
        while (const auto command = narrative_->nextCommand())
          narrative_->resolve(dispatchNarrative(command->action));
      }
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
       !level_.household().radios.empty() || !level_.narrative().events.empty())) {
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
