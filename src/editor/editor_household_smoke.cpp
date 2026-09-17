#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "core/development/frame_capture.hpp"
#include "core/world/door.hpp"
#include "core/world/household.hpp"
#include "editor/editor_application.hpp"

namespace {
void requireHousehold(bool condition, const std::string& message) {
  if (!condition)
    throw std::runtime_error("Editor household smoke: " + message);
}

class HouseholdFailure {
 public:
  explicit HouseholdFailure(const char* stage) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE") == 0 && value)
      previous_ = value;
    std::free(value);
#else
    if (const auto* value =
            std::getenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"))
      previous_ = value;
#endif
    requireHousehold(set(stage), "cannot set Vulkan failure control");
  }
  ~HouseholdFailure() {
    static_cast<void>(set(previous_ ? previous_->c_str() : nullptr));
  }

 private:
  static bool set(const char* value) {
#ifdef _WIN32
    return _putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE",
                     value ? value : "") == 0;
#else
    return value
               ? setenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", value, 1) == 0
               : unsetenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE") == 0;
#endif
  }
  std::optional<std::string> previous_;
};

std::size_t changedPixels(const std::vector<std::uint8_t>& before,
                          const std::vector<std::uint8_t>& after) {
  requireHousehold(before.size() == after.size(),
                   "capture extent changed during comparison");
  std::size_t count = 0;
  for (std::size_t pixel = 0; pixel < before.size(); pixel += 4)
    if (before[pixel] != after[pixel] ||
        before[pixel + 1] != after[pixel + 1] ||
        before[pixel + 2] != after[pixel + 2])
      ++count;
  return count;
}

// Capture the actual reader panel, excluding status text, selection outlines
// and window decorations. This checks that page glyphs reached the ImGui GPU draw.
std::vector<std::uint8_t> readerPixels(const FrameCapture& capture) {
  const ImGuiWindow* canvas = nullptr;
  for (const auto* window : ImGui::GetCurrentContext()->Windows)
    if (window->Active && window->ParentWindow &&
        std::string_view(window->ParentWindow->Name) == "Readable preview" &&
        std::string_view(window->Name).find("Reader canvas") !=
            std::string_view::npos)
      canvas = window;
  requireHousehold(canvas != nullptr, "reader canvas was not submitted");
  // The preview's Dummy(panel size) records these current-frame content bounds.
  // ClipRect also includes unused child space where the parent's resize grip
  // can change with focus after a scene-only submission.
  ImRect panel(canvas->DC.CursorStartPos, canvas->DC.CursorMaxPos);
  panel.ClipWith(canvas->ClipRect);
  const auto display = ImGui::GetIO().DisplaySize;
  const float sx = capture.width / display.x, sy = capture.height / display.y;
  const unsigned left = static_cast<unsigned>(
      std::clamp(panel.Min.x * sx, 0.F, float(capture.width)));
  const unsigned right = static_cast<unsigned>(
      std::clamp(panel.Max.x * sx, 0.F, float(capture.width)));
  const unsigned top = static_cast<unsigned>(
      std::clamp(panel.Min.y * sy, 0.F, float(capture.height)));
  const unsigned bottom = static_cast<unsigned>(
      std::clamp(panel.Max.y * sy, 0.F, float(capture.height)));
  requireHousehold(right > left && bottom > top,
                   "reader canvas has no captured pixels");
  std::vector<std::uint8_t> pixels;
  pixels.reserve(std::size_t(right - left) * (bottom - top) * 4);
  for (unsigned y = top; y < bottom; ++y) {
    const auto begin =
        capture.rgba.begin() + (std::size_t(y) * capture.width + left) * 4;
    pixels.insert(pixels.end(), begin, begin + (right - left) * 4);
  }
  return pixels;
}

LevelDocument capacityDocument(const LevelDocument& original) {
  LevelDocument level = original;
  level.terrain.reset();
  auto floor = level.solids.front();
  floor.center = {0, -.25F, 0};
  floor.half_extent = {35, .25F, 35};
  level.solids = {floor};
  level.entries = {{"smoke-entry", {{10, 0, 10}, -90}}};
  level.default_entry = "smoke-entry";
  level.characters = {};
  level.light_switches.clear();
  level.doors.clear();
  level.props.clear();
  level.audio = {};
  level.audio.cues.push_back(
      {"radio", "radio", "radio", AudioCueKind::Ambience, true, true});
  level.household = {};
  for (unsigned i = 0; i < 32; ++i) {
    DoorDefinition door;
    door.id = "smoke-door-" + std::to_string(i);
    door.hinge_position = {-24.F + 6 * (i % 8), 0, -24.F + 6 * (i / 8)};
    level.doors.push_back(door);
    level.household.documents.push_back(
        {"smoke-note-" + std::to_string(i),
         {1.F + .4F * (i % 8), .004F, 3.F + .5F * (i / 8)},
         0,
         "Ёж и коробки",
         {"Контрольная страница редактора."}});
  }
  for (unsigned i = 0; i < 16; ++i)
    level.household.boxes.push_back(
        {"smoke-box-" + std::to_string(i),
         {-5.F + 1.5F * (i % 4), .15F, 1.5F * (i / 4)},
         float(i * 13)});
  for (unsigned i = 0; i < 8; ++i) {
    const auto suffix = std::to_string(i);
    const WorldPosition position{4.F + (i % 4), 0, 1.5F * (i / 4)};
    level.props.push_back(
        {"radio-prop-" + suffix, "apartment-radio", position, 0, 1, {}});
    level.audio.sources.push_back(
        {"radio-source-" + suffix, "radio", position, 1, 1, 20, false});
    level.household.radios.push_back({"radio-control-" + suffix,
                                      "radio-prop-" + suffix,
                                      "radio-source-" + suffix, i % 2 != 0});
  }
  return level;
}
}  // namespace

void EditorApplication::runHouseholdSmoke(
    std::vector<std::string>& events, FrameCapture& capture,
    const std::filesystem::path& capture_directory) {
  requireHousehold(renderer_.validationEnabled(),
                   "Vulkan validation is required");
  requireHousehold(
      document_.document() && scene_resources_installed_ && document_.valid(),
      "neutral household fixture did not prepare");
  const auto original = *document_.document();
  requireHousehold(original.household.boxes.size() == 4 &&
                       original.household.documents.size() == 1 &&
                       original.household.radios.size() == 1,
                   "expected four boxes, one document and one radio");
  requireHousehold(!std::filesystem::exists(capture_directory),
                   "capture directory must be fresh");
  std::filesystem::create_directories(capture_directory);
  std::ofstream evidence(capture_directory / "checks.txt");
  requireHousehold(bool(evidence), "cannot create check evidence");
  evidence << "Automated editor GPU checks; visual/interaction/audio "
              "acceptance is separate.\n";
  const auto count = [&](const char* event) {
    return std::count(events.begin(), events.end(), event);
  };
  const auto immutable = [&] {
    return std::array{count("editor.document-resources.replaced"),
                      count("world.mesh.uploaded"),
                      count("door_preview.mesh.uploaded"),
                      count("texture.created"),
                      count("lighting.created"),
                      count("character.resources.created")};
  };
  const auto stale = [&] {
    return std::any_of(document_.diagnostics().begin(),
                       document_.diagnostics().end(), [](const auto& d) {
                         return d.message.find("Preview is stale") !=
                                std::string::npos;
                       });
  };
  const auto install = [&](const LevelDocument& level, const char* name) {
    const auto path = capture_directory / name;
    const auto saved = saveLevelDocument(path, level);
    requireHousehold(bool(saved),
                     "controlled fixture is invalid: " +
                         formatLevelDiagnostics(saved.diagnostics));
    requireHousehold(document_.open(path), "cannot open controlled fixture");
    synchronizeDocumentResources();
    requireHousehold(
        scene_resources_installed_ && character_resources_current_ && !stale(),
        "controlled fixture replacement failed");
  };
  for (int i = 0; i < 13; ++i) camera_.update({.move_forward = true}, .1);
  camera_.update({.look_delta_x = -300, .look_delta_y = 180}, 0);
  stopInspections();
  const auto frozen_characters = initial_character_palettes_;
  const auto* trusted_font = caption_font_.get();
  const auto* trusted_bytes = caption_font_->fontBytes().data();
  const auto font_atlas =
      std::vector(caption_font_->atlas().begin(), caption_font_->atlas().end());
  const auto capture_ui = [&](const std::string& name) {
    capture.requested = true;
    for (int i = 0; i < 8 && capture.requested; ++i)
      requireHousehold(tick(), "UI capture stopped");
    requireHousehold(!capture.requested && !capture.rgba.empty(),
                     "UI readback did not complete");
    capture.writePpm(capture_directory / (name + ".ppm"));
    return capture.rgba;
  };
  // Direct renderer submissions keep diagnostic/UI changes out of scene
  // comparisons while exercising the same retained scene and matching mesh.
  const auto capture_scene = [&](const std::string& name) {
    capture.requested = true;
    for (int i = 0; i < 8 && capture.requested; ++i) {
      window_.pollEvents();
      renderer_.beginUiFrame();
      glfw_imgui_bridge_.beginFrame();
      ui_.finishFrame();
      FrameRequest frame;
      frame.framebuffer = window_.framebufferExtent();
      requireHousehold(!frame.framebuffer.isZero(),
                       "framebuffer became unavailable during scene capture");
      frame.framebuffer_resized = window_.consumeFramebufferResize();
      frame.camera = camera_.frame(float(frame.framebuffer.width) /
                                   frame.framebuffer.height);
      frame.point_light_enabled = preview_point_light_enabled_;
      frame.characters = initial_character_frames_;
      static_cast<void>(renderer_.renderFrame(frame));
    }
    requireHousehold(!capture.requested && !capture.rgba.empty(),
                     "scene readback did not complete");
    capture.writePpm(capture_directory / (name + ".ppm"));
    return capture.rgba;
  };
  requireHousehold(tick(), "initial editor frame stopped");
  ui_.collapsePanelsForCapture(true);
  const auto initial = capture_scene("01-initial-scene");
  requireHousehold(count("door_preview.mesh.drawn") > 0,
                   "household preview mesh was not drawn");
  const auto baseline_resources = immutable();
  requireHousehold(capture_scene("02-still-scene") == initial &&
                       initial_character_palettes_ == frozen_characters &&
                       *document_.document() == original,
                   "editor advanced physical boxes or initial character poses");
  requireHousehold(immutable() == baseline_resources,
                   "idle preview rebuilt scene resources");
  const auto box_id = document_.householdIds(EditorHouseholdKind::Box).front();
  auto box = std::get<HouseholdBoxDefinition>(*document_.object(box_id));
  box.center.y += .75F;
  box.yaw_degrees += 45;
  requireHousehold(document_.replaceObject(box_id, box),
                   "box pose edit failed");
  synchronizeDocumentResources();
  const auto moved = capture_scene("03-edited-box");
  const auto box_pixels = changedPixels(initial, moved);
  requireHousehold(box_pixels > 30,
                   "authored box pose did not change visible scene pixels");
  requireHousehold(document_.undo(), "box pose undo failed");
  synchronizeDocumentResources();
  requireHousehold(capture_scene("04-box-undo") == initial,
                   "box undo did not restore scene pixels");
  evidence << "Box pose changed pixels: " << box_pixels
           << "; undo restored exact scene.\n";

  const auto radio_id =
      document_.householdIds(EditorHouseholdKind::Radio).front();
  auto radio = std::get<HouseholdRadioDefinition>(*document_.object(radio_id));
  radio.initially_on = !radio.initially_on;
  requireHousehold(document_.replaceObject(radio_id, radio),
                   "radio initial state edit failed");
  synchronizeDocumentResources();
  const auto radio_pixels =
      changedPixels(initial, capture_scene("05-radio-on"));
  requireHousehold(
      radio_pixels >= 2,
      "radio indicator initial state did not change visible pixels");
  requireHousehold(document_.undo(), "radio undo failed");
  synchronizeDocumentResources();
  requireHousehold(capture_scene("06-radio-undo") == initial,
                   "radio undo did not restore its indicator");
  evidence << "Radio indicator changed pixels: " << radio_pixels
           << "; undo restored exact scene.\n";

  const auto note_id =
      document_.householdIds(EditorHouseholdKind::Document).front();
  document_.select(note_id);
  requireHousehold(tick(), "reader preview frame stopped");
  ui_.collapsePanelsForCapture(true);
  ImGui::SetWindowCollapsed("Readable preview", false);
  ImGui::SetWindowPos("Readable preview", {8, 35});
  ImGui::SetWindowSize("Readable preview", {780, 360});
  capture_ui("07-reader-before");
  const auto before_page = readerPixels(capture);
  auto note = std::get<HouseholdDocumentDefinition>(*document_.object(note_id));
  note.pages[0] = "Ёж ждёт у окна.\nЭто изменённая страница редактора.";
  requireHousehold(document_.replaceObject(note_id, note),
                   "document page revision failed");
  capture_ui("08-reader-after");
  const auto page_pixels = changedPixels(before_page, readerPixels(capture));
  requireHousehold(page_pixels > 20,
                   "document revision did not change reader canvas glyphs");
  requireHousehold(capture_scene("09-page-only-scene") == initial,
                   "page text edit changed world geometry");
  requireHousehold(document_.undo(), "page text undo failed");
  capture_ui("10-reader-undo");
  requireHousehold(readerPixels(capture) == before_page,
                   "page undo did not restore reader canvas glyphs");
  evidence << "Reader canvas changed pixels: " << page_pixels
           << "; undo restored exact canvas.\n";
  const auto recovery_resources = immutable();
  auto* imgui_font = ImGui::GetIO().Fonts->Fonts.front();
  const auto imgui_font_count = ImGui::GetIO().Fonts->Fonts.Size;
  for (const auto [width, height] :
       std::array<std::pair<unsigned, unsigned>, 3>{
           {{800, 600}, {1920, 1080}, {3840, 2160}}}) {
    window_.setSize(width, height);
    renderer_.requestSwapchainRecreation();
    capture_ui("11-reader-" + std::to_string(width) + "x" +
               std::to_string(height));
    requireHousehold(capture.width == width && capture.height == height,
                     "requested framebuffer extent is unavailable: " +
                         std::to_string(width) + "x" + std::to_string(height));
    requireHousehold(
        !readerPixels(capture).empty() && *document_.document() == original,
        "resize lost readable preview or authored data");
  }
  {
    const auto formats = count("swapchain.surface_format.alternate.selected");
    HouseholdFailure alternate("alternate_surface_format");
    renderer_.requestSwapchainRecreation();
    capture_ui("12-alternate-format-reader");
    requireHousehold(
        count("swapchain.surface_format.alternate.selected") > formats,
        "alternate attachment format was not exercised");
  }
  window_.setSize(1600, 900);
  renderer_.requestSwapchainRecreation();
  capture_ui("13-restored-reader");
  requireHousehold(
      immutable() == recovery_resources &&
          caption_font_.get() == trusted_font &&
          caption_font_->fontBytes().data() == trusted_bytes &&
          std::equal(font_atlas.begin(), font_atlas.end(),
                     caption_font_->atlas().begin()) &&
          ImGui::GetIO().Fonts->Fonts.front() == imgui_font &&
          ImGui::GetIO().Fonts->Fonts.Size == imgui_font_count,
      "compatible recovery replaced scene or trusted font resources");
  evidence << "800x600, 1920x1080, 3840x2160 and alternate-format reader "
              "recovery retained scene/font owners.\n";

  ui_.collapsePanelsForCapture(true);
  document_.select(editor_no_object);
  const auto stable = capture_scene("14-before-replacement-failure");
  for (const auto* stage : {"door_preview_mesh_upload", "shadow_image"}) {
    const auto old_lights = preview_point_light_enabled_;
    const auto old_palettes = initial_character_palettes_;
    const auto old_installs = count("editor.document-resources.replaced");
    auto failed_box =
        std::get<HouseholdBoxDefinition>(*document_.object(box_id));
    failed_box.center.y += .6F;
    requireHousehold(document_.replaceObject(box_id, failed_box),
                     "failed candidate box edit rejected");
    auto failed_radio =
        std::get<HouseholdRadioDefinition>(*document_.object(radio_id));
    failed_radio.initially_on = !failed_radio.initially_on;
    requireHousehold(document_.replaceObject(radio_id, failed_radio),
                     "failed candidate radio edit rejected");
    const auto prop_id = document_.propIds().front();
    auto failed_prop =
        std::get<PrototypeStaticProp>(*document_.object(prop_id));
    failed_prop.translation.z += .4F;
    requireHousehold(document_.replaceObject(prop_id, failed_prop),
                     "failed candidate static edit rejected");
    const auto light_id = document_.lightIds().front();
    auto failed_light =
        std::get<PrototypePointLight>(*document_.object(light_id));
    failed_light.initially_on = !failed_light.initially_on;
    requireHousehold(document_.replaceObject(light_id, failed_light),
                     "failed candidate lighting edit rejected");
    {
      HouseholdFailure failure(stage);
      synchronizeDocumentResources();
    }
    requireHousehold(
        scene_resources_installed_ && stale() &&
            count("editor.document-resources.replaced") == old_installs &&
            preview_point_light_enabled_ == old_lights &&
            initial_character_palettes_ == old_palettes,
        std::string(stage) + " mixed preceding/candidate scene owners");
    const std::string expected_failure =
        std::string_view(stage) == "shadow_image"
            ? "shadow_image"
            : "Forced door_preview mesh upload failure";
    requireHousehold(
        std::any_of(document_.diagnostics().begin(),
                    document_.diagnostics().end(),
                    [&](const auto& diagnostic) {
                      return diagnostic.message.find(expected_failure) !=
                             std::string::npos;
                    }),
        std::string(stage) + " was not the observed replacement failure");
    for (const auto& diagnostic : document_.diagnostics())
      evidence << stage << ": " << diagnostic.document_path << ": "
               << diagnostic.message << '\n';
    requireHousehold(
        capture_scene(std::string("15-retained-") + stage) == stable,
        std::string(stage) +
            " did not retain the complete preceding scene and household mesh");
    const auto failed_resources = immutable();
    requireHousehold(
        tick() && immutable() == failed_resources && stale(),
        "stale preview retried replacement without an authored edit");
    requireHousehold(document_.undo() && document_.undo() && document_.undo() &&
                         document_.undo(),
                     "failed candidate undo failed");
    synchronizeDocumentResources();
    requireHousehold(
        !stale() &&
            capture_scene(std::string("16-repaired-") + stage) == stable,
        std::string(stage) + " correction did not restore complete scene");
  }

  auto door_free = original;
  door_free.doors.clear();
  for (auto& connection : door_free.audio.connections) connection.door.reset();
  install(door_free, "door-free.level.json");
  capture_scene("17-door-free-household");
  requireHousehold(householdInitialPresentation(door_free).size() == 6,
                   "door-free preview lost one of its household markers");
  auto empty_household = door_free;
  empty_household.household = {};
  install(empty_household, "door-free-empty.level.json");
  const auto empty_pixels = capture_scene("18-door-free-empty");
  install(door_free, "door-free-restored.level.json");
  requireHousehold(
      changedPixels(empty_pixels, capture_scene("19-door-free-restored")) > 30,
      "household-only material/mesh did not reach the color pass");
  const auto capacity = capacityDocument(original);
  requireHousehold(capacity.doors.size() * 6 +
                           householdInitialPresentation(capacity).size() ==
                       248,
                   "maximum fixture does not exercise all 248 generated boxes");
  install(capacity, "maximum-household.level.json");
  capture_scene("20-maximum-248-boxes");
  requireHousehold(!stale(), "maximum preview failed resource installation");
  evidence << "Door-free household-only material and maximum 248-box preview "
              "submitted successfully.\n";
  requireHousehold(validation_diagnostics_.errorCount() == 0,
                   "Vulkan validation reported an error");
  evidence << "Pre-teardown Vulkan errors: 0. Final destruction/lifetime "
              "counts are in lifecycle.txt.\n";
  requireHousehold(bool(evidence), "cannot complete check evidence");
}
