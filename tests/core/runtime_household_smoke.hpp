#ifndef TESTS_RUNTIME_HOUSEHOLD_SMOKE_HPP
#define TESTS_RUNTIME_HOUSEHOLD_SMOKE_HPP

#include "core/development/frame_capture.hpp"
#include "core/engine.hpp"
#include "core/render/scene_assets.hpp"
#include "core/world/household.hpp"
#include "development/character_fixture.hpp"

// This opt-in GPU scenario uses real Engine ownership and presentation. Input
// and fixed boundaries are explicit; readback frames are never measurements.
struct EngineHouseholdSmoke {
  enum class TextLayer { Scene, All, Reader, Captions };

  class Failure {
   public:
    Failure(const char* variable, const char* stage) : variable_(variable) {
#ifdef _WIN32
      char* previous = nullptr;
      std::size_t length = 0;
      require(_dupenv_s(&previous, &length, variable) == 0,
              "Cannot read household failure control");
      if (previous) previous_ = previous;
      std::free(previous);
#else
      if (const auto* previous = std::getenv(variable)) previous_ = previous;
#endif
      require(set(stage), "Cannot set household failure control");
    }
    ~Failure() { (void)set(previous_ ? previous_->c_str() : ""); }
   private:
    bool set(const char* value) {
#ifdef _WIN32
      return _putenv_s(variable_, value) == 0;
#else
      return (*value ? setenv(variable_, value, 1) : unsetenv(variable_)) == 0;
#endif
    }
    const char* variable_;
    std::optional<std::string> previous_;
  };

  static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
  }

  static LevelDocument document(bool maximum) {
    LevelDocument d;
    d.solids = {{{0, -.25F, 0}, {25, .25F, 25},
                  {210, 205, 190, 255}, PrototypeSolidKind::Floor}};
    d.entries = {{"explore", {{0, 0, 0}, -90}}};
    d.default_entry = "explore";
    d.environment_light.ambient_intensity = .1F;
    d.environment_light.point_lights = {
        {{0, 4, -2}, {1, .9F, .8F}, 1, 12, "room", true, true}};
    d.audio.cues = {
        {"radio-loop", "radio", "radio", AudioCueKind::Ambience, true, true}};
    for (int i = 0; i < (maximum ? 16 : 1); ++i)
      d.household.boxes.push_back(
          {"box-" + std::to_string(i),
           {i == 0 ? 0.F : -5.F + (i % 4), 1.65F, -1.F - (i / 4)},
           float(i * 11)});
    for (int i = 0; i < (maximum ? 32 : 1); ++i)
      d.household.documents.push_back(
          {"note-" + std::to_string(i), {float(i % 8) - 4, .003F, -6.F - i / 8},
           float(i * 7), "Записка Ёжика", {"Первая страница. Ёж и ёлка.",
                                         "Вторая страница: Latin / русский."}});
    for (int i = 0; i < (maximum ? 8 : 1); ++i) {
      const auto id = std::to_string(i);
      d.props.push_back({"receiver-" + id, "apartment-radio",
                         {float(i) - 4, 0, -11}, 0, 1, {}});
      d.audio.sources.push_back({"source-" + id, "radio-loop",
                                 {15, 1, 15}, 1, 1, 20, false});
      d.household.radios.push_back(
          {"control-" + id, "receiver-" + id, "source-" + id, i == 0});
    }
    if (maximum)
      for (int i = 0; i < 32; ++i) {
        DoorDefinition door;
        door.id = "door-" + std::to_string(i);
        door.hinge_position = {-15.F + (i % 8) * 4, 0, 4.F + (i / 8) * 4};
        d.doors.push_back(door);
      }
    const auto errors = validateLevelDocument(d);
    if (!errors.empty())
      throw std::runtime_error(formatLevelDiagnostics(errors));
    return d;
  }

  static void tick(Engine& engine, const PlayerActionSnapshot& input = {},
                   bool advance = true,
                   const HouseholdDevelopmentInput& controls = {}) {
    engine.fixed_step_.reset();
    if (advance)
      (void)engine.fixed_step_.sample(FixedStepAccumulator::Clock::now() -
                                      std::chrono::milliseconds(100));
    require(engine.tick(&input, nullptr, &controls),
            "Household runtime stopped unexpectedly");
  }

  static void preflight() {
    const CaptionFont font("resources");
    for (const bool maximum : {false, true}) {
      const auto d = document(maximum);
      validateHouseholdText(d.household, font);
      const auto assets = prepareSceneAssets("resources", d);
      require(assets.obstacle_material.has_value(),
              "Household smoke scene has no changing geometry material");
      auto boxes = householdInitialPresentation(d);
      for (const auto& door : d.doors) {
        const auto parts = doorPresentationBoxes(door, 0, false);
        boxes.insert(boxes.end(), parts.begin(), parts.end());
      }
      require(boxes.size() == (maximum ? 248U : 3U),
              "Household smoke fixture count mismatch");
      require(buildOpaqueBoxVertices(boxes).size() == boxes.size() * 36,
              "Household smoke fixture has invalid geometry");
    }
  }

  static void capture(Engine& engine, FrameCapture& pixels,
                      const std::filesystem::path& path,
                      TextLayer text_layer = TextLayer::Scene,
                      std::optional<std::array<float, 4>> first_box_rotation = {}) {
    FrameRequest frame;
    frame.framebuffer = engine.window_.framebufferExtent();
    frame.camera = character_fixture::camera(
        {2.8F, 3.8F, 4}, {-1, .5F, -2},
        float(frame.framebuffer.width) / frame.framebuffer.height);
    frame.point_light_enabled = engine.light_switch_.pointLightEnabled();
    const auto doors = engine.doors_.presentation();
    const auto current_boxes = engine.household_.presentation(doors);
    auto boxes = std::vector(current_boxes.begin(), current_boxes.end());
    if (first_box_rotation)
      boxes.at(doors.size()).orientation = *first_box_rotation;
    frame.opaque_boxes = boxes;
    frame.characters = engine.characters_.presentation();
    // Maximum supported scalar counts, with word boundaries for valid layout.
    const auto repeat = [](std::string_view word, int count) {
      std::string text;
      for (int i = 0; i < count; ++i) text += word;
      return text;
    };
    const auto title = repeat("Ёж ", 26) + "Ёж";
    const auto page = repeat("Ёж идёт. ", 53) + "Ёж ";
    const auto controls = repeat("W ", 48);
    const auto hint = repeat("W ", 48);
    const auto feedback = repeat("W ", 60);
    const auto foreground = repeat("WWWWWWWWW ", 16);
    const auto ambience = repeat("rrrrrrrrr ", 16);
    if (text_layer == TextLayer::Reader || text_layer == TextLayer::All)
      frame.household_text = {{title, page, controls}, hint, feedback};
    if (text_layer == TextLayer::Captions || text_layer == TextLayer::All)
      frame.captions = {{"", foreground}, {"", ambience}};
    pixels.requested = true;
    for (int attempt = 0; attempt < 4 && pixels.requested; ++attempt)
      (void)engine.renderer_.renderFrame(frame);
    require(!pixels.requested && !pixels.rgba.empty(),
            "Household readback was not submitted");
    require(pixels.width == frame.framebuffer.width &&
                pixels.height == frame.framebuffer.height,
            "Household readback dimensions differ from framebuffer");
    pixels.writePpm(path);
  }

  static void run(ValidationDiagnostics& diagnostics) {
    preflight();
    const auto output = std::filesystem::absolute(
        "build/household-runtime-captures/" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(output);
    std::cout << "Household runtime captures: " << output << '\n';
    near_laugh::RuntimeConfig config;
    config.resource_root = std::filesystem::absolute("resources");
    config.window_width = 800;
    config.window_height = 600;
    for (const bool maximum : {false, true}) {
      const auto path = output / (maximum ? "maximum.level.json" : "door-free.level.json");
      const auto saved = saveLevelDocument(path, document(maximum));
      if (!saved) throw std::runtime_error(formatLevelDiagnostics(saved.diagnostics));
      config.level_path = path;
      FrameCapture pixels;
      {
        Engine engine(config, diagnostics, false, AudioOutput::Silent,
                      nullptr, false, &pixels);
        require(engine.renderer_.validationEnabled(),
                "Household smoke requires Vulkan validation");
        require(engine.household_.presentation(engine.doors_.presentation()).size() ==
                    (maximum ? 248U : 3U), "Household mixed geometry count mismatch");
        require(engine.audio_.instance("source-0") == 1 &&
                    engine.household_.radioOn(0), "Initial radio did not start exactly once");
        const auto initial = engine.physics_.boxState(0);
        const auto prefix = maximum ? "maximum-" : "door-free-";
        capture(engine, pixels, output / (std::string(prefix) + "initial.ppm"));
        engine.physics_.applyBoxImpulse(0, {0, 1, 0}, {.03F, .08F, .05F});
        tick(engine);
        require(engine.physics_.boxState(0).orientation != initial.orientation,
                "Physical box did not rotate");
        capture(engine, pixels, output / (std::string(prefix) + "spinning.ppm"));
        const auto spinning_pixels = pixels.rgba;
        // Same accepted centres, other bodies, camera and empty text layers:
        // only box 0's orientation differs. Falling neighbours or new hints
        // cannot make a renderer that ignores rotation pass this comparison.
        capture(engine, pixels, output / (std::string(prefix) + "rotation-reference.ppm"),
                TextLayer::Scene, initial.orientation);
        require(spinning_pixels != pixels.rgba,
                "Changing only box orientation produced unchanged pixels");

        engine.household_.openDocument(0);
        tick(engine, {}, false);
        PlayerActionSnapshot next;
        next.move_right = true;
        tick(engine, next, false);
        require(engine.household_.page() == 1, "Engine did not navigate reader");
        for (const auto size : {FramebufferExtent{800, 600}, {1920, 1080}, {3840, 2160}}) {
          engine.window_.setSize(int(size.width), int(size.height));
          engine.window_.pollEvents();
          engine.renderer_.requestSwapchainRecreation();
          tick(engine, next, false);
          require(engine.household_.page() == 1 && engine.household_.readingDocument() == 0,
                  "Resize replayed navigation or lost reader identity");
          const auto actual = engine.window_.framebufferExtent();
          require(actual.width == size.width && actual.height == size.height,
                  "Desktop could not provide required household test resolution");
          const auto name = std::string(prefix) + std::to_string(size.width);
          capture(engine, pixels, output / (name + "-scene.ppm"));
          const auto scene_pixels = pixels.rgba;
          capture(engine, pixels, output / (name + "-reader.ppm"), TextLayer::Reader);
          require(scene_pixels != pixels.rgba, "Readable text did not reach color output");
          capture(engine, pixels, output / (name + "-captions.ppm"), TextLayer::Captions);
          require(scene_pixels != pixels.rgba, "Caption lanes did not reach color output");
          capture(engine, pixels, output / (name + "-text.ppm"), TextLayer::All);
        }
        const auto before_skip = engine.physics_.boxState(0);
        require(engine.renderer_.renderFrame({}) == FrameOutcome::Skipped,
                "Zero-size household render did not skip");
        require(engine.physics_.boxState(0).center == before_skip.center &&
                    engine.household_.page() == 1,
                "Skipped render changed household state");
        engine.window_.setSize(800, 600);
        engine.window_.pollEvents();
        tick(engine, {}, false);
        PlayerActionSnapshot escape;
        escape.menu = true;
        tick(engine, escape, false);
        tick(engine, escape, false);
        require(!engine.household_.readingDocument() && engine.window_.cursorCaptured(),
                "Reader Escape activated cursor release twice");
        tick(engine, {}, false);
        HouseholdDevelopmentInput mute;
        mute.mute = true;
        tick(engine, {}, false, mute);
        require(engine.audio_.muted(), "Neutral development mute was not applied");
        engine.household_.toggleRadio(0);
        require(engine.audio_.status("source-0") == CueStatus::Cancelled &&
                    engine.audio_.captions().ambience.text.empty(),
                "Radio-off frame retained its caption");
        engine.window_.requestClose();
        require(!engine.tick(), "Household close did not stop runtime");
      }
      // A new scene owns fresh bodies, velocities, reading and radio state.
      Engine fresh(config, diagnostics, false, AudioOutput::Silent);
      const auto state = fresh.physics_.boxState(0);
      require(state.center == document(maximum).household.boxes[0].center &&
                  state.linear_velocity.y == 0 && state.angular_velocity.y == 0 &&
                  !fresh.household_.readingDocument() && fresh.audio_.instance("source-0") == 1,
              "Fresh runtime retained previous household state");
    }

    config.level_path = output / "door-free.level.json";
    {
      Engine engine(config, diagnostics, false, AudioOutput::Silent);
      (void)engine.samplePlayerInput({});
      require(engine.household_.requestPickup(0, engine.player_.viewPose(1)),
              "Household pickup was not queued");
      tick(engine);
      require(engine.household_.heldBox() == 0, "Engine boundary did not acquire box");
      HouseholdDevelopmentInput pause;
      pause.pause = true;
      tick(engine, {}, false, pause);
      const auto held = engine.physics_.boxState(0);
      const auto active_time = engine.audio_.activeTime();
      tick(engine);
      require(engine.suspended_ && engine.household_.heldBox() == 0 &&
                  engine.physics_.boxState(0).center == held.center &&
                  engine.audio_.activeTime() == active_time,
              "Explicit suspension advanced or released held box");
      tick(engine, {}, false, pause);
      tick(engine, {}, false);
      engine.window_.setCursorCaptured(false);
      (void)engine.samplePlayerInput({});
      require(engine.household_.safetyReleaseOwed() && engine.household_.heldBox() == 0,
              "Zero-step cursor loss did not retain owed safety release");
      engine.window_.minimize();
      engine.window_.pollEvents();
      require(engine.window_.framebufferExtent().isZero(), "Household window did not minimize");
      {
        std::jthread wake([](std::stop_token stop) {
          while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            glfwPostEmptyEvent();
          }
        });
        tick(engine, {}, false);
      }
      require(engine.suspended_ && engine.household_.safetyReleaseOwed(),
              "Minimize erased owed safety release");
      engine.window_.restore();
      engine.window_.pollEvents();
      engine.window_.setCursorCaptured(true);
      tick(engine, {}, false);
      tick(engine);
      require(!engine.household_.heldBox() && !engine.household_.safetyReleaseOwed(),
              "Restore failed to discharge safety release before world step");
    }

    for (const auto& failure : std::array{
             std::array{"player-inner-body", "player inner body"},
             std::array{"box-body-1", "forced to fail after box: box-0"},
             std::array{"world-update", "Physics world update failed"},
             std::array{"changing_mesh_buffer", "Forced changing opaque allocation failure"},
             std::array{"changing_mesh_upload", "Forced changing opaque upload failure"}}) {
      const auto* stage = failure[0];
      const bool gpu = std::string_view(stage).starts_with("changing_");
      bool failed = false;
      {
        Failure inject(gpu ? "NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"
                           : "NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE", stage);
        try {
          Engine engine(config, diagnostics, false, AudioOutput::Silent);
          tick(engine);
        } catch (const std::runtime_error& error) {
          std::cout << "Household expected failure " << stage << ": " << error.what() << '\n';
          failed = std::string_view(error.what()).find(failure[1]) != std::string_view::npos;
        }
      }
      require(failed, "Household injected failure did not occur");
      Engine recovered(config, diagnostics, false, AudioOutput::Silent);
      tick(recovered);
    }
    require(diagnostics.errorCount() == 0, "Household runtime recorded Vulkan errors");
  }
};
#endif
