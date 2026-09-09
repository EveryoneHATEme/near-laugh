#include "editor/editor_application.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "core/audio/transmission.hpp"
#include "core/development/frame_capture.hpp"
#include "core/testing/test_controls.hpp"
#include "core/world/characters.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "editor/editor_loop.hpp"
#include "editor/editor_overlay.hpp"

namespace {
double auditionNow() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
std::filesystem::path requireEditorFile(const std::filesystem::path& path) {
  const std::filesystem::path resolved =
      std::filesystem::absolute(path).lexically_normal();
  if (!std::filesystem::is_regular_file(resolved)) {
    const auto text = resolved.u8string();
    throw std::runtime_error("Required editor resource is missing: " +
                             std::string(text.begin(), text.end()));
  }
  return resolved;
}

EditorRendererResources resolveEditorRendererResources(
    const std::filesystem::path& resource_root, FrameCapture* capture) {
  const std::filesystem::path root =
      std::filesystem::absolute(resource_root).lexically_normal();
  return {requireEditorFile(root / "shaders" / "prototype_scene_vertex.spv"),
          requireEditorFile(root / "shaders" / "prototype_scene_fragment.spv"),
          root, capture};
}
}  // namespace

EditorApplication::EditorApplication(
    std::filesystem::path resource_root,
    std::optional<std::filesystem::path> initial_level,
    ValidationDiagnostics& diagnostics, FrameCapture* capture)
    : validation_diagnostics_(diagnostics),
      resource_root_(
          std::filesystem::absolute(resource_root).lexically_normal()),
      caption_font_(std::make_shared<CaptionFont>(resource_root_)),
      window_(platform_, 1600, 900, "near-laugh level editor"),
      glfw_imgui_bridge_(window_, caption_font_),
      renderer_(window_, window_.framebufferExtent(),
                resolveEditorRendererResources(resource_root, capture),
                validation_diagnostics_) {
  if (initial_level) {
    static_cast<void>(document_.open(*initial_level));
    synchronizeDocumentResources();
  }
}

void EditorApplication::run() {
  while (tick()) {
  }
}

void EditorApplication::runCharacterSmoke(std::vector<std::string>& events,
                                          FrameCapture& capture) {
  const auto require = [](bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
  };
  struct Failure {
    explicit Failure(const char* stage) {
#ifdef _WIN32
      if (_putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", stage) != 0)
#else
      if (setenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", stage, 1) != 0)
#endif
        throw std::runtime_error("Could not configure character smoke failure");
    }
    ~Failure() {
#ifdef _WIN32
      static_cast<void>(_putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", ""));
#else
      static_cast<void>(unsetenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"));
#endif
    }
  };
  const auto count = [&](const char* event) {
    return std::count(events.begin(), events.end(), event);
  };
  const auto immutable = [&] {
    return std::array{count("editor.document-resources.replaced"),
                      count("editor.terrain-resources.replaced"),
                      count("character.resources.created"),
                      count("character.indices.uploaded"),
                      count("texture.created"),
                      count("lighting.created"),
                      count("world.mesh.uploaded")};
  };
  const auto rendered = [&] {
    const auto before = count("character.vertices.drawn");
    for (int i = 0; i < 3; ++i)
      require(tick(), "Editor character frame stopped");
    require(count("character.vertices.drawn") > before,
            "Editor did not submit retained character geometry");
  };
  require(scene_resources_installed_ && initial_character_frames_.size() == 4,
          "Editor did not prepare four initial actors");
  require(renderer_.validationEnabled(),
          "Editor character smoke requires Vulkan validation");
  const auto original = *document_.document();
  const auto frozen = initial_character_palettes_;
  for (int i = 0; i < 4; ++i) {
    if (i == 2) renderer_.requestSwapchainRecreation();
    require(tick(), "Editor character preview stopped");
    require(initial_character_palettes_ == frozen,
            "Editor played an initial route");
  }
  const auto original_resources = immutable();
  const auto original_uploads = count("character.vertices.uploaded");
  const auto actor_id =
      document_.characterIds(EditorCharacterKind::Actor).front();
  document_.select(actor_id);
  const EditorCharacterPreviewRequest clip_request{
      EditorCharacterPreviewMode::Clip, actor_id, 0, "interact"};
  require(startCharacterPreview(clip_request), "Clip snapshot could not start");
  character_preview_.advance(.4);
  require(character_preview_.pose() != frozen.front(),
          "Clip snapshot did not sample a changing pose");
  require(tick(), "Clip snapshot frame failed");
  require(initial_character_palettes_ == frozen &&
              *document_.document() == original,
          "Clip inspection changed authored data or other initial poses");
  character_preview_.pause();
  const auto paused_pose = character_preview_.pose();
  const auto paused_time = character_preview_.time();
  require(tick() && character_preview_.pose() == paused_pose,
          "Paused snapshot moved");
  // Capture the actual tick submission, with static collapsed UI and a fixed
  // close camera. CPU sampling alone cannot prove the snapshot reached the GPU.
  ui_.collapsePanelsForCapture(true);
  for (int i = 0; i < 8; ++i) camera_.update({.move_left = true}, .1);
  for (int i = 0; i < 23; ++i) camera_.update({.move_forward = true}, .1);
  for (int i = 0; i < 2; ++i) camera_.update({.move_down = true}, .1);
  const auto capture_frame = [&] {
    capture.requested = true;
    for (int i = 0; i < 8 && capture.requested; ++i)
      require(tick(), "Character capture frame stopped");
    require(!capture.requested && !capture.rgba.empty(),
            "Character GPU readback did not complete");
    return capture.rgba;
  };
  const auto first_pose_pixels = capture_frame();
  character_preview_.seek(.8);
  const auto sought_pixels = capture_frame();
  require(first_pose_pixels.size() == sought_pixels.size(),
          "Window extent changed between fixed-view character captures");
  const auto changed_bytes = std::inner_product(
      first_pose_pixels.begin(), first_pose_pixels.end(), sought_pixels.begin(),
      std::size_t{}, std::plus<>{}, std::not_equal_to<>{});
  require(changed_bytes > 100,
          "Seeking the snapshot did not change presented character pixels");
  require(capture_frame() == sought_pixels,
          "Paused snapshot did not retain its presented pose");
  character_preview_.seek(paused_time);
  require(capture_frame() == first_pose_pixels,
          "Seeking back did not restore the same presented pose");
  ui_.collapsePanelsForCapture(false);
  require(tick(), "Character panels did not restore after capture");
  window_.setSize(1280, 800);
  rendered();
  require(character_preview_.paused() &&
              character_preview_.pose() == paused_pose &&
              character_preview_.time() == paused_time,
          "Resize changed the paused snapshot");
  {
    const auto alternate = count("swapchain.surface_format.alternate.selected");
    Failure format("alternate_surface_format");
    renderer_.requestSwapchainRecreation();
    rendered();
    require(count("swapchain.surface_format.alternate.selected") > alternate,
            "Editor did not exercise alternate attachment format");
    require(character_preview_.pose() == paused_pose &&
                character_preview_.time() == paused_time,
            "Format recovery changed the paused snapshot");
  }
  renderer_.requestSwapchainRecreation();
  rendered();
  character_preview_.selectClip("walk");
  character_preview_.pause();  // Resume the same snapshot after recovery.
  const auto running_time = character_preview_.time();
  renderer_.requestSwapchainRecreation();
  rendered();
  require(character_preview_.active() && !character_preview_.paused() &&
              character_preview_.clip() == "walk" &&
              character_preview_.time() > running_time,
          "Presentation recovery restarted or stopped running playback");
  require(immutable() == original_resources &&
              count("character.vertices.uploaded") > original_uploads,
          "Pose playback/recovery rebuilt immutable scene resources");
  const auto source_id = document_.audioIds(EditorAudioKind::Source).front();
  require(startAudition(source_id, auditionNow(), AudioOutput::Silent),
          "Silent audition could not start");
  require(!character_preview_.active() && audition_.cues(),
          "Audition retained character snapshot");
  require(startCharacterPreview(clip_request),
          "Character snapshot could not replace audition");
  require(!audition_.cues(), "Character snapshot retained audition/captions");
  stopInspections();
  require(tick() && !character_preview_.active() && !audition_.cues(),
          "Stopped inspection restarted");
  document_.select(document_.characterIds(EditorCharacterKind::Route).front());
  require(startCharacterPreview({EditorCharacterPreviewMode::Route, actor_id}),
          "Route snapshot could not start");
  character_preview_.advance(1);
  require(tick() && *document_.document() == original,
          "Schematic route changed authored definitions");
  character_preview_.pause();
  const auto route_position = character_preview_.placement().position;
  const auto route_pose = character_preview_.pose();
  const auto route_segment = character_preview_.segment();
  const auto route_stage = character_preview_.stage();
  renderer_.requestSwapchainRecreation();
  rendered();
  require(character_preview_.paused() &&
              character_preview_.placement().position == route_position &&
              character_preview_.pose() == route_pose &&
              character_preview_.segment() == route_segment &&
              character_preview_.stage() == route_stage &&
              immutable() == original_resources,
          "Recovery changed the paused route state or rebuilt its resources");
  document_.select(actor_id);
  require(tick() && !character_preview_.active(),
          "Selection retained route snapshot");
  require(startCharacterPreview(clip_request), "Fresh clip start failed");
  window_.minimize();
  {
    std::jthread wake([] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      EditorGlfwBridge::postEmptyEvent();
    });
    require(tick() && !character_preview_.active(),
            "Minimize retained character snapshot");
  }
  const auto mark_id =
      document_.characterIds(EditorCharacterKind::Mark).front();
  auto minimized_mark =
      std::get<CharacterMarkDefinition>(*document_.object(mark_id));
  minimized_mark.yaw_degrees += 25;
  require(document_.replaceObject(mark_id, minimized_mark),
          "Minimized mark edit failed");
  const auto minimized_resources = immutable();
  {
    std::jthread wake([] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      EditorGlfwBridge::postEmptyEvent();
    });
    require(tick() && immutable() == minimized_resources,
            "Minimized edit attempted GPU replacement");
  }
  window_.restore();
  window_.pollEvents();
  require(tick() && !character_preview_.active(),
          "Restore restarted character snapshot");
  require(character_resources_current_ &&
              initial_character_frames_.front().yaw_degrees ==
                  minimized_mark.yaw_degrees,
          "Restore did not install the edit made while minimized");
  require(document_.undo(), "Minimized mark edit undo failed");
  synchronizeDocumentResources();
  rendered();
  struct Temporary {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("near-laugh-character-editor-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    Temporary() { std::filesystem::create_directory(path); }
    ~Temporary() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } temporary;
  require(startCharacterPreview(clip_request),
          "Clip start before refused Play failed");
  launchPlay({temporary.path / "absent.level.json", document_.launchEntry()});
  require(!game_process_.active() && !character_preview_.active(),
          "Refused Play retained character snapshot");
  require(tick() && !character_preview_.active(),
          "Refused Play replayed a pending snapshot");
  require(startCharacterPreview(clip_request),
          "Fresh snapshot after refused Play failed");
  const auto original_root = resource_root_;
  for (const auto directory : {"audio", "captions", "fonts"})
    std::filesystem::copy(resource_root_ / directory,
                          temporary.path / directory,
                          std::filesystem::copy_options::recursive);
  auto light = document_.document()->environment_light.point_lights.front();
  light.intensity = .8F;
  require(document_.replaceObject(document_.lightIds().front(), light),
          "Editor light edit failed");
  require(document_.document()->characters == original.characters,
          "Light edit changed routes");
  require(document_.saveAs(temporary.path / "characters.level.json"),
          "Editor character Save As failed");
  resource_root_ =
      temporary
          .path;  // All selected audio/font exists; mannequin alone is absent.
  synchronizeDocumentResources();
  require(!character_preview_.active(), "Document edit retained clip snapshot");
  require(initial_character_palettes_ == frozen &&
              initial_character_frames_.size() == 4,
          "Failed actor replacement discarded compatible pose storage");
  require(tick(), "Stale actor preview cannot render");
  launchPlay({*document_.path(), document_.launchEntry()});
  require(!game_process_.active(),
          "Missing selected mannequin created a child");
  require(std::any_of(document_.diagnostics().begin(),
                      document_.diagnostics().end(),
                      [](const auto& d) {
                        return d.message.find("actor '") != std::string::npos;
                      }),
          "Selected mannequin failure lost actor context");
  resource_root_ = original_root;
  require(document_.undo(), "Actor-bearing light undo failed");
  synchronizeDocumentResources();
  require(tick(), "Editor failed to restore valid actor assets");
  require(document_.document()->characters == original.characters,
          "Undo changed actor definitions");
  require(initial_character_palettes_ == frozen,
          "Editor recovery advanced initial animation");

  // A decoder failure also retains the old CPU/GPU set, with a visible stale
  // diagnostic. An edit/undo explicitly retries the corrected resources.
  std::filesystem::create_directory(temporary.path / "characters");
  std::ofstream(temporary.path / "characters/test-mannequin.glb")
      << "invalid glb";
  resource_root_ = temporary.path;
  require(document_.redo(), "Character decoder failure edit failed");
  synchronizeDocumentResources();
  require(
      !character_resources_current_ && initial_character_palettes_ == frozen,
      "Decoder failure installed partial character data");
  rendered();
  resource_root_ = original_root;
  require(document_.undo(), "Character decoder correction undo failed");
  synchronizeDocumentResources();
  rendered();

  const auto stale = [&] {
    return std::any_of(document_.diagnostics().begin(),
                       document_.diagnostics().end(), [](const auto& d) {
                         return d.message.find("Preview is stale") !=
                                std::string::npos;
                       });
  };
  for (const auto stage :
       {"character_index_upload", "character_slots", "shadow_image"}) {
    document_.select(actor_id);
    require(startCharacterPreview(clip_request),
            "Failure preview start failed");
    const auto old_enables = preview_point_light_enabled_;
    const auto old_assets = initial_character_assets_;
    const auto old_ids = initial_character_ids_;
    const auto installed = count("editor.document-resources.replaced");
    document_.select(document_.characterIds(EditorCharacterKind::Actor).back());
    require(document_.removeSelected(), "Candidate actor removal failed");
    document_.select(document_.lightIds().back());
    require(document_.removeSelected(), "Candidate light removal failed");
    {
      Failure failure(stage);
      synchronizeDocumentResources();
    }
    require(!character_preview_.active() && !character_resources_current_ &&
                scene_resources_installed_ && stale(),
            "Failed replacement was not stopped and labeled stale");
    require(count("editor.document-resources.replaced") == installed &&
                initial_character_palettes_ == frozen &&
                initial_character_assets_ == old_assets &&
                initial_character_ids_ == old_ids &&
                initial_character_frames_.size() == old_ids.size() &&
                preview_point_light_enabled_ == old_enables,
            "Failed replacement mixed old/new actors, lighting or palettes");
    require(!startCharacterPreview(clip_request),
            "Stale resources allowed a new snapshot");
    const auto retained = immutable();
    rendered();
    require(immutable() == retained && !character_preview_.active(),
            "Stale scene retried replacement or restarted playback implicitly");
    require(document_.undo() && document_.undo(), "Candidate undo failed");
    synchronizeDocumentResources();
    rendered();
    require(character_resources_current_ && !stale() &&
                initial_character_palettes_ == frozen &&
                preview_point_light_enabled_ == old_enables &&
                !character_preview_.active(),
            "Undo failed to install a coherent fresh preview");
  }

  // Successful replacement changes the exact actor set; no old frame span may
  // outlive its asset/palette owner or reappear after redo/document
  // replacement.
  document_.select(actor_id);
  require(startCharacterPreview(clip_request),
          "Replacement preview start failed");
  document_.select(document_.characterIds(EditorCharacterKind::Actor).back());
  require(document_.removeSelected(), "Fresh actor removal failed");
  synchronizeDocumentResources();
  rendered();
  require(initial_character_frames_.size() == 3 &&
              initial_character_assets_.size() == 3 &&
              !character_preview_.active() && character_resources_current_,
          "Successful replacement retained obsolete actor state");
  require(document_.undo(), "Fresh actor undo failed");
  synchronizeDocumentResources();
  rendered();
  require(initial_character_frames_.size() == 4 &&
              initial_character_palettes_ == frozen,
          "Undo did not restore four authored initial poses");
  require(document_.redo(), "Fresh actor redo failed");
  synchronizeDocumentResources();
  rendered();
  require(initial_character_frames_.size() == 3 && !character_preview_.active(),
          "Redo retained an old snapshot or actor");
  require(document_.undo(), "Final actor undo failed");
  synchronizeDocumentResources();
  document_.select(actor_id);
  require(startCharacterPreview(clip_request), "Document preview start failed");
  require(
      document_.open(resource_root_ / "levels/interior-lighting.level.json"),
      "Empty-character document replacement failed");
  synchronizeDocumentResources();
  require(tick() && !character_preview_.active() &&
              initial_character_frames_.empty() &&
              initial_character_assets_.empty() &&
              initial_character_ids_.empty() &&
              initial_character_palettes_.empty(),
          "Document replacement retained snapshot or borrowed character data");
  require(document_.open(resource_root_ /
                         "levels/scripted-characters-four.level.json"),
          "Four-character document restoration failed");
  synchronizeDocumentResources();
  rendered();
  const auto restored_actor =
      document_.characterIds(EditorCharacterKind::Actor).front();
  document_.select(restored_actor);
  require(startCharacterPreview(
              {EditorCharacterPreviewMode::Clip, restored_actor, 0, "walk"}),
          "Shutdown preview start failed");
  rendered();
  document_.requestExit();
  require(!tick() && !character_preview_.active() && !audition_.cues(),
          "Exit retained active inspection state");
}

void EditorApplication::runSmoke(const std::filesystem::path& valid_level) {
  struct FrameEventLog {
    std::vector<std::string> events;
    FrameEventLog() { setLifecycleLog(&events); }
    ~FrameEventLog() { setLifecycleLog(nullptr); }
  } frame_log;
  if (!document_.document()) {
    if (!document_.open(valid_level)) {
      throw std::runtime_error(
          "Editor smoke could not open the packaged level");
    }
    synchronizeDocumentResources();
  }
  if (!scene_resources_installed_) {
    throw std::runtime_error(
        "Editor smoke did not install scene resources for the packaged level");
  }
  for (int frame = 0; frame < 2 && tick(); ++frame) {
  }

  struct SmokeDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("near_laugh_editor_smoke_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    SmokeDirectory() { std::filesystem::create_directory(path); }
    ~SmokeDirectory() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } temporary;
  const auto require = [](bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
  };
  const auto preview = [&] {
    require(tick(), "Editor stopped during editing smoke");
    require(scene_resources_installed_ &&
                rendered_document_revision_ == document_.revision(),
            "Edited scene resources were not installed");
    require(std::none_of(
                document_.diagnostics().begin(), document_.diagnostics().end(),
                [](const auto& d) {
                  return d.category == LevelDiagnosticCategory::Filesystem;
                }),
            "Editor reported a preview resource failure");
    require(
        preview_point_light_enabled_ ==
            initialPointLightEnabled(document_.document()->environment_light),
        "Editor preview lost authored point-light state");
  };
  require(document_.saveAs(temporary.path / "working.level.json"),
          "Editor smoke Save As failed");
  const LevelDocument original = *document_.document();
  document_.select(document_.solidIds().back());
  require(document_.duplicateSelected(), "Editor smoke duplicate failed");
  preview();
  require(document_.removeSelected(), "Editor smoke remove failed");
  preview();
  require(document_.undo(), "Editor smoke undo failed");
  preview();
  require(document_.undo(), "Editor smoke duplicate undo failed");
  preview();
  require(*document_.document() == original && !document_.dirty(),
          "Editor smoke undo did not restore saved content");

  document_.select(document_.switchIds().front());
  require(document_.removeSelected(), "Editor smoke switch removal failed");
  preview();
  require(document_.addLightSwitch(), "Editor smoke switch creation failed");
  require(document_.duplicateSelected(),
          "Editor smoke switch duplicate failed");
  preview();
  require(document_.undo(), "Editor smoke switch duplicate undo failed");
  preview();
  require(document_.undo() && document_.undo(),
          "Editor smoke switch restoration failed");
  preview();
  const auto switch_id = document_.switchIds().front();
  auto light_switch = document_.document()->light_switches.front();
  light_switch.yaw_degrees += 25;
  for (const auto slot : {0U, 1U}) {
    for (const bool on : {false, true}) {
      auto light = document_.document()->environment_light.point_lights[slot];
      light_switch.light_id = light.id;
      light.initially_on = on;
      require(document_.replaceObject(document_.lightIds()[slot], light) ||
                  document_.document()->environment_light.point_lights[slot] ==
                      light,
              "Editor smoke initial light property failed");
      require(document_.replaceObject(switch_id, light_switch) ||
                  document_.document()->light_switches.front() == light_switch,
              "Editor smoke switch properties failed");
      preview();
    }
  }
  preview();
  require(document_.save(), "Editor smoke switch save failed");
  // Exercise every point/spot combination. Normal frames restore the
  // document's initial-state preview without mutating authored values.
  for (int mask = 0; mask < 8; ++mask) {
    preview();
    FrameRequest frame;
    frame.framebuffer = window_.framebufferExtent();
    frame.camera = camera_.frame(16.0F / 9.0F);
    const std::array<std::uint8_t, 2> enables{
        static_cast<std::uint8_t>((mask & 1) != 0),
        static_cast<std::uint8_t>((mask & 2) != 0)};
    frame.point_light_enabled = enables;
    if (mask & 4)
      frame.spot_light = {
          {0, 2, 4, 10}, {0, 0, -1, 0.95F}, {1, 1, 1, 1}, {0.85F, 1, 0, 0}};
    static_cast<void>(renderer_.renderFrame(frame));
  }
  const auto before_terrain = *document_.document();

  EditorTerrainBrush brush;
  brush.radius = 3;
  brush.strength = 0.01F;
  require(document_.setTerrainBrush(brush),
          "Editor smoke brush settings failed");
  const auto uploads = renderer_.terrainReplacementCount();
  document_.beginTerrainStroke({{10, 0, 10}});
  for (int i = 1; i <= 20; ++i)
    document_.extendTerrainStroke({{10 + i * 0.25F, 0, 10}});
  preview();
  require(
      renderer_.terrainReplacementCount() == uploads + 1,
      "Same-frame terrain stamps did not coalesce into one buffer replacement");
  preview();
  require(renderer_.terrainReplacementCount() == uploads + 1,
          "Unchanged frame unnecessarily rebuilt the terrain");
  document_.extendTerrainStroke({{16, 0, 10}});
  window_.setSize(1280, 720);
  renderer_.requestSwapchainRecreation();
  preview();
  require(document_.terrainStrokeActive(),
          "Resize unexpectedly lost the stroke");
  require(document_.finishTerrainStroke(), "Editor smoke stroke commit failed");
  preview();
  const auto sculpted = *document_.document();
  require(document_.undo(), "Editor smoke terrain undo failed");
  preview();
  require(*document_.document() == before_terrain && !document_.dirty(),
          "Terrain undo did not restore the saved document");
  require(document_.redo(), "Editor smoke terrain redo failed");
  preview();
  require(*document_.document() == sculpted, "Terrain redo changed samples");

  brush.mode = EditorBrushMode::Smooth;
  require(document_.setTerrainBrush(brush),
          "Editor smoke smooth settings failed");
  document_.beginTerrainStroke({{12, 0, 10}});
  preview();
  document_.extendTerrainStroke({{15, 0, 10}});
  require(document_.finishTerrainStroke(),
          "Editor smoke smoothing had no effect");
  preview();
  require(document_.save(), "Editor smoke sculpted save failed");
  const auto saved_terrain = *document_.document();
  require(document_.open(temporary.path / "working.level.json"),
          "Editor smoke sculpted reload failed");
  require(*document_.document() == saved_terrain,
          "Editor smoke sculpted round trip changed content");
  preview();

  auto spawn = original.entries.front();
  spawn.pose.foot_position.y += 2;
  require(document_.replaceObject(editor_spawn, spawn),
          "Editor smoke invalid spawn edit failed");
  document_.select(editor_spawn);
  preview();
  require(!document_.valid() && !document_.save(),
          "Editor smoke saved an invalid level");
  document_.requestClose();
  preview();
  require(document_.resolvePending(EditorPendingDecision::Cancel),
          "Editor smoke cancel close failed");
  require(document_.undo(), "Editor smoke invalid edit undo failed");
  preview();

  auto light = original.environment_light.point_lights[0];
  light.position.x += 0.5F;
  light.intensity += 0.25F;
  document_.select(document_.lightIds().front());
  require(document_.replaceObject(document_.lightIds().front(), light),
          "Editor smoke light edit failed");
  preview();
  auto prop = original.props.front();
  prop.yaw_degrees += 20;
  prop.uniform_scale *= 0.9F;
  document_.select(editor_prop);
  require(document_.replaceObject(editor_prop, prop),
          "Editor smoke prop edit failed");
  preview();
  require(document_.save(), "Editor smoke edited save failed");
  const auto edited = *document_.document();
  require(document_.open(temporary.path / "working.level.json"),
          "Editor smoke edited reload failed");
  require(*document_.document() == edited,
          "Editor smoke save/reload changed content");
  document_.select(editor_prop);
  preview();

  document_.requestNewInterior();
  require(document_.valid() && !document_.document()->terrain,
          "New interior did not create a valid terrain-free document");
  preview();
  document_.select(document_.solidIds().front());
  require(document_.removeSelected(), "Interior floor removal failed");
  require(!document_.valid(), "Empty interior should remain invalid");
  preview();  // Entries and UI survive an entirely empty geometry stream.

  const auto apartment = loadLevelDocument(valid_level.parent_path() /
                                           "apartment-stairs.level.json");
  require(static_cast<bool>(apartment),
          "Packaged apartment could not be decoded");
#if defined(_WIN32)
  require(_putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE",
                    "world_mesh_upload") == 0,
          "Could not configure replacement failure");
#else
  require(setenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", "world_mesh_upload",
                 1) == 0,
          "Could not configure replacement failure");
#endif
  bool replacement_failed = false;
  try {
    renderer_.replaceDocument(*apartment.document);
  } catch (const std::runtime_error&) {
    replacement_failed = true;
  }
#if defined(_WIN32)
  static_cast<void>(_putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", ""));
#else
  static_cast<void>(unsetenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"));
#endif
  require(replacement_failed, "Expected interior mesh replacement failure");
  preview();  // Previously installed empty-world resources remain usable.
  require(
      document_.open(valid_level.parent_path() / "apartment-stairs.level.json"),
      "Editor could not open the apartment");
  preview();
  require(document_.selectLaunchEntry("lower-landing"),
          "Alternate editor entry missing");
  const auto furnished = *document_.document();
  require(!document_.doorIds().empty() && document_.propIds().size() >= 4,
          "Furnished scene is missing doors or selected props");
  document_.select(document_.doorIds().front());
  const auto door_id = document_.selection();
  auto door = std::get<DoorDefinition>(*document_.object(door_id));
  door.initially_open = true;
  require(document_.replaceObject(door_id, door), "Door preview edit failed");
  preview();
  require(document_.undo(), "Door preview undo failed");
  preview();
  door.initially_open = false;
  door.initially_locked = true;
  require(document_.replaceObject(door_id, door), "Locked preview edit failed");
  preview();
  require(document_.undo(), "Locked preview undo failed");
  document_.select(document_.propIds().front());
  require(document_.duplicateSelected(), "Shared model duplication failed");
  const auto prop_id = document_.selection();
  preview();
  require(document_.removeSelected(), "Shared model deletion failed");
  preview();
  require(document_.undo() && document_.selection() == prop_id,
          "Shared model restoration lost identity");
  preview();
  require(document_.undo(), "Shared model duplication undo failed");
  preview();
  require(*document_.document() == furnished,
          "Content history lost authored state");
  require(document_.open(valid_level.parent_path() /
                         "interior-lighting-capacity.level.json"),
          "Editor could not open the lighting capacity fixture");
  preview();
  const auto lighting_original = *document_.document();
  const auto lighting_enables = preview_point_light_enabled_;
  document_.select(document_.lightIds().front());
  require(document_.duplicateSelected() == false,
          "Editor exceeded the eight-light limit");
  auto changed_light = lighting_original.environment_light.point_lights.front();
  changed_light.initially_on = false;
  require(document_.replaceObject(document_.lightIds().front(), changed_light),
          "Lighting initial-state edit failed");
#if defined(_WIN32)
  static_cast<void>(
      _putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", "shadow_image"));
#else
  static_cast<void>(
      setenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", "shadow_image", 1));
#endif
  require(tick(), "Editor stopped during failed shadow replacement");
#if defined(_WIN32)
  static_cast<void>(_putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", ""));
#else
  static_cast<void>(unsetenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"));
#endif
  require(preview_point_light_enabled_ == lighting_enables &&
              scene_resources_installed_,
          "Failed shadow allocation changed the last usable preview state");
  require(std::any_of(document_.diagnostics().begin(),
                      document_.diagnostics().end(),
                      [](const auto& diagnostic) {
                        return diagnostic.message.find("Preview is stale") !=
                               std::string::npos;
                      }),
          "Failed shadow replacement did not report a stale preview");
  require(document_.undo(), "Shadow replacement undo failed");
  preview();
  require(*document_.document() == lighting_original,
          "Shadow recovery lost authored state");
  document_.select(document_.lightIds().back());
  require(document_.removeSelected(), "Lighting capacity removal failed");
  preview();
  document_.select(document_.lightIds().front());
  require(document_.duplicateSelected(), "Shadow-light duplication failed");
  require(!document_.valid() && !document_.save(),
          "Fifth shadow light passed Save preflight");
  require(tick() && preview_point_light_enabled_.size() == 7,
          "Invalid shadow budget did not retain its coherent preview");
  require(document_.undo() && document_.undo(), "Shadow budget repair failed");
  preview();
  require(document_.saveAs(temporary.path /
                           std::filesystem::path(u8"Свет и двери.json")),
          "Lighting Unicode Save As failed");
#if defined(_WIN32)
  static_cast<void>(
      _putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", "shadow_image"));
#else
  static_cast<void>(
      setenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", "shadow_image", 1));
#endif
  launchPlay({*document_.path(), document_.launchEntry()});
#if defined(_WIN32)
  static_cast<void>(_putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", ""));
#else
  static_cast<void>(unsetenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"));
#endif
  require(
      !game_process_.active() &&
          std::any_of(document_.diagnostics().begin(),
                      document_.diagnostics().end(),
                      [](const auto& diagnostic) {
                        return diagnostic.message.find("shadow_image") !=
                               std::string::npos;
                      }),
      "Failed lighting Play preflight launched a game or lost its diagnostic");
  require(document_.open(*document_.path()),
          "Lighting reload after refused Play failed");
  preview();
  document_.requestNewInterior();
  require(document_.document() && document_.switchIds().empty(),
          "Empty lighting preview setup failed");
  while (!document_.lightIds().empty()) {
    document_.select(document_.lightIds().back());
    require(document_.removeSelected(),
            "Empty lighting preview deletion failed");
    preview();
  }
  require(preview_point_light_enabled_.empty(),
          "Empty editor scene retained a light enable");
  require(document_.undo(), "Empty lighting preview undo failed");
  preview();
  require(
      document_.open(valid_level.parent_path() / "audio-captions.level.json"),
      "Editor could not open the audio fixture");
  preview();
  const auto audio_original = *document_.document();
  const auto audition_source = document_.audioIds(EditorAudioKind::Source)[3];
  document_.select(audition_source);
  require(audition_.start(document_, audition_source, resource_root_,
                          *caption_font_, auditionNow(), AudioOutput::Silent),
          "Editor smoke could not start silent audition");
  preview();
  require(
      audition_.cues() && !audition_.cues()->captions().foreground.text.empty(),
      "Editor audition lost Russian captions");
  require(!document_.dirty() && *document_.document() == audio_original,
          "Audition dirtied the document");
  require(document_.duplicateSelected(), "Audio source duplication failed");
  preview();
  require(!audition_.cues(), "Audio edit did not stop audition");
  require(document_.undo(), "Audio duplication undo failed");
  preview();
  require(audition_.start(document_, audition_source, resource_root_,
                          *caption_font_, auditionNow(), AudioOutput::Silent),
          "Editor smoke could not restart audition");
  window_.minimize();
  {
    std::jthread wake([] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      EditorGlfwBridge::postEmptyEvent();
    });
    require(tick() && !audition_.cues(), "Editor minimize retained audition");
  }
  window_.restore();
  window_.pollEvents();
  preview();
  require(!audition_.cues(), "Editor restore automatically replayed audition");
  require(document_.saveAs(temporary.path /
                           std::filesystem::path(u8"Звуки и подписи.json")),
          "Audio Unicode Save As failed");
  const auto packaged_root = resource_root_;
  resource_root_ =
      temporary.path;  // Selected audio files are deliberately absent here.
  launchPlay({*document_.path(), document_.launchEntry()});
  resource_root_ = packaged_root;
  require(!game_process_.active() && !document_.diagnostics().empty(),
          "Broken audio Play preflight launched a process");
  require(document_.open(*document_.path()),
          "Audio reload failed after refused Play");
  preview();
  renderer_.validateSceneAssets(*document_.document());
  window_.setSize(1200, 800);
  renderer_.requestSwapchainRecreation();
  preview();

  document_.requestOpen(valid_level);
  static_cast<void>(tick());
  if (!scene_resources_installed_) {
    throw std::runtime_error(
        "Editor smoke did not replace scene resources for a valid level");
  }
  document_.requestOpen(valid_level.parent_path() / "missing.level.json");
  static_cast<void>(tick());

  document_.select(editor_prop);
  window_.setSize(1280, 720);
  brush.mode = EditorBrushMode::Lower;
  require(document_.setTerrainBrush(brush),
          "Editor smoke lower settings failed");
  document_.beginTerrainStroke({{22, 0, 22}});
  renderer_.requestSwapchainRecreation();
  static_cast<void>(tick());

  window_.minimize();
  std::thread wake_wait([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EditorGlfwBridge::postEmptyEvent();
  });
  static_cast<void>(tick());
  wake_wait.join();
  window_.restore();
  EditorGlfwBridge::postEmptyEvent();
  static_cast<void>(tick());
  require(document_.selection() == editor_prop && scene_resources_installed_,
          "Editor recovery lost the selection or scene resources");

  document_.requestExit();
  require(document_.pendingAction().kind == EditorPendingActionKind::Exit,
          "Sculpted exit did not request an unsaved decision");
  require(document_.resolvePending(EditorPendingDecision::Discard),
          "Editor smoke sculpted discard failed");
  static_cast<void>(tick());
  bool in_frame = false;
  bool ui_drawn = false;
  std::size_t completed_frames = 0;
  for (const auto& event : frame_log.events) {
    if (event == "editor.frame.begin") {
      require(!in_frame, "Editor began a frame before closing its predecessor");
      in_frame = true;
      ui_drawn = false;
    } else if (event == "editor.ui.drawn") {
      require(in_frame && !ui_drawn, "Editor must draw UI once in each frame");
      ui_drawn = true;
    } else if (event.ends_with(".mesh.drawn")) {
      require(in_frame && !ui_drawn,
              "Editor must draw scene geometry before UI");
    } else if (event == "editor.frame.end") {
      require(in_frame && ui_drawn, "Editor frame ended without UI");
      in_frame = false;
      ++completed_frames;
    }
  }
  require(!in_frame && completed_frames > 0,
          "Editor smoke recorded no complete frames");
}

bool EditorApplication::tick() {
  game_process_.poll();
  window_.pollEvents();
  if (window_.shouldClose()) {
    window_.cancelCloseRequest();
    document_.requestExit();
  }

  const FramebufferExtent framebuffer = window_.framebufferExtent();
  switch (decideEditorLoopAction(document_.exitRequested(), framebuffer)) {
    case EditorLoopAction::Exit:
      stopInspections();
      return false;
    case EditorLoopAction::WaitForEvents:
      stopInspections();
      static_cast<void>(document_.finishTerrainStroke());
      window_.waitEvents();
      frame_clock_.reset();
      return !document_.exitRequested();
    case EditorLoopAction::Render:
      break;
  }

  renderer_.beginUiFrame();
  glfw_imgui_bridge_.beginFrame();
  updateNavigation(glfw_imgui_bridge_.captureIntent());
  const CameraFrame camera =
      camera_.frame(static_cast<float>(framebuffer.width) /
                    static_cast<float>(framebuffer.height));
  ui_.draw(document_, game_process_.active(), game_process_.status());
  const bool play_attempt = ui_.takePlayAttempt();
  if (play_attempt) stopInspections();
  if (auto launch = ui_.takeLaunchRequest()) {
    launchPlay(*launch);
  }
  const auto placement_hit =
      ui_.updateViewport(document_, camera, window_.cursorCaptured());
  synchronizeDocumentResources();
  const double now = auditionNow();
  const bool can_inspect = !play_attempt && document_.pendingAction().kind ==
                                                EditorPendingActionKind::None;
  if (!can_inspect) stopInspections();
  updateAudition(now, can_inspect);
  updateCharacterPreview(now, can_inspect);
  renderer_.drawOverlays(buildEditorOverlay(
      document_, camera, placement_hit,
      ui_.sculpting() ? &document_.terrainBrush() : nullptr));
  renderer_.drawOverlayLabels(
      buildEditorCharacterOverlayLabels(document_, camera));
  ui_.finishFrame();

  FrameRequest frame;
  frame.framebuffer = framebuffer;
  frame.framebuffer_resized = window_.consumeFramebufferResize();
  frame.camera = camera;
  frame.point_light_enabled = preview_point_light_enabled_;
  auto character_frames = initial_character_frames_;
  if (character_preview_.active()) {
    const auto found =
        std::find(initial_character_ids_.begin(), initial_character_ids_.end(),
                  character_preview_.actor());
    if (found != initial_character_ids_.end()) {
      auto& posed = character_frames[static_cast<std::size_t>(
          found - initial_character_ids_.begin())];
      posed.joint_globals = character_preview_.pose();
      posed.position = character_preview_.placement().position;
      posed.yaw_degrees = character_preview_.placement().yaw_degrees;
    }
  }
  frame.characters = character_frames;
  const FrameOutcome outcome = renderer_.renderFrame(frame);
  return editorContinuesAfter(outcome) && !document_.exitRequested();
}

void EditorApplication::updateAudition(double now, bool allow_start) {
  const auto position = camera_.position();
  const float yaw = camera_.yawDegrees() * std::numbers::pi_v<float> / 180;
  const float pitch = camera_.pitchDegrees() * std::numbers::pi_v<float> / 180;
  const WorldPosition listener{position.x, position.y, position.z};
  audition_.update(document_, listener,
                   {std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                    std::sin(yaw) * std::cos(pitch)},
                   now);
  EditorAuditionView view;
  view.warning = audition_.error();
  if (auto* cues = audition_.cues()) {
    view.active = true;
    view.muted = cues->muted();
    view.paused = cues->suspended();
    view.source = audition_.source();
    view.captions = cues->captions();
    view.gain = cues->effectiveGain(view.source);
    view.warning = cues->playback().warning();
    view.listener_room =
        audioRoomAt(cues->definitions(), listener).value_or("outside");
    for (const auto& source : cues->definitions().sources)
      if (source.id == view.source)
        view.source_room = audioRoomAt(cues->definitions(), source.position)
                               .value_or("outside");
  }
  const auto selected = document_.object(document_.selection());
  const bool can_start =
      allow_start && selected &&
      std::holds_alternative<AudioSourceDefinition>(*selected) &&
      document_.valid();
  switch (ui_.drawAudition(view, can_start)) {
    case EditorAuditionAction::Start:
      static_cast<void>(startAudition(document_.selection(), now));
      break;
    case EditorAuditionAction::Stop:
      audition_.stop();
      break;
    case EditorAuditionAction::Mute:
      audition_.mute();
      break;
    case EditorAuditionAction::Pause:
      audition_.pause(now);
      break;
    case EditorAuditionAction::None:
      break;
  }
}

void EditorApplication::launchPlay(const EditorLaunchRequest& launch) {
  stopInspections();
  try {
    const auto saved = loadEditorPlayDocument(document_, launch);
    renderer_.validateSceneAssets(saved);
    static_cast<void>(launchEditorPlay(document_, launch, resource_root_,
                                       editorGameExecutable(), game_process_));
  } catch (const std::exception& error) {
    document_.reportResourceError(
        std::string("Play asset validation failed: ") + error.what());
  }
}

void EditorApplication::stopInspections() {
  audition_.stop();
  character_preview_.stop();
  character_preview_time_.reset();
}

bool EditorApplication::startAudition(EditorObjectId source, double now,
                                      AudioOutput output) {
  character_preview_.stop();
  character_preview_time_.reset();
  return audition_.start(document_, source, resource_root_, *caption_font_, now,
                         output);
}

bool EditorApplication::startCharacterPreview(
    const EditorCharacterPreviewRequest& request) {
  audition_.stop();
  character_preview_.stop();
  character_preview_time_.reset();
  const auto found = std::find(initial_character_ids_.begin(),
                               initial_character_ids_.end(), request.actor);
  const auto asset =
      character_resources_current_ && found != initial_character_ids_.end()
          ? initial_character_assets_[static_cast<std::size_t>(
                found - initial_character_ids_.begin())]
          : std::shared_ptr<const CharacterAsset>{};
  return character_preview_.start(document_, request, asset);
}

void EditorApplication::updateCharacterPreview(double now, bool can_start) {
  character_preview_.synchronize(document_);
  if (character_preview_.active() && character_preview_time_)
    character_preview_.advance(std::max(0., now - *character_preview_time_));
  character_preview_time_ = now;
  if (auto request =
          ui_.drawCharacterPreview(document_, character_preview_,
                                   can_start && character_resources_current_)) {
    static_cast<void>(startCharacterPreview(*request));
    character_preview_time_ = now;
  }
}

void EditorApplication::updateNavigation(EditorUiCaptureIntent capture) {
  const PhysicalInputSnapshot& physical = window_.input();
  if (window_.cursorCaptured() && physical.isKeyDown(PhysicalKey::Escape)) {
    window_.setCursorCaptured(false);
    frame_clock_.reset();
  } else if (!window_.cursorCaptured() && !capture.pointer &&
             physical.isMouseButtonDown(PhysicalMouseButton::Right)) {
    window_.setCursorCaptured(true);
    frame_clock_.reset();
  }

  const bool navigation_active = window_.cursorCaptured();
  camera_.update(editorNavigationInput(physical, navigation_active, capture),
                 frame_clock_.sample(EditorFrameClock::Clock::now()));
}

void EditorApplication::synchronizeDocumentResources() {
  character_preview_.synchronize(document_);
  if (rendered_document_revision_ == document_.revision()) {
    return;
  }
  try {
    // Allocate all CPU state before installing the matching GPU candidate.
    auto point_light_enabled =
        document_.document()
            ? initialPointLightEnabled(document_.document()->environment_light)
            : std::vector<std::uint8_t>{};
    if (document_.document()) {
      if (scene_resources_installed_ &&
          rendered_object_revision_ == document_.objectRevision())
        renderer_.replaceTerrain(*document_.document());
      else {
        const auto& definitions = document_.document()->characters;
        LevelCharacters renderable;
        std::vector<std::size_t> actor_indices;
        for (std::size_t i = 0; i < definitions.actors.size(); ++i) {
          const auto& actor = definitions.actors[i];
          if (!findCharacterModel(actor.model) ||
              !findCharacterMark(definitions, actor.initial_mark))
            continue;
          renderable.actors.push_back(actor);
          actor_indices.push_back(i);
        }
        auto assets = prepareCharacterAssets(resource_root_, renderable);
        std::vector<EditorObjectId> actor_ids;
        for (const auto index : actor_indices)
          actor_ids.push_back(
              document_.characterIds(EditorCharacterKind::Actor)[index]);
        std::vector<CharacterRenderInstance> instances;
        std::vector<CharacterPose> palettes;
        std::vector<CharacterPoseFrame> frames;
        palettes.reserve(assets.size());
        for (std::size_t i = 0; i < assets.size(); ++i) {
          const auto& actor = definitions.actors[actor_indices[i]];
          const auto mark = std::find_if(
              definitions.marks.begin(), definitions.marks.end(),
              [&](const auto& m) { return m.id == actor.initial_mark; });
          if (mark == definitions.marks.end())
            throw std::runtime_error("actor '" + actor.id +
                                     "': initial mark is missing");
          instances.push_back({static_cast<std::uint32_t>(i), assets[i]});
          palettes.push_back(CharacterPlayback(assets[i]).pose());
          const auto p = mark->feet_position;
          frames.push_back({static_cast<std::uint32_t>(i),
                            assets[i]->skeleton_identity,
                            palettes.back(),
                            {p.x, p.y, p.z},
                            mark->yaw_degrees});
        }
        renderer_.replaceDocument(*document_.document(), instances);
        // Move storage only after the renderer commits its selected resources.
        initial_character_palettes_ = std::move(palettes);
        initial_character_frames_ = std::move(frames);
        initial_character_assets_ = std::move(assets);
        initial_character_ids_ = std::move(actor_ids);
      }
    } else {
      renderer_.clearDocument();
      initial_character_frames_.clear();
      initial_character_palettes_.clear();
      initial_character_assets_.clear();
      initial_character_ids_.clear();
    }
    scene_resources_installed_ = document_.document().has_value();
    character_resources_current_ = scene_resources_installed_;
    preview_point_light_enabled_ = std::move(point_light_enabled);
    rendered_document_revision_ = document_.revision();
    rendered_object_revision_ = document_.objectRevision();
  } catch (const std::exception& error) {
    character_resources_current_ = false;
    character_preview_.stop();
    // Replacement is transactional: retain the last usable preview on failure.
    rendered_document_revision_ = document_.revision();
    document_.reportResourceError(
        std::string("Preview is stale; scene resource replacement failed: ") +
        error.what());
  }
}
