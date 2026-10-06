#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <locale>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "core/development/frame_timings.hpp"
#include "core/engine.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/world/level_document.hpp"

namespace {
std::string identity(std::string_view prefix, std::size_t i) {
  return std::string(prefix) + (i < 10 ? "0" : "") + std::to_string(i);
}
LevelDocument checkedLoad(const std::filesystem::path& path) {
  auto loaded = loadLevelDocument(path);
  if (!loaded)
    throw std::runtime_error(formatLevelDiagnostics(loaded.diagnostics));
  const auto errors = validateLevelDocument(*loaded.document, path);
  if (!errors.empty()) throw std::runtime_error(formatLevelDiagnostics(errors));
  return std::move(*loaded.document);
}
LevelDocument narrativeMeasurementDocument(const LevelDocument& scene,
                                           bool capacity) {
  if (scene.household.radios.size() != 1 ||
      scene.household.radios[0].id != "receiver" ||
      !scene.household.radios[0].initially_on || scene.doors.size() != 1 ||
      scene.doors[0].id != "contact-door" || scene.doors[0].initially_open ||
      scene.doors[0].initially_locked ||
      scene.environment_light.point_lights.empty() ||
      scene.environment_light.point_lights[0].id != "table-light" ||
      scene.environment_light.point_lights[0].initially_on ||
      scene.characters.actors.size() != 1 ||
      scene.characters.actors[0].id != "walker" ||
      std::none_of(
          scene.audio.sources.begin(), scene.audio.sources.end(),
          [](const auto& source) { return source.id == "sequence-source"; }) ||
      std::any_of(
          scene.characters.actors.begin(), scene.characters.actors.end(),
          [](const auto& actor) { return actor.initial_route.has_value(); }))
    throw std::runtime_error(
        "Narrative measurement requires the neutral T4 scene with its initial "
        "radio, door, light and idle actors");
  LevelDocument result = scene;
  result.narrative = {};
  for (std::size_t i = 0; i < level_maximum_narrative_fact_count; ++i)
    result.narrative.facts.push_back({identity("fact-", i), true});
  for (std::size_t i = 0; i < level_maximum_narrative_region_count; ++i)
    result.narrative.regions.push_back({identity("region-", i),
                                        {-2.F + float(i) * .01F, 1, 1.5F},
                                        {.8F, 1, .4F}});
  if (!capacity) return result;
  for (std::size_t i = 0; i < level_maximum_narrative_event_count; ++i) {
    NarrativeEventDefinition event;
    event.id = identity("capacity-", i);
    event.trigger = NarrativeInteractionTrigger{
        NarrativeInteractionTarget::Radio, "receiver",
        NarrativeInteractionAction::RadioOn};
    event.repeat = NarrativeRepeat::Rearm;
    for (std::size_t predicate = 0;
         predicate < level_maximum_narrative_predicate_count; ++predicate)
      event.guards.push_back(NarrativeFactPredicate{
          identity("fact-", (i + predicate) % 32), true});
    event.cancel = event.guards;
    std::get<NarrativeFactPredicate>(event.cancel->back()).value = false;
    for (std::size_t step = 0; step < level_maximum_narrative_step_count;
         ++step) {
      switch (step % 7) {
        case 0:
          event.steps.push_back(
              NarrativeSetFactStep{identity("fact-", (i + step) % 32), true});
          break;
        case 1:
          event.steps.push_back(NarrativeSetLightStep{"table-light", false});
          break;
        case 2:
          event.steps.push_back(
              NarrativeSetDoorOpenStep{"contact-door", false});
          break;
        case 3:
          event.steps.push_back(
              NarrativeSetDoorLockedStep{"contact-door", false});
          break;
        case 4:
          event.steps.push_back(NarrativeSetRadioStep{"receiver", true});
          break;
        case 5:
          event.steps.push_back(NarrativeDelayStep{0});
          break;
        case 6:
          event.steps.push_back(NarrativeWaitUntilStep{event.guards});
          break;
      }
    }
    result.narrative.events.push_back(std::move(event));
  }
  const auto errors = validateLevelDocument(result);
  if (!errors.empty()) throw std::runtime_error(formatLevelDiagnostics(errors));
  return result;
}
}  // namespace

struct EngineNarrativeMeasurement {
  struct Workload {
    double elapsed{};
    std::uint64_t total_runs{}, accepted_radio_changes{}, radio_on_starts{},
        dropped_trace{};
    std::size_t completed{}, running{};
  };
  std::vector<Workload> samples;
  bool complete{};
  std::uint64_t changes{}, on_starts{};
  std::string failure;

  void run(Engine& engine, FrameTimings& timings, bool capacity, bool check) {
    engine.window_.useFullscreenMode(1920, 1080, 60);
    engine.window_.pollEvents();
    if (engine.window_.framebufferExtent().isZero()) {
      engine.window_.restore();
      engine.window_.pollEvents();
    }
    if (!check && engine.renderer_.validationEnabled())
      throw std::runtime_error(
          "Release measurement requires Vulkan validation disabled");
    std::cout
        << "Validation "
        << (engine.renderer_.validationEnabled() ? "enabled" : "disabled")
        << "; FIFO; 1920x1080/60 Hz; warmup 10 s; sample 60 s; silent audio\n";
    using Clock = FrameTimings::Clock;
    auto start = Clock::now();
    double next_radio = .5;
    std::size_t frames{};
    samples.reserve(5000);
    const PlayerActionSnapshot input{};
    while (true) {
      const auto now = Clock::now();
      const auto elapsed = std::chrono::duration<double>(now - start).count();
      if (elapsed >= (check ? 3. : 70.)) break;
      timings.beginFrame(now);
      auto& row = timings.current();
      row.action = capacity ? "capacity-64-events" : "events-disabled";
      if (elapsed >= next_radio) {
        const bool before = engine.household_.radioOn(0);
        engine.household_.toggleRadio(0, &engine.accepted_interactions_);
        if (engine.household_.radioOn(0) == before)
          throw std::runtime_error("Measurement radio stimulus was refused");
        ++changes;
        if (!before) ++on_starts;
        next_radio = elapsed + .5;  // Never replay missed inputs after a stall.
      }
      if (check && frames == 60) engine.renderer_.requestSwapchainRecreation();
      const bool continued = engine.tick(&input);
      timings.endFrame(
          Clock::now());  // Workload assertions are outside active CPU timing.
      const auto extent = engine.window_.framebufferExtent();
      if (!continued || extent.width != 1920 || extent.height != 1080 ||
          engine.suspended_)
        throw std::runtime_error(
            "Measurement interrupted by close, resize or suspension");
      Workload workload{elapsed, 0, changes, on_starts,
                        engine.narrative_->droppedTraceRecords()};
      for (const auto& event : engine.narrative_->runs()) {
        workload.total_runs += event.run;
        workload.completed += event.status == NarrativeRunStatus::Completed;
        workload.running += event.status == NarrativeRunStatus::Running;
        if (event.status == NarrativeRunStatus::Failed ||
            event.status == NarrativeRunStatus::Canceled)
          throw std::runtime_error("Capacity event failed/canceled: " +
                                   event.event + ": " + event.reason);
        if (event.run && event.step != level_maximum_narrative_step_count)
          throw std::runtime_error(
              "Capacity event did not execute all 32 immediate steps");
      }
      if (workload.total_runs != on_starts * (capacity ? 64 : 0) ||
          workload.running != 0 ||
          (on_starts && workload.completed != (capacity ? 64U : 0U)))
        throw std::runtime_error(
            "Measured event count does not match accepted radio occurrences");
      if (engine.light_switch_.pointLightEnabled()[0] ||
          engine.doors_.state(0).moving || engine.doors_.state(0).angle != 0 ||
          engine.doors_.state(0).locked ||
          engine.characters_.result(0).instance != 0 ||
          engine.audio_.instance("sequence-source") != 0)
        throw std::runtime_error(
            "Capacity workload changed scene presentation relative to disabled "
            "events");
      samples.push_back(workload);
      ++frames;
    }
    const auto first = std::find_if(
        samples.begin(), samples.end(),
        [&](const auto& sample) { return check || sample.elapsed >= 10; });
    if (first == samples.end() ||
        samples.back().radio_on_starts - first->radio_on_starts <
            (check ? 1 : 55))
      throw std::runtime_error(
          "Insufficient executed radio/event cycles in the measurement "
          "interval");
    complete = true;
  }

  void evidence(const std::filesystem::path& csv, bool capacity,
                bool check) const {
    auto workload_path = csv;
    workload_path.replace_extension("workload.csv");
    std::ofstream output(workload_path, std::ios::binary);
    output.imbue(std::locale::classic());
    output << "elapsed_seconds,total_runs,accepted_radio_changes,radio_on_"
              "starts,completed,running,dropped_trace\n";
    for (const auto& row : samples)
      output << row.elapsed << ',' << row.total_runs << ','
             << row.accepted_radio_changes << ',' << row.radio_on_starts << ','
             << row.completed << ',' << row.running << ',' << row.dropped_trace
             << '\n';
    if (!output)
      throw std::runtime_error("Cannot write narrative workload CSV");
    auto report_path = csv;
    report_path.replace_extension("workload.json");
    nlohmann::ordered_json report{
        {"passed", complete},
        {"failure", failure},
        {"mode", check ? "check_not_performance" : "measure"},
        {"capacity", capacity},
        {"events", capacity ? 64 : 0},
        {"facts", 32},
        {"regions", 32},
        {"steps_per_event", capacity ? 32 : 0},
        {"predicates_per_list", capacity ? 8 : 0},
        {"radio_changes", changes},
        {"radio_on_starts", on_starts},
        {"frames", samples.size()},
        {"world_scope", "household/physics/player and doors"},
        {"character_audio_scope", "accepted character audio handoff"},
        {"narrative_cost", "included in existing total active CPU scope"},
        {"presentation",
         "same idle actors, static lights/doors, radio stimulus and camera in "
         "both profiles"}};
    std::ofstream report_file(report_path, std::ios::binary);
    report_file << report.dump(2) << '\n';
    if (!report_file)
      throw std::runtime_error("Cannot write narrative workload report");
  }
};

template <class Char>
int measure(int argc, Char** argv) {
  try {
    if (argc == 4 && std::filesystem::path(argv[1]).string() == "--preflight") {
      const auto source = checkedLoad(argv[2]);
      const auto disabled = narrativeMeasurementDocument(source, false);
      const auto capacity = narrativeMeasurementDocument(source, true);
      auto comparable = capacity;
      comparable.narrative.events.clear();
      if (comparable != disabled)
        throw std::runtime_error(
            "Measurement scenes differ outside narrative events");
      const std::filesystem::path directory = argv[3];
      if (std::filesystem::exists(directory))
        throw std::runtime_error("Choose a fresh preflight directory");
      std::filesystem::create_directories(directory);
      for (const auto& [name, document] :
           {std::pair{"disabled", disabled}, std::pair{"capacity", capacity}}) {
        const auto saved = saveLevelDocument(
            directory / (std::string(name) + ".level.json"), document);
        if (!saved)
          throw std::runtime_error(formatLevelDiagnostics(saved.diagnostics));
      }
      std::cout << "Narrative measurement preflight passed; no window or GPU "
                   "constructed\n";
      return 0;
    }
    if (argc != 5)
      throw std::runtime_error(
          "Usage: narrative_measure --check|--measure disabled|capacity LEVEL "
          "CSV; or --preflight LEVEL NEW_DIR");
    const auto mode = std::filesystem::path(argv[1]).string();
    const auto profile = std::filesystem::path(argv[2]).string();
    const bool check = mode == "--check";
    const bool capacity = profile == "capacity";
    if ((!check && mode != "--measure") || (!capacity && profile != "disabled"))
      throw std::runtime_error("Unknown narrative measurement mode or profile");
#ifndef NDEBUG
    if (!check)
      throw std::runtime_error(
          "Performance samples require a separate Release build");
#endif
    const auto level_path = std::filesystem::absolute(argv[3]);
    const auto document =
        narrativeMeasurementDocument(checkedLoad(level_path), capacity);
    const auto csv = std::filesystem::absolute(argv[4]);
    auto generated = csv;
    generated.replace_extension("level.json");
    auto workload = csv;
    workload.replace_extension("workload.csv");
    auto report = csv;
    report.replace_extension("workload.json");
    if (std::filesystem::exists(csv) || std::filesystem::exists(generated) ||
        std::filesystem::exists(workload) || std::filesystem::exists(report))
      throw std::runtime_error("Choose new timing and generated-level paths");
    std::filesystem::create_directories(csv.parent_path());
    const auto saved = saveLevelDocument(generated, document);
    if (!saved)
      throw std::runtime_error(formatLevelDiagnostics(saved.diagnostics));
    near_laugh::RuntimeConfig config;
    config.resource_root = level_path.parent_path().parent_path();
    config.level_path = generated;
    config.window_title = "T4 narrative measurement";
    config.window_width = 1920;
    config.window_height = 1080;
    FrameTimings timings;
    EngineNarrativeMeasurement measurement;
    ValidationDiagnostics diagnostics;
    try {
      Engine engine(config, diagnostics, false, AudioOutput::Silent, &timings);
      measurement.run(engine, timings, capacity, check);
    } catch (const std::exception& error) {
      measurement.complete = false;
      measurement.failure = error.what();
      timings.writeCsv(csv);
      measurement.evidence(csv, capacity, check);
      throw;
    }
    if (diagnostics.errorCount()) {
      measurement.complete = false;
      measurement.failure =
          "Narrative measurement recorded Vulkan errors through teardown";
    }
    timings.writeCsv(csv);
    measurement.evidence(csv, capacity, check);
    if (!measurement.complete) throw std::runtime_error(measurement.failure);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Narrative measurement: " << error.what() << '\n';
    return 1;
  }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return measure(argc, argv); }
#else
int main(int argc, char** argv) { return measure(argc, argv); }
#endif
