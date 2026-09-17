#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/development/frame_capture.hpp"
#include "core/development/frame_timings.hpp"
#include "core/engine.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "launcher/executable_path.hpp"

namespace {
constexpr float step_seconds = float(FixedStepAccumulator::step_seconds);
constexpr unsigned cycle_steps = 120;
constexpr unsigned hold_steps = 60;
constexpr unsigned perturb_steps = 15;
constexpr float perturb_impulse = 2;

double audioTime(FrameTimings::Clock::time_point now) {
  return std::chrono::duration<double>(now.time_since_epoch()).count();
}
float speed(PhysicsVector v) { return std::hypot(v.x, v.y, v.z); }

std::array<float, 4> carryOrientation(std::array<float, 4> base, float yaw) {
  const auto turn = yawQuaternion(yaw);
  return {turn[3] * base[0] + turn[1] * base[2],
          turn[3] * base[1] + turn[1] * base[3],
          turn[3] * base[2] - turn[1] * base[0],
          turn[3] * base[3] - turn[1] * base[1]};
}

struct WorkloadSample {
  std::size_t frame{};
  unsigned step{};
  double elapsed{};
  unsigned awake{}, resting{}, pickups{}, drops{}, throws{}, breaks{};
  int held{-1};
  float maximum_linear{}, maximum_angular{}, door_angle{};
  double actor_distance{};
  unsigned actor_legs{};
};
}  // namespace

// Explicit development executable. Its fixed-boundary workload drives the
// existing Engine owners; no level filename changes ordinary game behavior.
struct EngineHouseholdMeasurement {
  std::vector<WorkloadSample> samples;
  WorldPosition anchor{};
  std::array<float, 4> orientation{0, 0, 0, 1};
  unsigned fixed_steps{}, pickups{}, drops{}, throws{}, breaks{};
  unsigned actor_legs{}, door_requests{};
  double completed_actor_distance{}, door_distance{}, discarded_wall_seconds{};
  bool capacity{}, completed{};

  explicit EngineHouseholdMeasurement(bool with_boxes) : capacity(with_boxes) {
    samples.reserve(5000);
  }

  void validateScene(Engine& engine) const {
    if (engine.physics_.boxCount() != (capacity ? 16U : 0U) ||
        engine.physics_.actorCount() != 1 || engine.physics_.doorCount() != 1 ||
        engine.level_.characters().actors[0].id != "walker" ||
        !findCharacterRoute(engine.level_.characters(), "walk") ||
        !findCharacterRoute(engine.level_.characters(), "walk-back") ||
        engine.level_.household().documents.empty() ||
        engine.level_.household().radios.size() != 1)
      throw std::runtime_error(
          "Measurement requires the prepared matching household profiles, "
          "walker routes, document and radio");
    const auto& lights = engine.level_.environmentLight().point_lights;
    if (std::count_if(lights.begin(), lights.end(), [](const auto& light) {
          return light.casts_shadows && light.initially_on;
        }) < 2)
      throw std::runtime_error(
          "Measurement requires both authored point-light shadows");
  }

  void boxCommands(PhysicsWorld& physics) {
    if (!capacity) return;
    const unsigned cycle = fixed_steps / cycle_steps;
    const unsigned phase = fixed_steps % cycle_steps;
    if (phase == 0) {
      if (physics.heldBox()) {
        physics.dropHeldBox();
        ++breaks;
      }
      const std::size_t index = cycle % physics.boxCount();
      const auto body = physics.boxState(index);
      anchor = {std::clamp(body.center.x, -5.3F, 5.3F),
                std::clamp(body.center.y + .35F, .6F, 3.2F),
                std::clamp(body.center.z, -6.3F, 6.3F)};
      orientation = body.orientation;
      if (physics.beginBoxHold(index, anchor, orientation)) ++pickups;
    }
    if (phase < hold_steps && physics.heldBox()) {
      const float t = float(phase) / hold_steps;
      const WorldPosition target{anchor.x + .2F * std::sin(t * 3.F),
                                 anchor.y + .1F * std::sin(t * 4.F),
                                 anchor.z + .2F * std::sin(t * 2.F)};
      if (!physics.updateBoxHold(
              target, carryOrientation(orientation, 40.F * std::sin(t * 3.F))))
        ++breaks;
    }
    if (phase == hold_steps && physics.heldBox()) {
      if (cycle % 2 == 0) {
        physics.dropHeldBox();
        ++drops;
      } else {
        const auto body = physics.boxState(*physics.heldBox());
        // Throw back into the authored room; the body still meets any nearer
        // wall, player, actor or accepted door through ordinary collision.
        if (physics.throwHeldBox({-body.center.x, .3F, -body.center.z}))
          ++throws;
      }
    }
    for (std::size_t i = 0; i < physics.boxCount(); ++i) {
      if (physics.heldBox() == i || (fixed_steps + i) % perturb_steps != 0)
        continue;
      const auto body = physics.boxState(i);
      // A real bounded pulse, not a transform reset or hidden sleep override.
      // Near the ceiling push downward. All post-step awake counts are checked;
      // a sleeping body makes the workload invalid, including in --check.
      physics.applyBoxImpulse(
          i, {0, body.center.y > 3.4F ? -perturb_impulse : perturb_impulse, 0});
    }
  }

  void participantCommands(Engine& engine) {
    const auto& result = engine.characters_.result(0);
    if (result.action == CharacterAction::Completed) {
      completed_actor_distance += result.walked_distance;
      ++actor_legs;
      const auto next = result.route == "walk" ? "walk-back" : "walk";
      if (engine.characters_.start("walker", next) != CharacterStart::Started)
        throw std::runtime_error(
            "Measurement could not restart the authored actor route");
    }
    if (fixed_steps % 180 == 0) {
      const auto eye = engine.player_.viewPose(1).position;
      (void)engine.doors_.act(0, DoorAction::Interact, {eye.x, eye.y, eye.z});
      ++door_requests;
    }
  }

  void observe(Engine& engine, std::size_t frame, double elapsed,
               bool held_before_world) {
    WorkloadSample row;
    row.frame = frame;
    row.step = fixed_steps;
    row.elapsed = elapsed;
    const auto held = engine.physics_.heldBox();
    if (held_before_world && !held) ++breaks;
    row.held = held ? int(*held) : -1;
    row.pickups = pickups;
    row.drops = drops;
    row.throws = throws;
    row.breaks = breaks;
    for (std::size_t i = 0; i < engine.physics_.boxCount(); ++i) {
      const auto body = engine.physics_.boxState(i);
      row.awake += !body.sleeping;
      row.maximum_linear =
          std::max(row.maximum_linear, speed(body.linear_velocity));
      row.maximum_angular =
          std::max(row.maximum_angular, speed(body.angular_velocity));
      const auto x =
          rotateVector(body.orientation, {household_box_half_extent, 0, 0});
      const auto y =
          rotateVector(body.orientation, {0, household_box_half_extent, 0});
      const auto z =
          rotateVector(body.orientation, {0, 0, household_box_half_extent});
      const float bottom =
          body.center.y - std::abs(x[1]) - std::abs(y[1]) - std::abs(z[1]);
      // Report actual near-floor, low-speed rest separately from sleeping.
      if (held != i && std::abs(bottom) <= .02F &&
          speed(body.linear_velocity) < .1F &&
          speed(body.angular_velocity) < .2F)
        ++row.resting;
    }
    row.door_angle = engine.doors_.state(0).angle;
    row.actor_distance =
        completed_actor_distance + engine.characters_.result(0).walked_distance;
    row.actor_legs = actor_legs;
    samples.push_back(row);
  }

  void run(Engine& engine, FrameTimings& timings, bool check,
           FrameCapture& capture) {
    validateScene(engine);
    engine.window_.useFullscreenMode(1920, 1080, 60);
    engine.window_.pollEvents();
    if (engine.window_.framebufferExtent().isZero()) {
      engine.window_.restore();
      engine.window_.pollEvents();
    }
    (void)engine.household_.sampleInput({}, true, false);
    engine.household_.toggleRadio(0);
    if (!engine.household_.radioOn(0))
      throw std::runtime_error("Measurement radio did not start");
    std::cout << "Validation "
              << (engine.renderer_.validationEnabled() ? "enabled" : "disabled")
              << "; FIFO; household " << (capacity ? "capacity" : "baseline")
              << "; 1920x1080/60 Hz; warmup 10 s; sample 60 s; silent audio\n";
    std::cout << "Explicit fixed-boundary physics workload; 2 N s vertical "
                 "pulses every 15 steps; "
                 "120-step cycles with 60-step hold and alternating "
                 "drop/throw; post-step awake gate\n";
    FixedStepAccumulator accumulator;
    using Clock = FrameTimings::Clock;
    auto previous = Clock::now();
    unsigned frames{};
    const PlayerViewPose view{{-4.9F, 3.3F, 5.9F}, {.55F, -.22F, -.806F}};
    while (check ? frames < 480 : timings.elapsedSeconds(Clock::now()) < 70) {
      const auto now = Clock::now();
      timings.beginFrame(now);
      engine.window_.pollEvents();
      const auto extent = engine.window_.framebufferExtent();
      if (engine.window_.shouldClose() || extent.width != 1920 ||
          extent.height != 1080)
        throw std::runtime_error(
            "Measurement interrupted or framebuffer is not 1920x1080");
      const double elapsed =
          std::chrono::duration<double>(now - previous).count();
      previous = now;
      discarded_wall_seconds += std::max(
          0., elapsed - FixedStepAccumulator::maximum_contribution_seconds);
      const auto batch = accumulator.advance(elapsed);
      auto& row = timings.current();
      const auto previous_pickups = pickups, previous_drops = drops,
                 previous_throws = throws;
      for (int step = 0; step < batch.complete_steps; ++step) {
        auto begin = Clock::now();
        participantCommands(engine);
        row.route_decision_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - begin)
                .count();
        begin = Clock::now();
        boxCommands(engine.physics_);
        const bool held_before_world = engine.physics_.heldBox().has_value();
        engine.physics_.advanceWorld(step_seconds);
        engine.player_.fixedStep(step_seconds);
        row.world_player_doors_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - begin)
                .count();
        engine.characters_.fixedStep(step_seconds, &row);
        begin = Clock::now();
        const float old_angle = engine.doors_.state(0).angle;
        engine.doors_.fixedStep(step_seconds, engine.physics_);
        door_distance += std::abs(engine.doors_.state(0).angle - old_angle);
        row.world_player_doors_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - begin)
                .count();
        observe(engine, frames, row.elapsed_seconds, held_before_world);
        ++fixed_steps;
      }
      const auto audio_begin = Clock::now();
      engine.audio_.update(audioTime(now));
      engine.audio_.listener(
          {view.position.x, view.position.y, view.position.z},
          {view.direction.x, view.direction.y, view.direction.z});
      const std::array<float, 1> angles{engine.doors_.state(0).angle};
      engine.audio_.acceptedDoors(angles);
      engine.characters_.handoffAudio();
      row.character_audio_ms +=
          std::chrono::duration<double, std::milli>(Clock::now() - audio_begin)
              .count();
      row.action = !capacity                     ? "baseline-actor-door"
                   : throws != previous_throws   ? "capacity-throw"
                   : drops != previous_drops     ? "capacity-drop"
                   : pickups != previous_pickups ? "capacity-pickup"
                   : engine.physics_.heldBox()   ? "capacity-hold"
                                                 : "capacity-release-settle";
      if (!samples.empty())
        row.action += ";awake=" + std::to_string(samples.back().awake);
      FrameRequest frame;
      frame.framebuffer = extent;
      frame.framebuffer_resized = engine.window_.consumeFramebufferResize();
      frame.camera = engine.player_.cameraFrame(1920.F / 1080, view);
      frame.point_light_enabled = engine.light_switch_.pointLightEnabled();
      frame.opaque_boxes =
          engine.household_.presentation(engine.doors_.presentation());
      frame.characters = engine.characters_.presentation();
      frame.captions = engine.audio_.captions();
      frame.household_text = engine.household_.text();
      if (check && frames == 120) engine.renderer_.requestSwapchainRecreation();
      if (check && frames == 360) capture.requested = true;
      if (!runtimeContinuesAfter(engine.renderer_.renderFrame(frame)))
        throw std::runtime_error("Unexpected measurement render outcome");
      timings.endFrame(Clock::now());
      ++frames;
    }
    completed = true;
    const auto first = std::find_if(
        samples.begin(), samples.end(),
        [&](const auto& sample) { return check || sample.elapsed >= 10; });
    if (first == samples.end())
      throw std::runtime_error("No measured fixed boundaries");
    const bool awake_valid =
        std::all_of(first, samples.end(), [&](const auto& sample) {
          return sample.awake == (capacity ? 16U : 0U);
        });
    const auto rest_steps =
        std::count_if(first, samples.end(),
                      [](const auto& sample) { return sample.resting > 0; });
    double measured_door_distance = 0;
    for (auto sample = first + 1; sample != samples.end(); ++sample)
      measured_door_distance +=
          std::abs(sample->door_angle - (sample - 1)->door_angle);
    std::cout << "Fixed steps " << fixed_steps << "; accepted holds " << pickups
              << "; drops " << drops << "; throws " << throws << "; breaks "
              << breaks << "; floor-rest boundaries " << rest_steps
              << "; actor distance " << samples.back().actor_distance
              << " m; actor legs " << actor_legs << "; door travel "
              << door_distance << " deg; awake gate "
              << (awake_valid ? "pass" : "FAIL") << '\n';
    if (!awake_valid ||
        (capacity &&
         (pickups - first->pickups < 2 || drops - first->drops < 1 ||
          throws - first->throws < 1 || rest_steps == 0)) ||
        samples.back().actor_distance - first->actor_distance <
            (check ? .5 : 16.) ||
        (!check && actor_legs - first->actor_legs < 2) ||
        measured_door_distance < 90)
      throw std::runtime_error(
          "Physical workload acceptance failed; raw timings and actual "
          "action/awake evidence retained");
  }

  void writeEvidence(const std::filesystem::path& path) const {
    std::ofstream stream(path, std::ios::binary);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(6)
           << "frame,step,elapsed_seconds,simulation_seconds,awake,floor_"
              "resting_free,held_index,accepted_pickups,accepted_drops,"
              "accepted_throws,hold_breaks,max_linear_mps,max_angular_radps,"
              "door_angle,actor_distance,actor_legs\n";
    for (const auto& row : samples)
      stream << row.frame << ',' << row.step << ',' << row.elapsed << ','
             << (row.step + 1) * double(step_seconds) << ',' << row.awake << ','
             << row.resting << ',' << row.held << ',' << row.pickups << ','
             << row.drops << ',' << row.throws << ',' << row.breaks << ','
             << row.maximum_linear << ',' << row.maximum_angular << ','
             << row.door_angle << ',' << row.actor_distance << ','
             << row.actor_legs << '\n';
    stream.close();
    std::cout << "Run duration completed " << completed
              << "; discarded wall time " << discarded_wall_seconds
              << " s; requested door actions " << door_requests << '\n';
  }
};

template <class Char>
int measure(int argc, Char** argv) {
  try {
    if (argc != 5)
      throw std::invalid_argument(
          "Usage: household_measure --check|--measure baseline|capacity "
          "<level.json> <new.csv>");
    const auto mode = std::filesystem::path(argv[1]).string();
    const auto workload = std::filesystem::path(argv[2]).string();
    if ((mode != "--check" && mode != "--measure") ||
        (workload != "baseline" && workload != "capacity"))
      throw std::invalid_argument(
          "Unknown household measurement mode/workload");
    const bool check = mode == "--check";
#ifndef NDEBUG
    if (!check)
      throw std::invalid_argument(
          "Performance samples require a separate Release build");
#endif
    const auto output =
        std::filesystem::absolute(std::filesystem::path(argv[4]));
    auto evidence = output;
    evidence.replace_extension(".workload.csv");
    auto image = output;
    image.replace_extension(".ppm");
    if (std::filesystem::exists(output) || std::filesystem::exists(evidence) ||
        (check && std::filesystem::exists(image)))
      throw std::invalid_argument("Choose fresh timing/evidence/capture paths");
    near_laugh::RuntimeConfig config;
    config.resource_root = launcher::executableResourceRoot();
    config.level_path =
        std::filesystem::absolute(std::filesystem::path(argv[3]));
    config.window_width = 1920;
    config.window_height = 1080;
    config.window_title = "P06 household measurement";
    FrameTimings timings;
    FrameCapture capture;
    ValidationDiagnostics diagnostics;
    EngineHouseholdMeasurement measurement(workload == "capacity");
    try {
      Engine engine(config, diagnostics, false, AudioOutput::Silent, &timings,
                    false, check ? &capture : nullptr);
      measurement.run(engine, timings, check, capture);
    } catch (...) {
      timings.writeCsv(output);
      measurement.writeEvidence(evidence);
      throw;
    }
    timings.writeCsv(output);  // Engine teardown drains the last GPU queries.
    measurement.writeEvidence(evidence);
    if (check) capture.writePpm(image);
    if (diagnostics.errorCount())
      throw std::runtime_error(
          "Vulkan validation errors through final teardown");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return measure(argc, argv); }
#else
int main(int argc, char** argv) { return measure(argc, argv); }
#endif
