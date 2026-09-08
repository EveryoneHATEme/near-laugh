#ifndef TESTS_RUNTIME_CHARACTER_SMOKE_HPP
#define TESTS_RUNTIME_CHARACTER_SMOKE_HPP

#include "core/development/frame_capture.hpp"
#include "core/engine.hpp"
#include "development/character_fixture.hpp"

class CharacterSmokeFailure {
 public:
  CharacterSmokeFailure(const char* variable, const char* stage)
      : variable_(variable) {
    set(stage);
  }
  ~CharacterSmokeFailure() { set(""); }

 private:
  void set(const char* stage) {
#ifdef _WIN32
    (void)_putenv_s(variable_, stage);
#else
    if (*stage)
      (void)setenv(variable_, stage, 1);
    else
      (void)unsetenv(variable_);
#endif
  }
  const char* variable_;
};

struct EngineCharacterSmoke {
  static void run(ValidationDiagnostics& diagnostics) {
    const auto require = [](bool condition, const char* message) {
      if (!condition) throw std::runtime_error(message);
    };
    near_laugh::RuntimeConfig failed_config;
    failed_config.resource_root = std::filesystem::absolute("resources");
    failed_config.level_path = failed_config.resource_root /
                               "levels/scripted-characters-four.level.json";
    failed_config.window_width = 800;
    failed_config.window_height = 600;
    {
      const auto missing_root = std::filesystem::absolute(
          "build/character-missing-selected-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
      std::filesystem::create_directories(missing_root);
      struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() {
          std::error_code error;
          std::filesystem::remove_all(root, error);
        }
      } cleanup{missing_root};
      for (const auto name : {"fonts", "shaders"})
        std::filesystem::copy(failed_config.resource_root / name,
                              missing_root / name,
                              std::filesystem::copy_options::recursive);
      auto selected_failure = failed_config;
      selected_failure.resource_root = missing_root;
      bool failed = false;
      try {
        Engine engine(selected_failure, diagnostics, false,
                      AudioOutput::Silent);
      } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        failed = message.find("walker") != std::string::npos &&
                 message.find("test-mannequin") != std::string::npos;
      }
      require(
          failed,
          "Missing selected mannequin did not fail with actor/model context");
    }
    for (const auto& failure :
         std::array{std::array{"NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE",
                               "actor-body-3", "north"},
                    std::array{"NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE",
                               "character_index_upload", "character index"}}) {
      bool failed = false;
      {
        CharacterSmokeFailure inject(failure[0], failure[1]);
        try {
          Engine engine(failed_config, diagnostics, false, AudioOutput::Silent);
        } catch (const std::runtime_error& error) {
          failed = std::string_view(error.what()).find(failure[2]) !=
                   std::string_view::npos;
        }
      }
      require(
          failed,
          "Composed character startup failure did not report expected context");
      Engine recovered(failed_config, diagnostics, false, AudioOutput::Silent);
      require(recovered.tick(),
              "Fresh runtime failed after partial character cleanup");
    }
    const auto capture_directory = std::filesystem::absolute(
        "build/scripted-character-runtime-captures/" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(capture_directory);
    std::cout << "Character acceptance captures: " << capture_directory << '\n';
    for (const bool four : {false, true}) {
      near_laugh::RuntimeConfig config;
      config.resource_root = std::filesystem::absolute("resources");
      config.level_path = config.resource_root / "levels" /
                          (four ? "scripted-characters-four.level.json"
                                : "scripted-characters.level.json");
      config.window_width = 800;
      config.window_height = 600;
      FrameCapture capture;
      Engine engine(config, diagnostics, false, AudioOutput::Silent, nullptr,
                    true, &capture);
      require(engine.characters_.presentation().size() == (four ? 4 : 1),
              "Actor selection mismatch");
      require(engine.characters_.result(0).instance == 1,
              "Authored initial route did not start once");
      const PlayerActionSnapshot input{};
      const CharacterDevelopmentInput released{};
      const auto tick = [&](const CharacterDevelopmentInput& controls = {}) {
        require(engine.tick(&input, &controls),
                "Character runtime stopped unexpectedly");
      };
      tick();
      bool door_opened = false;
      std::array<bool, 6> captured{};
      for (int frame = 0; frame < 310; ++frame) {
        // Supply a full capped interval through Engine's actual sampler. Fast
        // smoke presentation need not wait for 30 seconds of route wall time.
        engine.fixed_step_.reset();
        (void)engine.fixed_step_.sample(FixedStepAccumulator::Clock::now() -
                                        std::chrono::milliseconds(100));
        if (!door_opened && engine.characters_.result(0).obstruction ==
                                PhysicsActorObstruction::Door) {
          (void)engine.doors_.act(0, DoorAction::Interact, {0, 2, 4});
          door_opened = true;
        }
        if (frame == 70 || frame == 160)
          engine.renderer_.requestSwapchainRecreation();
        const auto action = engine.characters_.result(0);
        int capture_index =
            engine.audio_.status("walker-touch") == CueStatus::Playing ? 5
            : frame == 0                                               ? 0
            : action.obstruction == PhysicsActorObstruction::Door      ? 2
            : action.action == CharacterAction::Interacting            ? 3
            : action.action == CharacterAction::Completed              ? 4
            : action.action == CharacterAction::Walking && frame > 15  ? 1
                                                                       : -1;
        if (capture_index >= 0 && !captured[capture_index])
          capture.requested = true;
        tick();
        if (capture_index >= 3 && !captured[capture_index]) {
          // Close inspection of the accepted final action, beyond the doorway.
          FrameRequest inspection{
              engine.window_.framebufferExtent(),
              false,
              character_fixture::camera({2, 2.4F, 5.1F}, {0, 1.5F, 5.3F},
                                        800.F / 600),
              {},
              engine.light_switch_.pointLightEnabled()};
          inspection.characters = engine.characters_.presentation();
          inspection.opaque_boxes = engine.doors_.presentation();
          inspection.captions = engine.audio_.captions();
          capture.requested = true;
          (void)engine.renderer_.renderFrame(inspection);
        }
        if (capture_index >= 0 && !captured[capture_index] &&
            !capture.requested) {
          capture.writePpm(
              capture_directory /
              (std::string(four ? "four-" : "one-") +
               std::array{"initial", "walking", "door-blocked", "interacting",
                          "completed", "caption"}[capture_index] +
               ".ppm"));
          captured[capture_index] = true;
        }
        for (std::size_t i = 0; i < engine.physics_.actorCount(); ++i) {
          const auto accepted = engine.physics_.actorState(i);
          const auto& visible = engine.characters_.presentation()[i];
          require(visible.position ==
                          std::array<float, 3>{accepted.feet_position.x,
                                               accepted.feet_position.y,
                                               accepted.feet_position.z} &&
                      visible.yaw_degrees == accepted.yaw_degrees,
                  "Render placement diverged from actor collision");
          require(engine.characters_.result(i).instance == 1,
                  "Recovery replayed an actor action");
        }
      }
      require(door_opened, "Route did not exercise blocking door");
      require(std::all_of(captured.begin(), captured.end(),
                          [](bool value) { return value; }),
              "Route did not retain every acceptance pose");
      require(engine.characters_.result(0).action == CharacterAction::Completed,
              "Route did not complete");
      require(engine.audio_.instance("walker-touch") != 0,
              "Interaction marker was not handed to audio");

      CharacterDevelopmentInput restart;
      restart.restart = true;
      engine.window_.setCursorCaptured(false);
      tick(restart);
      engine.window_.setCursorCaptured(true);
      tick(restart);
      require(engine.characters_.result(0).instance == 1,
              "Inactive restart edge replayed");
      tick(released);
      tick(restart);
      require(engine.characters_.result(0).instance == 2,
              "Explicit route restart failed");
      tick(released);

      // Pause while player/actor/door motion and a foreground cue are pending.
      (void)engine.doors_.act(0, DoorAction::Interact, {0, 2, 4});
      CharacterDevelopmentInput pause;
      pause.pause = true;
      tick(pause);
      require(engine.suspended_ && engine.audio_.suspended(),
              "World/audio pause diverged");
      const auto feet = engine.physics_.actorState(0).feet_position;
      const auto player = engine.physics_.characterState().foot_position;
      const auto angle = engine.doors_.state(0).angle;
      const auto palette = engine.characters_.playback(0).pose();
      const auto audio_time = engine.audio_.activeTime();
      std::this_thread::sleep_for(std::chrono::milliseconds(120));
      tick(released);
      require(
          engine.physics_.actorState(0).feet_position == feet &&
              engine.physics_.characterState().foot_position.x == player.x &&
              engine.physics_.characterState().foot_position.z == player.z &&
              engine.doors_.state(0).angle == angle &&
              engine.characters_.playback(0).pose() == palette &&
              engine.audio_.activeTime() == audio_time,
          "Suspension advanced a participant");
      tick(pause);
      require(!engine.suspended_, "Explicit resume failed");
      require(engine.physics_.actorState(0).feet_position == feet,
              "Resume replayed suspended motion");
      tick(released);

      engine.window_.setCursorCaptured(false);
      const auto active_time = engine.audio_.activeTime();
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      tick();
      require(engine.audio_.activeTime() > active_time && !engine.suspended_,
              "Cursor release suspended world");

      engine.window_.minimize();
      engine.window_.pollEvents();
      require(engine.window_.framebufferExtent().isZero(),
              "Runtime did not minimize");
      for (int wait = 0; wait < 2; ++wait) {
        std::jthread wake([] {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          glfwPostEmptyEvent();
        });
        tick(restart);
        require(engine.suspended_, "Minimized wait did not suspend simulation");
      }
      const auto frozen = engine.physics_.actorState(0).feet_position;
      const auto frozen_time = engine.audio_.activeTime();
      engine.window_.restore();
      engine.window_.pollEvents();
      engine.window_.setCursorCaptured(true);
      tick(restart);
      require(engine.characters_.result(0).instance == 2 &&
                  engine.physics_.actorState(0).feet_position == frozen &&
                  engine.audio_.activeTime() == frozen_time,
              "Restore replayed a held edge or suspended time");
      CharacterDevelopmentInput cancel;
      cancel.cancel = true;
      tick(cancel);
      require(engine.characters_.result(0).action == CharacterAction::Canceled,
              "Development cancel failed");
      engine.window_.requestClose();
      require(!engine.tick(), "Close did not terminate character runtime");
    }
    if (diagnostics.errorCount())
      throw std::runtime_error("Character runtime recorded Vulkan errors");
  }
};
#endif
