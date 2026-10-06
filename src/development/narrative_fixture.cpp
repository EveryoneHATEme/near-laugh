#include "development/narrative_fixture.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

#include "core/engine.hpp"
#include "core/render/scene_assets.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/world/household.hpp"
#include "core/world/narrative.hpp"

namespace {
using Json = nlohmann::ordered_json;
std::string_view statusName(NarrativeRunStatus value) {
  switch (value) {
    case NarrativeRunStatus::Dormant:
      return "dormant";
    case NarrativeRunStatus::Running:
      return "running";
    case NarrativeRunStatus::Completed:
      return "completed";
    case NarrativeRunStatus::Canceled:
      return "canceled";
    case NarrativeRunStatus::Failed:
      return "failed";
  }
  return "invalid";
}
Json runJson(const NarrativeRunSnapshot& run) {
  Json result{{"event", run.event},   {"run", run.run},
              {"step", run.step},     {"status", statusName(run.status)},
              {"reason", run.reason}, {"target", run.target},
              {"owned", nullptr}};
  if (run.owned)
    result["owned"] = {
        {"kind", run.owned->kind == NarrativeOwnedKind::Cue ? "cue" : "route"},
        {"target", run.owned->target},
        {"instance", run.owned->instance}};
  return result;
}
struct Report {
  std::filesystem::path path;
  Json value{{"schema", 1},
             {"passed", false},
             {"mode", "not_started"},
             {"failed_step", nullptr},
             {"checks", Json::array()},
             {"snapshots", Json::array()},
             {"visual_acceptance", "not_run"},
             {"human_listening", "not_run"}};
  explicit Report(const std::filesystem::path& directory)
      : path(directory / "report.json") {
    std::filesystem::create_directories(directory);
    if (std::filesystem::exists(path))
      throw std::runtime_error("Choose a fresh narrative report directory: " +
                               directory.string());
    save();
  }
  void save() const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << value.dump(2) << '\n';
    if (!output)
      throw std::runtime_error("Cannot write narrative report: " +
                               path.string());
  }
  void check(bool passed, std::string_view name) {
    value["checks"].push_back({{"name", name}, {"passed", passed}});
    if (!passed) {
      value["failed_step"] = name;
      save();
      throw std::runtime_error("Narrative check failed: " + std::string(name));
    }
  }
  void failed(std::string_view detail) {
    value["passed"] = false;
    value["failure"] = detail;
    if (value["failed_step"].is_null()) value["failed_step"] = value["phase"];
    save();
  }
};
struct FailureControl {
  const char* name;
  std::optional<std::string> previous;
  FailureControl(const char* variable, const char* value) : name(variable) {
#ifdef _WIN32
    char* stored = nullptr;
    std::size_t length{};
    if (_dupenv_s(&stored, &length, name) != 0)
      throw std::runtime_error("Cannot read narrative failure control");
    if (stored) previous = stored;
    std::free(stored);
#else
    if (const auto* stored = std::getenv(name)) previous = stored;
#endif
    if (!set(value))
      throw std::runtime_error("Cannot set narrative failure control");
  }
  bool set(const char* value) const {
#ifdef _WIN32
    return _putenv_s(name, value) == 0;
#else
    return (*value ? setenv(name, value, 1) : unsetenv(name)) == 0;
#endif
  }
  ~FailureControl() { (void)set(previous ? previous->c_str() : ""); }
};
LevelDocument load(const std::filesystem::path& resources,
                   std::string_view name) {
  const auto path = resources / "levels" / (std::string(name) + ".level.json");
  auto loaded = loadLevelDocument(path);
  if (!loaded)
    throw std::runtime_error(formatLevelDiagnostics(loaded.diagnostics));
  const auto errors = validateLevelDocument(*loaded.document, path);
  if (!errors.empty()) throw std::runtime_error(formatLevelDiagnostics(errors));
  return std::move(*loaded.document);
}
void preflight(const std::filesystem::path& resources, Report& report) {
  const CaptionFont font(resources);
  for (const auto* name :
       {"narrative-t4", "narrative-t4-door", "narrative-t4-refusal",
        "narrative-t4-interactions", "narrative-t4-busy"}) {
    report.value["phase"] = std::string("preflight/") + name;
    const auto document = load(resources, name);
    const auto assets = prepareSceneAssets(resources, document);
    const auto characters =
        prepareCharacterAssets(resources, document.characters);
    validateHouseholdText(document.household, font);
    const auto audio = prepareAudioContent(resources, document.audio);
    validateAudioCaptions(audio, document.audio, font);
    validateCharacterAudio(audio, document.audio, document.characters);
    report.check(!document.narrative.events.empty() && !characters.empty() &&
                     assets.obstacle_material.has_value(),
                 name);
  }
}
}  // namespace

struct EngineNarrativeFixture {
  struct FailureEvidence {
    Report& report;
    const Engine& engine;
    int exceptions{std::uncaught_exceptions()};
    ~FailureEvidence() noexcept {
      if (std::uncaught_exceptions() > exceptions) {
        try {
          snapshot(report, engine, "failed_scenario_state");
        } catch (...) {
        }  // Preserve the original test failure if writing fails.
      }
    }
  };
  static void tick(Engine& engine, double elapsed = .1,
                   const PlayerActionSnapshot& input = {},
                   const HouseholdDevelopmentInput& controls = {}) {
    const NarrativeDevelopmentInput time{elapsed};
    if (!engine.tick(&input, nullptr, &controls, &time))
      throw std::runtime_error(
          "Narrative Engine stopped before the scenario finished");
  }
  static NarrativeRunSnapshot run(const Engine& engine,
                                  std::string_view id = "neutral-sequence") {
    const auto values = engine.narrative_->runs();
    const auto it = std::find_if(values.begin(), values.end(),
                                 [&](const auto& v) { return v.event == id; });
    if (it == values.end())
      throw std::runtime_error("Fixture event missing: " + std::string(id));
    return *it;
  }
  static void snapshot(Report& report, const Engine& engine,
                       std::string_view name) {
    Json values = Json::array(), trace = Json::array();
    for (const auto& value : engine.narrative_->runs())
      values.push_back(runJson(value));
    for (const auto& transition : engine.narrative_->trace())
      trace.push_back({{"active_time", transition.active_time},
                       {"event", runJson(transition.event)}});
    report.value["snapshots"].push_back(
        {{"name", name},
         {"facts", engine.narrative_->facts()},
         {"runs", values},
         {"trace", trace},
         {"dropped_records", engine.narrative_->droppedTraceRecords()},
         {"active_time", engine.audio_.activeTime()},
         {"suspended", engine.suspended_},
         {"muted", engine.audio_.muted()},
         {"silent", engine.audio_.playback().silent()},
         {"reading", engine.household_.readingDocument().has_value()},
         {"page", engine.household_.page()},
         {"foreground_caption", engine.audio_.captions().foreground.text}});
    report.save();
  }
  static void enter(Engine& engine, Report& report) {
    tick(engine, 0);
    PlayerActionSnapshot forward;
    forward.move_forward = true;
    for (int i = 0; i < 20 && run(engine).run == 0; ++i)
      tick(engine, .1, forward);
    tick(engine, 0);
    report.check(run(engine).run == 1,
                 "accepted player movement enters the authored region once");
  }
  template <class Predicate>
  static void until(Engine& engine, Predicate predicate, int maximum = 240) {
    for (int i = 0; i < maximum; ++i) {
      if (predicate()) return;
      tick(engine);
    }
    if (!predicate())
      throw std::runtime_error(
          "Bounded narrative wait did not reach its expected state");
  }
  static near_laugh::RuntimeConfig config(
      const std::filesystem::path& resources, std::string_view name) {
    near_laugh::RuntimeConfig result;
    result.resource_root = std::filesystem::absolute(resources);
    result.level_path =
        result.resource_root / "levels" / (std::string(name) + ".level.json");
    result.window_title = "T4 development checks";
    result.window_width = 800;
    result.window_height = 600;
    return result;
  }
  static void cancelRadio(Engine& engine) {
    engine.household_.toggleRadio(0, &engine.accepted_interactions_);
    tick(engine, 0);
  }
  static void normal(const std::filesystem::path& resources, Report& report,
                     ValidationDiagnostics& diagnostics, bool muted) {
    report.value["phase"] = muted ? "normal_muted" : "normal_silent";
    Engine engine(config(resources, "narrative-t4"), diagnostics, false,
                  AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    report.check(engine.renderer_.validationEnabled(),
                 "Vulkan validation is enabled");
    const auto initial_active = engine.audio_.activeTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    tick(engine, 0);
    report.check(
        engine.audio_.activeTime() == initial_active,
        "first injected zero sample excludes constructor-to-tick wall time");
    if (muted) {
      tick(engine, 0, {}, {.mute = true});
      tick(engine, 0);
      report.check(engine.audio_.muted(), "shared development mute applies");
    }
    enter(engine, report);
    report.check(
        engine.light_switch_.pointLightEnabled()[0] != 0 &&
            engine.audio_.status("sequence-source") == CueStatus::Playing,
        "region starts light and one logical cue");
    const auto cue = engine.audio_.instance("sequence-source");
    const auto position = engine.physics_.actorState(0).feet_position;
    tick(engine, 0);
    report.check(engine.audio_.instance("sequence-source") == cue &&
                     engine.physics_.actorState(0).feet_position == position,
                 "zero-step boundary retains cue identity and physical pose");
    engine.household_.openDocument(0, &engine.accepted_interactions_);
    tick(engine, 0);
    tick(engine, 0, {}, {.pause = true});
    const auto paused = engine.audio_.activeTime();
    const auto offset = engine.audio_.offset("sequence-source");
    tick(engine, 20);
    report.check(engine.suspended_ && engine.audio_.activeTime() == paused &&
                     engine.audio_.offset("sequence-source") == offset &&
                     engine.household_.readingDocument() == 0,
                 "shared suspension freezes cue and narrative while retaining "
                 "the reader");
    tick(engine, 0, {}, {.pause = true});
    tick(engine, 0);
    until(engine, [&] { return run(engine).reason == "delay"; });
    report.check(
        engine.household_.readingDocument() == 0 &&
            engine.audio_.status("sequence-source") == CueStatus::Completed,
        "reading preserves logical completion and sequence progression");
    tick(engine, 5);
    report.check(run(engine).owned &&
                     run(engine).owned->kind == NarrativeOwnedKind::Route &&
                     engine.characters_.result(0).walked_distance == 0,
                 "expired delay starts route now without backdated travel");
    const auto actor = engine.characters_.result(0).instance;
    const auto before = engine.characters_.result(0).walked_distance;
    tick(engine, 20);
    report.check(engine.characters_.result(0).walked_distance - before <= .076,
                 "long active stall retains six-step movement cap");
    const auto trace_size = engine.narrative_->trace().size();
    report.check(
        engine.renderer_.renderFrame({}) == FrameOutcome::Skipped,
        "zero-extent presentation skips without advancing progression");
    engine.renderer_.requestSwapchainRecreation();
    tick(engine, 0);
    report.check(engine.characters_.result(0).instance == actor &&
                     engine.audio_.instance("sequence-source") == cue &&
                     engine.narrative_->trace().size() == trace_size,
                 "swapchain recovery preserves action and event identities");
    until(engine,
          [&] { return run(engine).status == NarrativeRunStatus::Completed; });
    report.check(
        engine.narrative_->facts().at("completed") &&
            engine.characters_.result(0).action == CharacterAction::Completed,
        "ordinary authored sequence completes its own route and fact");
    snapshot(report, engine, muted ? "normal_muted" : "normal_silent");
  }
  static void cancellation(const std::filesystem::path& resources,
                           Report& report, ValidationDiagnostics& diagnostics,
                           std::string_view stage) {
    report.value["phase"] = "cancel_" + std::string(stage);
    Engine engine(config(resources, "narrative-t4"), diagnostics, false,
                  AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    tick(engine, 0);
    if (stage == "before_entry") {
      cancelRadio(engine);
      PlayerActionSnapshot forward;
      forward.move_forward = true;
      for (int i = 0; i < 8; ++i) tick(engine, .1, forward);
      report.check(
          run(engine).run == 0 && !engine.narrative_->facts().at("completed"),
          "true cancellation consumes region trigger without starting a run");
    } else {
      enter(engine, report);
      if (stage == "delay")
        until(engine, [&] { return run(engine).reason == "delay"; });
      if (stage == "route")
        until(engine, [&] {
          return run(engine).owned &&
                 run(engine).owned->kind == NarrativeOwnedKind::Route;
        });
      const auto owned = run(engine).owned;
      cancelRadio(engine);
      report.check(
          run(engine).status == NarrativeRunStatus::Canceled &&
              !engine.narrative_->facts().at("completed") &&
              engine.light_switch_.pointLightEnabled()[0] != 0,
          "cancellation stops future work and retains accepted light state");
      if (owned && owned->kind == NarrativeOwnedKind::Cue)
        report.check(
            engine.audio_.status(owned->target) == CueStatus::Cancelled,
            "cancellation stops the matching cue and caption");
      if (owned && owned->kind == NarrativeOwnedKind::Route)
        report.check(
            engine.characters_.result(0).action == CharacterAction::Canceled,
            "cancellation stops the matching route");
      tick(engine, 10);
      report.check(
          run(engine).run == 1 && !engine.narrative_->facts().at("completed"),
          "canceled once event never replays later steps");
    }
    snapshot(report, engine, std::string("cancel_") + std::string(stage));
  }
  static void shutdown(const std::filesystem::path& resources, Report& report,
                       ValidationDiagnostics& diagnostics, bool route) {
    report.value["phase"] = route ? "shutdown_route" : "shutdown_cue";
    Engine engine(config(resources, "narrative-t4"), diagnostics, false,
                  AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    enter(engine, report);
    if (route)
      until(engine, [&] {
        return run(engine).owned &&
               run(engine).owned->kind == NarrativeOwnedKind::Route;
      });
    tick(engine, 0, {}, {.pause = true});
    engine.window_.requestClose();
    const NarrativeDevelopmentInput time{10};
    const PlayerActionSnapshot input{};
    report.check(
        !engine.tick(&input, nullptr, nullptr, &time) &&
            run(engine).status == NarrativeRunStatus::Canceled,
        "close while suspended closes dispatch and cancels owned work");
    report.check(
        route ? engine.characters_.result(0).action == CharacterAction::Canceled
              : engine.audio_.status("sequence-source") == CueStatus::Cancelled,
        "shutdown owner reports cancellation before destruction");
    snapshot(report, engine, route ? "shutdown_route" : "shutdown_cue");
  }
  static void variation(const std::filesystem::path& resources, Report& report,
                        ValidationDiagnostics& diagnostics,
                        std::string_view name) {
    report.value["phase"] = name;
    Engine engine(config(resources, name), diagnostics, false,
                  AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    enter(engine, report);
    if (name == "narrative-t4-refusal") {
      report.check(run(engine).status == NarrativeRunStatus::Failed &&
                       run(engine).reason == "door_locked" &&
                       engine.audio_.instance("sequence-source") == 0,
                   "locked-door refusal prevents every later step");
    } else {
      if (name == "narrative-t4-busy")
        report.check(run(engine).reason == "source_owned" && !run(engine).owned,
                     "busy source remains foreign until a later new start");
      until(engine, [&] {
        return run(engine).status == NarrativeRunStatus::Completed;
      });
      report.check(engine.narrative_->facts().at("completed"),
                   "variation reaches authored completion");
      if (name == "narrative-t4-door")
        report.check(!engine.doors_.state(0).moving &&
                         engine.doors_.state(0).angle ==
                             engine.level_.doors()[0].open_angle_degrees,
                     "door wait advances only after stationary open endpoint");
    }
    snapshot(report, engine, name);
  }
  static void marker(const std::filesystem::path& resources, Report& report,
                     ValidationDiagnostics& diagnostics) {
    report.value["phase"] = "marker_cancellation";
    auto document = load(resources, "narrative-t4");
    document.audio.sources.push_back(
        {"actor-touch", "sequence-cue", {3, 1, -4}});
    document.characters.actors[0].interaction_source = "actor-touch";
    document.characters.routes[0].marks = {
        document.characters.actors[0].initial_mark};
    document.characters.routes[0].final_clip = "interact";
    document.narrative.events[0].steps = {
        NarrativeRunRouteStep{"walker", "walk"},
        NarrativeSetFactStep{"completed", true}};
    const auto path = report.path.parent_path() / "marker.level.json";
    const auto saved = saveLevelDocument(path, document);
    if (!saved)
      throw std::runtime_error(formatLevelDiagnostics(saved.diagnostics));
    auto options = config(resources, "narrative-t4");
    options.level_path = std::filesystem::absolute(path);
    Engine engine(options, diagnostics, false, AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    enter(engine, report);
    const auto threshold =
        characterClip(*engine.character_assets_[0], "interact").duration *
        test_mannequin_catalog.interaction_phase;
    for (int i = 0; i < 180; ++i) {
      if (engine.characters_.playback(0).clip() == "interact" &&
          engine.characters_.playback(0).time() + 1. / 60 >= threshold)
        break;
      tick(engine, 1. / 60);
    }
    report.check(engine.characters_.playback(0).clip() == "interact" &&
                     engine.audio_.instance("actor-touch") == 0,
                 "route reaches the boundary before its interaction marker");
    engine.household_.toggleRadio(0, &engine.accepted_interactions_);
    tick(engine, .1);
    report.check(
        run(engine).status == NarrativeRunStatus::Canceled &&
            engine.characters_.result(0).action == CharacterAction::Canceled &&
            engine.audio_.instance("actor-touch") == 0,
        "cancellation preflight suppresses same-batch pending character sound");
    snapshot(report, engine, "marker_cancellation");
  }
  static void minimize(const std::filesystem::path& resources, Report& report,
                       ValidationDiagnostics& diagnostics) {
    report.value["phase"] = "minimize_restore";
    Engine engine(config(resources, "narrative-t4"), diagnostics, false,
                  AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    enter(engine, report);
    engine.window_.minimize();
    engine.window_.pollEvents();
    report.check(engine.window_.framebufferExtent().isZero(),
                 "OS window minimizes to zero extent");
    {
      std::jthread wake([](std::stop_token stop) {
        while (!stop.stop_requested()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
          glfwPostEmptyEvent();
        }
      });
      tick(engine, 0);
    }
    const auto active = engine.audio_.activeTime();
    const auto cue = engine.audio_.instance("sequence-source");
    engine.window_.restore();
    engine.window_.pollEvents();
    tick(engine, 20);
    report.check(!engine.suspended_ && engine.audio_.activeTime() == active &&
                     engine.audio_.instance("sequence-source") == cue,
                 "restore excludes suspended time and preserves cue identity");
    snapshot(report, engine, "minimize_restore");
  }
  static void interactions(const std::filesystem::path& resources,
                           Report& report, ValidationDiagnostics& diagnostics) {
    report.value["phase"] = "accepted_interactions";
    Engine engine(config(resources, "narrative-t4-interactions"), diagnostics,
                  false, AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    enter(engine, report);
    auto selection = engine.player_.viewPose(1);
    const auto box = engine.physics_.boxState(0).center;
    const double length = std::hypot(double(box.x) - selection.position.x,
                                     double(box.y) - selection.position.y,
                                     double(box.z) - selection.position.z);
    selection.direction = {float((box.x - selection.position.x) / length),
                           float((box.y - selection.position.y) / length),
                           float((box.z - selection.position.z) / length)};
    report.check(engine.household_.requestPickup(0, selection),
                 "concrete pickup request queues successfully");
    tick(engine, 0);
    report.check(!engine.narrative_->facts().at("box-accepted"),
                 "queued zero-step pickup emits no accepted story action");
    tick(engine, 1. / 60);
    report.check(engine.household_.heldBox() == 0 &&
                     engine.narrative_->facts().at("box-accepted"),
                 "physical pickup publishes exactly one accepted trigger");
    const auto occurrence = run(engine, "box-accepted").run;
    engine.window_.setCursorCaptured(false);
    tick(engine, 0);
    report.check(
        engine.household_.safetyReleaseOwed(),
        "cursor release preserves zero-step safety release obligation");
    tick(engine, 1. / 60);
    report.check(!engine.household_.heldBox() &&
                     run(engine, "box-accepted").run == occurrence,
                 "safety release does not manufacture another player trigger");
    engine.window_.setCursorCaptured(true);
    tick(engine, 0);
    engine.household_.openDocument(0, &engine.accepted_interactions_);
    tick(engine, 0);
    report.check(
        engine.narrative_->facts().at("document-opened") &&
            engine.household_.readingDocument() == 0,
        "accepted document opening triggers at the zero-step boundary");
    PlayerActionSnapshot next;
    next.move_right = true;
    tick(engine, 0, next);
    const auto page = engine.household_.page();
    tick(engine, 0, next, {.pause = true});
    tick(engine, 10, next);
    report.check(engine.household_.page() == page,
                 "paused reader retains its page and consumes held navigation");
    tick(engine, 0, next, {.pause = true});
    tick(engine, 0, next);
    report.check(engine.household_.page() == page &&
                     run(engine, "document-opened").run == 1,
                 "resume does not replay held navigation or accepted opening");
    snapshot(report, engine, "accepted_interactions");
  }
  static void startupFailures(const std::filesystem::path& resources,
                              Report& report,
                              ValidationDiagnostics& diagnostics) {
    report.value["phase"] = "partial_startup";
    for (const bool gpu : {false, true}) {
      bool expected = false;
      {
        FailureControl failure(
            gpu ? "NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"
                : "NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE",
            gpu ? "world_mesh_upload" : "player-inner-body");
        try {
          Engine engine(config(resources, "narrative-t4"), diagnostics, false,
                        AudioOutput::Silent);
        } catch (const std::runtime_error& error) {
          expected =
              std::string_view(error.what())
                  .find(gpu ? "Forced world mesh upload failure"
                            : "player inner body") != std::string_view::npos;
          report.value["expected_startup_failures"].push_back(error.what());
        }
      }
      report.check(
          expected,
          gpu ? "renderer partial startup fails before narrative dispatch"
              : "physics partial startup fails before narrative dispatch");
      Engine recovered(config(resources, "narrative-t4"), diagnostics, false,
                       AudioOutput::Silent);
      FailureEvidence evidence{report, recovered};
      tick(recovered, 0);
      report.check(run(recovered).run == 0 &&
                       !recovered.narrative_->facts().at("completed"),
                   "fresh owner construction after failure has authored facts "
                   "and empty history");
    }
    Engine ordinary(config(resources, "narrative-t4"), diagnostics, false,
                    AudioOutput::Silent);
    (void)ordinary.tick();
    const auto active = ordinary.audio_.activeTime();
    bool rejected = false;
    try {
      tick(ordinary, 0);
    } catch (const std::logic_error&) {
      rejected = true;
    }
    report.check(rejected && ordinary.audio_.activeTime() == active,
                 "ordinary-to-injected clock switching is rejected before "
                 "state changes");
  }
  static void repetition(const std::filesystem::path& resources, Report& report,
                         ValidationDiagnostics& diagnostics) {
    report.value["phase"] = "fresh_region_rearm";
    auto document = load(resources, "narrative-t4");
    document.narrative.events[0].repeat = NarrativeRepeat::Rearm;
    document.narrative.events[0].steps = {
        NarrativeSetLightStep{"table-light", true}};
    const auto path = report.path.parent_path() / "rearm.level.json";
    const auto saved = saveLevelDocument(path, document);
    if (!saved)
      throw std::runtime_error(formatLevelDiagnostics(saved.diagnostics));
    auto options = config(resources, "narrative-t4");
    options.level_path = std::filesystem::absolute(path);
    Engine engine(options, diagnostics, false, AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    enter(engine, report);
    report.check(run(engine).status == NarrativeRunStatus::Completed,
                 "immediate rearming event finishes first run");
    for (int i = 0; i < 3; ++i) tick(engine, 0);
    report.check(run(engine).run == 1,
                 "occupancy alone does not repeat a region event");
    PlayerActionSnapshot backward;
    backward.move_backward = true;
    for (int i = 0; i < 8; ++i) tick(engine, .1, backward);
    PlayerActionSnapshot forward;
    forward.move_forward = true;
    for (int i = 0; i < 20 && run(engine).run == 1; ++i)
      tick(engine, .1, forward);
    report.check(run(engine).run == 2 &&
                     run(engine).status == NarrativeRunStatus::Completed,
                 "new accepted exit and entry rearms exactly one fresh run");
    bool rejected = false;
    try {
      (void)engine.tick();
    } catch (const std::logic_error&) {
      rejected = true;
    }
    report.check(rejected,
                 "injected and ordinary clock mixing is rejected explicitly");
    snapshot(report, engine, "fresh_region_rearm");
  }
  static void fixtureArbitration(const std::filesystem::path& resources,
                                 Report& report,
                                 ValidationDiagnostics& diagnostics) {
    report.value["phase"] = "p04_arbitration";
    auto document = load(resources, "narrative-t4");
    const auto legacy = load(resources, "audio-captions");
    document.narrative = {};
    document.audio.cues.insert(document.audio.cues.end(),
                               legacy.audio.cues.begin(),
                               legacy.audio.cues.end());
    document.audio.sources.insert(document.audio.sources.end(),
                                  legacy.audio.sources.begin(),
                                  legacy.audio.sources.end());
    document.audio.sources.push_back(
        {"actor-touch", "sequence-cue", {3, 1, -4}});
    document.characters.actors[0].interaction_source = "actor-touch";
    document.characters.actors[0].initial_route = "walk";
    document.characters.routes[0].marks = {
        document.characters.actors[0].initial_mark};
    document.characters.routes[0].final_clip = "interact";
    const auto path = report.path.parent_path() / "p04-arbitration.level.json";
    const auto saved = saveLevelDocument(path, document);
    if (!saved)
      throw std::runtime_error(formatLevelDiagnostics(saved.diagnostics));
    auto options = config(resources, "narrative-t4");
    options.level_path = std::filesystem::absolute(path);
    Engine engine(options, diagnostics, true, AudioOutput::Silent);
    FailureEvidence evidence{report, engine};
    bool refused = false;
    for (int i = 0; i < 600 && !refused; ++i) {
      try {
        tick(engine);
      } catch (const std::runtime_error& error) {
        if (std::string_view(error.what())
                .find("Audio fixture could not explicitly start 'footsteps'") ==
            std::string_view::npos)
          throw;
        refused = true;
      }
    }
    report.check(
        refused && engine.audio_.status("actor-touch") == CueStatus::Playing &&
            engine.audio_.instance("footsteps") == 0,
        "character foreground start retains priority over simultaneous P04 "
        "fixture advance");
    snapshot(report, engine, "p04_expected_foreground_refusal");
  }
};

void preflightNarrativeFixtures(const std::filesystem::path& resources,
                                const std::filesystem::path& directory) {
  Report report(directory);
  report.value["mode"] = "device_free_preflight";
  try {
    preflight(resources, report);
    report.value["passed"] = true;
    report.save();
  } catch (const std::exception& error) {
    report.failed(error.what());
    throw;
  }
}
void runNarrativeFixtureChecks(const std::filesystem::path& resources,
                               const std::filesystem::path& directory,
                               ValidationDiagnostics& diagnostics) {
  Report report(directory);
  report.value["mode"] = "production_engine_gpu_checks";
  try {
    preflight(resources, report);
    EngineNarrativeFixture::normal(resources, report, diagnostics, false);
    EngineNarrativeFixture::normal(resources, report, diagnostics, true);
    for (const auto* stage : {"before_entry", "cue", "delay", "route"})
      EngineNarrativeFixture::cancellation(resources, report, diagnostics,
                                           stage);
    for (const bool route : {false, true})
      EngineNarrativeFixture::shutdown(resources, report, diagnostics, route);
    for (const auto* name :
         {"narrative-t4-door", "narrative-t4-refusal", "narrative-t4-busy"})
      EngineNarrativeFixture::variation(resources, report, diagnostics, name);
    EngineNarrativeFixture::marker(resources, report, diagnostics);
    EngineNarrativeFixture::minimize(resources, report, diagnostics);
    EngineNarrativeFixture::interactions(resources, report, diagnostics);
    EngineNarrativeFixture::startupFailures(resources, report, diagnostics);
    EngineNarrativeFixture::repetition(resources, report, diagnostics);
    EngineNarrativeFixture::fixtureArbitration(resources, report, diagnostics);
    report.check(diagnostics.errorCount() == 0,
                 "zero unexpected Vulkan errors including destruction");
    report.value["vulkan_errors"] = diagnostics.errorCount();
    report.value["passed"] = true;
    report.save();
  } catch (const std::exception& error) {
    report.value["vulkan_errors"] = diagnostics.errorCount();
    report.failed(error.what());
    throw;
  }
}
void playNarrativeFixture(const std::filesystem::path& resources,
                          const std::filesystem::path& level,
                          const std::filesystem::path& directory,
                          ValidationDiagnostics& diagnostics, bool device) {
  Report report(directory);
  report.value["mode"] = "ordinary_wall_clock_play";
  report.value["phase"] = "play";
  try {
    auto options = EngineNarrativeFixture::config(resources, "narrative-t4");
    if (!level.empty()) options.level_path = std::filesystem::absolute(level);
    {
      Engine engine(options, diagnostics, false,
                    device ? AudioOutput::Device : AudioOutput::Silent);
      EngineNarrativeFixture::FailureEvidence evidence{report, engine};
      engine.run();
      EngineNarrativeFixture::snapshot(report, engine, "final_play_state");
    }
    report.check(diagnostics.errorCount() == 0,
                 "ordinary play teardown has no Vulkan errors");
    report.value["passed"] = true;
    report.save();
  } catch (const std::exception& error) {
    report.failed(error.what());
    throw;
  }
}
