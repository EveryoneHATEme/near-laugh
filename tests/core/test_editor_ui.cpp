#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <filesystem>

#include "editor/editor_camera.hpp"
#include "editor/editor_overlay.hpp"
#include "editor/editor_ui.hpp"

namespace {
class EditorUiInteraction : public testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1600, 900};
    io.DeltaTime = 1.0F / 60.0F;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ASSERT_TRUE(document.open("resources/levels/prototype.level.json"));
    frame();
    frame();
  }
  void TearDown() override { ImGui::DestroyContext(); }
  void frame(bool navigating = false) {
    ImGui::NewFrame();
    const auto& io = ImGui::GetIO();
    camera.update(
        editorNavigationInput(physical, navigating || camera_navigation,
                              {io.WantCaptureKeyboard, io.WantCaptureMouse}),
        io.DeltaTime);
    ui.draw(document);
    static_cast<void>(
        ui.updateViewport(document, camera.frame(1600.0F / 900), navigating));
    drawInspection();
    ui.finishFrame();
  }
  virtual void drawInspection() {}
  void click(ImVec2 point) {
    ImGui::GetIO().AddMousePosEvent(point.x, point.y);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    frame();
  }
  void key(ImGuiKey key, bool control = false, bool navigating = false) {
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, control);
    ImGui::GetIO().AddKeyEvent(key, true);
    frame(navigating);
    ImGui::GetIO().AddKeyEvent(key, false);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
    frame(navigating);
  }
  ImVec2 addButtonCenter() {
    const ImGuiWindow* window = ImGui::FindWindowByName("Objects");
    return {
        window->Pos.x + window->WindowPadding.x + 30,
        window->Pos.y + window->TitleBarHeight + window->WindowPadding.y + 8};
  }
  void activate(const char* window_name, const char* label,
                std::optional<int> scope = {}) {
    auto* window = ImGui::FindWindowByName(window_name);
    ASSERT_NE(window, nullptr);
    ImGuiID seed = window->ID;
    if (scope) seed = ImHashData(&*scope, sizeof(*scope), seed);
    ImGui::FocusWindow(window);
    ImGui::ActivateItemByID(ImHashStr(label, 0, seed));
    frame();
    frame();
  }
  void choosePopup(const char* label) {
    const ImGuiWindow* popup = nullptr;
    for (const auto* window : ImGui::GetCurrentContext()->Windows)
      if (window->Active && (window->Flags & ImGuiWindowFlags_Popup))
        popup = window;
    ASSERT_NE(popup, nullptr);
    const std::string name = popup->Name;
    activate(name.c_str(), label);
  }
  std::string loggedFrame() {
    ImGui::NewFrame();
    ImGui::LogToBuffer();
    ui.draw(document);
    drawInspection();
    const std::string result = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    static_cast<void>(
        ui.updateViewport(document, camera.frame(1600.0F / 900), false));
    ui.finishFrame();
    return result;
  }
  EditorDocument document;
  EditorUi ui;
  EditorCamera camera;
  PhysicalInputSnapshot physical;
  bool camera_navigation{};
};

class EditorCharacterUiInteraction : public EditorUiInteraction {
 protected:
  void SetUp() override {
    EditorUiInteraction::SetUp();
    ASSERT_TRUE(
        document.open("resources/levels/scripted-characters.level.json"));
    asset = loadCharacterAsset("resources/characters/test-mannequin.glb");
    actor = document.characterIds(EditorCharacterKind::Actor).front();
    route = document.characterIds(EditorCharacterKind::Route).front();
    document.select(actor);
    frame();
    frame();
  }
  void drawInspection() override {
    if (!asset) return;
    preview.synchronize(document);
    preview.advance(ImGui::GetIO().DeltaTime);
    if (const auto request =
            ui.drawCharacterPreview(document, preview, resources_current))
      EXPECT_TRUE(preview.start(document, *request, asset)) << preview.error();
  }
  void inspect(const char* label) { activate("Character inspection", label); }
  void selectCharacter(EditorObjectId id, const char* prefix) {
    const auto value = *document.object(id);
    std::string label;
    std::visit(
        [&](const auto& record) {
          if constexpr (requires { record.id; }) label = prefix + record.id;
        },
        value);
    activate("Objects", label.c_str(), static_cast<int>(id));
    ASSERT_EQ(document.selection(), id);
  }
  void numericInput(const char* window_name, const char* label) {
    auto* window = ImGui::FindWindowByName(window_name);
    ASSERT_NE(window, nullptr);
    ImGui::FocusWindow(window);
    const ImGuiID id = ImHashStr(label, 0, window->ID);
    ImGui::ActivateItemByID(id);
    ImGui::GetCurrentContext()->NavNextActivateFlags =
        ImGuiActivateFlags_PreferInput;
    frame();
    frame();
    ASSERT_EQ(ImGui::GetActiveID(), id);
    ASSERT_TRUE(ImGui::GetIO().WantTextInput);
  }
  void expectCapturedNavigation() {
    // Keyboard activation does not put the pointer over the UI. Supply a real
    // pointer position before exercising simultaneous keyboard/mouse capture.
    const auto* window = ImGui::GetCurrentContext()->ActiveIdWindow;
    ASSERT_NE(window, nullptr);
    ImGui::GetIO().AddMousePosEvent(window->Pos.x + 40,
                                    window->Pos.y + window->TitleBarHeight / 2);
    frame();
    frame();
    const auto position = camera.position();
    const auto yaw = camera.yawDegrees();
    const auto pitch = camera.pitchDegrees();
    ASSERT_TRUE(ImGui::GetIO().WantCaptureKeyboard);
    ASSERT_TRUE(ImGui::GetIO().WantCaptureMouse);
    physical.keys[static_cast<std::size_t>(PhysicalKey::W)] = true;
    physical.keys[static_cast<std::size_t>(PhysicalKey::D)] = true;
    physical.keys[static_cast<std::size_t>(PhysicalKey::Space)] = true;
    physical.cursor_delta_x = 40;
    physical.cursor_delta_y = -20;
    camera_navigation = true;
    frame();
    frame();
    physical = {};
    camera_navigation = false;
    EXPECT_FLOAT_EQ(camera.position().x, position.x);
    EXPECT_FLOAT_EQ(camera.position().y, position.y);
    EXPECT_FLOAT_EQ(camera.position().z, position.z);
    EXPECT_FLOAT_EQ(camera.yawDegrees(), yaw);
    EXPECT_FLOAT_EQ(camera.pitchDegrees(), pitch);
  }
  EditorCharacterPreview preview;
  std::shared_ptr<const CharacterAsset> asset;
  EditorObjectId actor{}, route{};
  bool resources_current{true};
};
}  // namespace

TEST_F(EditorUiInteraction,
       AddButtonDoesNotClickThroughAndShortcutsUndoDuplicateDelete) {
  const auto original = *document.document();
  click(addButtonCenter());
  ASSERT_EQ(document.solidIds().size(), original.solids.size() + 1);
  const auto added = document.selection();
  EXPECT_GE(added, editor_first_solid);
  key(ImGuiKey_D, true);
  EXPECT_EQ(document.solidIds().size(), original.solids.size() + 2);
  EXPECT_NE(document.selection(), added);
  key(ImGuiKey_Delete);
  EXPECT_EQ(document.solidIds().size(), original.solids.size() + 1);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(document.solidIds().size(), original.solids.size() + 2);
  key(ImGuiKey_Z, true);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
  key(ImGuiKey_Y, true);
  EXPECT_EQ(document.selection(), added);
}

TEST_F(EditorUiInteraction,
       LightingShortcutsAndCameraNavigationRespectCapture) {
  const auto original = *document.document();
  for (const auto id : {document.lightIds().front(), document.lightIds()[1]}) {
    document.select(id);
    frame();
    key(ImGuiKey_D, true);
    EXPECT_EQ(document.lightIds().size(),
              original.environment_light.point_lights.size() + 1);
    key(ImGuiKey_Z, true);
    document.select(id);
    key(ImGuiKey_Delete);
    EXPECT_EQ(document.lightIds().size(),
              original.environment_light.point_lights.size() - 1);
    key(ImGuiKey_Z, true);
    EXPECT_EQ(*document.document(), original);
  }
  document.select(document.solidIds()[0]);
  key(ImGuiKey_Delete, false, true);
  key(ImGuiKey_D, true, true);
  EXPECT_EQ(*document.document(), original);
}

TEST_F(EditorUiInteraction,
       AudioButtonsSelectionAndShortcutsUseTheSameHistory) {
  const auto original = *document.document();
  const ImGuiWindow* audio = ImGui::FindWindowByName("Audio authoring");
  ASSERT_NE(audio, nullptr);
  const ImVec2 add{
      audio->Pos.x + audio->WindowPadding.x + 25,
      audio->Pos.y + audio->TitleBarHeight + audio->WindowPadding.y + 8};
  click(add);
  ASSERT_EQ(document.audioIds(EditorAudioKind::Cue).size(), 1U);
  click({add.x + ImGui::CalcTextSize("Add cue").x +
             ImGui::GetStyle().FramePadding.x * 2 +
             ImGui::GetStyle().ItemSpacing.x,
         add.y});
  ASSERT_EQ(document.audioIds(EditorAudioKind::Source).size(), 1U);
  const auto source = document.selection();
  EXPECT_EQ(document.selection(),
            document.audioIds(EditorAudioKind::Source)[0]);
  key(ImGuiKey_D, true);
  ASSERT_EQ(document.audioIds(EditorAudioKind::Source).size(), 2U);
  key(ImGuiKey_Delete);
  ASSERT_EQ(document.audioIds(EditorAudioKind::Source).size(), 1U);
  key(ImGuiKey_Z, true);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(document.selection(), source);
  key(ImGuiKey_Z, true);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorUiInteraction,
       DoorButtonAndKeyboardEditsRetainIdentityThroughUndo) {
  const auto start_count = document.doorIds().size();
  const auto add = addButtonCenter();
  click({add.x, add.y + 3 * ImGui::GetFrameHeightWithSpacing()});
  ASSERT_EQ(document.doorIds().size(), start_count + 1);
  const auto handle = document.selection();
  const auto id = std::get<DoorDefinition>(*document.object(handle)).id;
  key(ImGuiKey_D, true);
  ASSERT_EQ(document.doorIds().size(), start_count + 2);
  const auto duplicate = document.selection();
  EXPECT_NE(std::get<DoorDefinition>(*document.object(duplicate)).id, id);
  key(ImGuiKey_Delete);
  EXPECT_EQ(document.doorIds().size(), start_count + 1);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(document.selection(), duplicate);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(document.selection(), handle);
  EXPECT_EQ(std::get<DoorDefinition>(*document.object(handle)).id, id);
}

TEST_F(EditorUiInteraction, UnsavedModalBlocksUnderlyingViewportAndShortcuts) {
  click(addButtonCenter());
  const auto edited = *document.document();
  document.requestClose();
  frame();
  frame();
  key(ImGuiKey_Z, true);
  key(ImGuiKey_Delete);
  EXPECT_EQ(*document.document(), edited);
  EXPECT_EQ(document.pendingAction().kind, EditorPendingActionKind::Close);
  ASSERT_TRUE(document.resolvePending(EditorPendingDecision::Cancel));
  EXPECT_EQ(*document.document(), edited);
}

TEST_F(EditorUiInteraction, NumericDragCommitsOnceOnRelease) {
  document.select(editor_spawn);
  frame();
  const auto original = *document.document();
  const ImGuiWindow* properties = ImGui::FindWindowByName("Properties");
  // Collapse terrain to expose the object's first two numeric rows directly.
  const float top = properties->Pos.y + properties->TitleBarHeight +
                    properties->WindowPadding.y;
  click({properties->Pos.x + 40, top + 8});
  const float row = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
  const ImVec2 yaw{properties->Pos.x + properties->WindowPadding.x + 40,
                   top + 3 * row + 8};
  ImGui::GetIO().AddMousePosEvent(yaw.x, yaw.y);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, true);
  frame();
  for (int i = 1; i <= 5; ++i) {
    ImGui::GetIO().AddMousePosEvent(yaw.x + i * 10, yaw.y);
    frame();
    EXPECT_EQ(*document.document(), original);
  }
  ImGui::GetIO().AddMouseButtonEvent(0, false);
  frame();
  EXPECT_NE(document.document()->entries.front().pose.yaw_degrees,
            original.entries.front().pose.yaw_degrees);
  ASSERT_TRUE(document.undo());
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.canUndo());
}

TEST_F(EditorUiInteraction, NewlyAddedSwitchUsesItsOwnFloorPlacementOffset) {
  document.requestNewInterior();
  frame();
  frame();
  document.select(document.solidIds().front());
  frame();
  const auto add = addButtonCenter();
  const auto row = ImGui::GetFrameHeightWithSpacing();
  click({add.x, add.y + 2 * row});
  ASSERT_EQ(document.selection(), document.switchIds().front());
  click({add.x, add.y + 5 * row});
  click({800, 700});
  ASSERT_FALSE(document.document()->light_switches.empty());
  EXPECT_FLOAT_EQ(document.document()->light_switches.front().position.y, 1.4F);
  EXPECT_NE(document.document()->light_switches.front().position.z, 2.0F);
}

TEST_F(EditorUiInteraction,
       NewInteriorClearsTerrainToolsAndPlayDialogsConsumeOneSaveAs) {
  const auto* properties = ImGui::FindWindowByName("Properties");
  const float top = properties->Pos.y + properties->TitleBarHeight +
                    properties->WindowPadding.y;
  click({properties->Pos.x + 40, top + 2 * ImGui::GetFrameHeightWithSpacing() +
                                     3 * ImGui::GetTextLineHeightWithSpacing() +
                                     8});
  ASSERT_TRUE(ui.sculpting());
  document.requestNewInterior();
  frame();
  frame();
  EXPECT_FALSE(ui.sculpting());
  EXPECT_FALSE(document.document()->terrain);
  const auto content = [](const char* name) {
    const auto* window = ImGui::FindWindowByName(name);
    return ImVec2{
        window->Pos.x + window->WindowPadding.x,
        window->Pos.y + window->TitleBarHeight + window->WindowPadding.y};
  };
  const auto activatePlay = [&] {
    const auto origin = content("Playtest");
    click({origin.x + 20, origin.y + ImGui::GetFrameHeightWithSpacing() + 8});
    frame();
  };
  activatePlay();
  ASSERT_TRUE(ImGui::IsPopupOpen("Save and Play", ImGuiPopupFlags_AnyPopupId));
  EXPECT_FALSE(ui.takeLaunchRequest());
  const auto confirm = content("Save and Play");
  click({confirm.x + ImGui::CalcTextSize("Save and Play").x +
             ImGui::GetStyle().FramePadding.x * 2 +
             ImGui::GetStyle().ItemSpacing.x + 20,
         confirm.y + ImGui::GetTextLineHeightWithSpacing() + 8});
  EXPECT_FALSE(ui.takeLaunchRequest());
  EXPECT_TRUE(document.dirty());
  activatePlay();
  const auto save_confirm = content("Save and Play");
  click({save_confirm.x + 35,
         save_confirm.y + ImGui::GetTextLineHeightWithSpacing() + 8});
  frame();
  frame();
  ASSERT_TRUE(ImGui::IsPopupOpen("Save Level As", ImGuiPopupFlags_AnyPopupId));
  const auto save_as = content("Save Level As");
  click({save_as.x + 35, save_as.y + 8});
  const auto path =
      std::filesystem::temp_directory_path() /
      ("near_laugh_ui_play_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".json");
  const auto native = path.u8string();
  const std::string text(native.begin(), native.end());
  ImGui::GetIO().AddInputCharactersUTF8(text.c_str());
  frame();
  click({save_as.x + 20, save_as.y + ImGui::GetFrameHeightWithSpacing() + 8});
  const auto launch = ui.takeLaunchRequest();
  ASSERT_TRUE(launch);
  EXPECT_EQ(launch->level_path, path);
  EXPECT_EQ(launch->entry_id, "default");
  EXPECT_FALSE(ui.takeLaunchRequest());
  EXPECT_FALSE(document.dirty());
  EXPECT_TRUE(std::filesystem::exists(path));
  std::filesystem::remove(path);
  ASSERT_TRUE(document.open("resources/levels/prototype.level.json"));
  frame();
  EXPECT_TRUE(document.document()->terrain);
  EXPECT_FALSE(ui.sculpting());
}

TEST_F(EditorUiInteraction, UpperSurfacePreviewAndClickUseTheSameCandidate) {
  document.requestNewInterior();
  frame();
  ASSERT_TRUE(document.addSolid({{0, 2.75F, 0},
                                 {5, .25F, 5},
                                 {150, 150, 150, 255},
                                 PrototypeSolidKind::Floor,
                                 "prototype-floor"}));
  document.select(document.entryIds()[0]);
  EditorNavigationInput up;
  up.move_up = true;
  for (int i = 0; i < 12; ++i) camera.update(up, .1);
  frame();
  const auto* objects = ImGui::FindWindowByName("Objects");
  const float top =
      objects->Pos.y + objects->TitleBarHeight + objects->WindowPadding.y;
  click(
      {objects->Pos.x + 30, top + 5 * ImGui::GetFrameHeightWithSpacing() + 8});
  const WorldPosition target{1, 3, -3};
  const auto projection =
      projectEditorLine(camera.frame(1600.0F / 900), target, target, {});
  ASSERT_TRUE(projection);
  const ImVec2 pointer{projection->first[0] * 1600, projection->first[1] * 900};
  ImGui::GetIO().AddMousePosEvent(pointer.x, pointer.y);
  frame();
  const auto before = *document.document();
  ImGui::NewFrame();
  ui.draw(document);
  const auto preview =
      ui.updateViewport(document, camera.frame(1600.0F / 900), false);
  ui.finishFrame();
  ASSERT_TRUE(preview);
  EXPECT_NEAR(preview->y, 3, .0001F);
  EXPECT_EQ(*document.document(), before);
  click(pointer);
  const auto placed = document.document()->entries[0].pose.foot_position;
  EXPECT_EQ(placed, *preview);
  EXPECT_TRUE(document.valid());
  key(ImGuiKey_Escape);
  click({pointer.x + 20, pointer.y});
  EXPECT_EQ(document.document()->entries[0].pose.foot_position, placed);
}

TEST_F(EditorUiInteraction,
       TerrainCheckboxDragAndCaptureProduceOneUndoableGesture) {
  const auto original = *document.document();
  const ImGuiWindow* properties = ImGui::FindWindowByName("Properties");
  const float top = properties->Pos.y + properties->TitleBarHeight +
                    properties->WindowPadding.y;
  const float row = ImGui::GetTextLineHeightWithSpacing();
  // Header, origin, spacing and height range precede the tool checkbox.
  click({properties->Pos.x + 40,
         top + 2 * ImGui::GetFrameHeightWithSpacing() + 3 * row + 8});
  ASSERT_TRUE(ui.sculpting());
  EXPECT_EQ(*document.document(), original);
  const ImVec2 start{750, 620};
  ImGui::GetIO().AddMousePosEvent(start.x, start.y);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, true);
  frame();
  ASSERT_TRUE(document.terrainStrokeActive());
  for (int i = 1; i <= 10; ++i) {
    ImGui::GetIO().AddMousePosEvent(start.x + i * 6, start.y);
    frame();
  }
  const auto dragged = *document.document();
  EXPECT_NE(dragged.terrain, original.terrain);
  for (int i = 0; i < 30; ++i) frame();
  EXPECT_EQ(*document.document(), dragged);
  ImGui::GetIO().AddMouseButtonEvent(0, false);
  frame();
  EXPECT_FALSE(document.terrainStrokeActive());
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.canUndo());

  // A UI drag ending over the scene remains a UI gesture.
  ImGui::GetIO().AddMousePosEvent(properties->Pos.x + 60, top + 10);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, true);
  frame();
  ImGui::GetIO().AddMousePosEvent(800, 650);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, false);
  frame();
  EXPECT_EQ(*document.document(), original);
  key(ImGuiKey_Escape);
  EXPECT_FALSE(ui.sculpting());
}

TEST_F(EditorUiInteraction,
       ActorSurfacePlacementShowsSharedMarkAndEscapeCancelsWithoutEditing) {
  document.requestNewInterior();
  ASSERT_TRUE(document.addSolid({{0, 2.75F, 0},
                                 {5, .25F, 5},
                                 {150, 150, 150, 255},
                                 PrototypeSolidKind::Floor,
                                 "prototype-floor"}));
  ASSERT_TRUE(document.addCharacter(EditorCharacterKind::Actor));
  const auto actor = document.selection();
  const auto mark = document.placementTarget();
  const auto initial = *document.document();
  EditorNavigationInput up;
  up.move_up = true;
  for (int i = 0; i < 12; ++i) camera.update(up, .1);
  frame();
  EXPECT_NE(loggedFrame().find("Mark consumers (all share this placement)"),
            std::string::npos);
  activate("Objects", "Place on surface");
  const WorldPosition target{1, 3, -3};
  const auto projection =
      projectEditorLine(camera.frame(1600.F / 900), target, target, {});
  ASSERT_TRUE(projection);
  const ImVec2 pointer{projection->first[0] * 1600, projection->first[1] * 900};
  ImGui::GetIO().AddMousePosEvent(pointer.x, pointer.y);
  frame();
  ImGui::NewFrame();
  ui.draw(document);
  const auto candidate =
      ui.updateViewport(document, camera.frame(1600.F / 900), false);
  ui.finishFrame();
  ASSERT_TRUE(candidate);
  EXPECT_NEAR(candidate->y, 3, .0001F);
  EXPECT_EQ(*document.document(), initial);
  click(pointer);
  EXPECT_EQ(document.selection(), actor);
  EXPECT_EQ(
      std::get<CharacterMarkDefinition>(*document.object(mark)).feet_position,
      *candidate);
  const auto placed = *document.document();
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), initial);
  key(ImGuiKey_Y, true);
  EXPECT_EQ(*document.document(), placed);
  key(ImGuiKey_Escape);
  click({pointer.x + 20, pointer.y});
  EXPECT_EQ(*document.document(), placed);
}

TEST_F(EditorUiInteraction, SwitchButtonsPropertiesAndInputCapture) {
  document.select(document.switchIds().front());
  frame();
  key(ImGuiKey_D, true);
  EXPECT_EQ(document.switchIds().size(), 2U);
  key(ImGuiKey_Z, true);
  EXPECT_FALSE(document.dirty());
  key(ImGuiKey_Delete, false, true);
  EXPECT_FALSE(document.document()->light_switches.empty());
  key(ImGuiKey_Delete);
  ASSERT_TRUE(document.document()->light_switches.empty());
  const auto* objects = ImGui::FindWindowByName("Objects");
  const float object_top =
      objects->Pos.y + objects->TitleBarHeight + objects->WindowPadding.y;
  const float row = ImGui::GetFrameHeightWithSpacing();
  click({objects->Pos.x + 60, object_top + 2 * row + 8});
  ASSERT_FALSE(document.document()->light_switches.empty());
  EXPECT_EQ(document.selection(), document.switchIds().front());
  const auto created = *document.document();
  click({objects->Pos.x + 60, object_top + 2 * row + 8});
  EXPECT_EQ(document.switchIds().size(), 2U);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), created);
  const auto* properties = ImGui::FindWindowByName("Properties");
  const float top = properties->Pos.y + properties->TitleBarHeight +
                    properties->WindowPadding.y;
  click({properties->Pos.x + 40, top + 8});  // collapse Terrain
  // Select Point light 2 through the real combo popup.
  click({properties->Pos.x + 70, top + 4 * row + 8});
  frame();
  frame();
  const ImGuiWindow* popup = nullptr;
  for (auto* window : ImGui::GetCurrentContext()->Windows)
    if (window->Active && (window->Flags & ImGuiWindowFlags_Popup))
      popup = window;
  ASSERT_NE(popup, nullptr);
  click({popup->Pos.x + 60, popup->Pos.y + popup->WindowPadding.y +
                                ImGui::GetTextLineHeightWithSpacing() + 6});
  ASSERT_EQ(document.document()->light_switches.front().light_id,
            "point-light-1");
  EXPECT_EQ(document.document()->environment_light, created.environment_light);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), created);
  // A yaw drag edits a draft and commits once when released.
  const ImVec2 yaw{properties->Pos.x + 40, top + 3 * row + 8};
  ImGui::GetIO().AddMousePosEvent(yaw.x, yaw.y);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, true);
  frame();
  ImGui::GetIO().AddMousePosEvent(yaw.x + 40, yaw.y);
  frame();
  EXPECT_EQ(*document.document(), created);
  ImGui::GetIO().AddMouseButtonEvent(0, false);
  frame();
  EXPECT_NE(document.document()->light_switches.front().yaw_degrees,
            created.light_switches.front().yaw_degrees);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), created);
  // Ctrl-click exact entry, including rejection of a non-finite value.
  for (const auto* text : {"1e39", "45"}) {
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
    click(yaw);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
    frame();
    ImGui::GetIO().AddInputCharactersUTF8(text);
    frame();
    key(ImGuiKey_Enter);
    if (text[0] == '1') {
      EXPECT_EQ(*document.document(), created);
      EXPECT_FALSE(document.editError().empty());
    } else {
      EXPECT_FLOAT_EQ(document.document()->light_switches.front().yaw_degrees,
                      45);
    }
  }
}
TEST_F(EditorUiInteraction, PointLightInitialAndShadowCheckboxesUseHistory) {
  document.select(document.lightIds().front());
  frame();
  const auto original = *document.document();
  const auto* properties = ImGui::FindWindowByName("Properties");
  const float top = properties->Pos.y + properties->TitleBarHeight +
                    properties->WindowPadding.y;
  const float row = ImGui::GetFrameHeightWithSpacing();
  click({properties->Pos.x + 40, top + 8});
  click({properties->Pos.x + 40, top + 6 * row + 8});
  EXPECT_FALSE(
      document.document()->environment_light.point_lights.front().initially_on);
  click({properties->Pos.x + 40, top + 7 * row + 8});
  EXPECT_TRUE(document.document()
                  ->environment_light.point_lights.front()
                  .casts_shadows);
  EXPECT_EQ(document.document()->light_switches, original.light_switches);
  key(ImGuiKey_Z, true);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorUiInteraction, CharacterListsSelectEachDurableRecordOnce) {
  ASSERT_TRUE(document.open("resources/levels/scripted-characters.level.json"));
  frame();
  const auto original = *document.document();
  const auto logged = loggedFrame();
  const std::array prefixes{"Actor: ", "Mark: ", "Route: "};
  for (std::size_t kind = 0; kind < prefixes.size(); ++kind) {
    for (const auto id :
         document.characterIds(static_cast<EditorCharacterKind>(kind))) {
      const auto value = *document.object(id);
      std::string label;
      std::visit(
          [&](const auto& record) {
            if constexpr (requires { record.id; })
              label = prefixes[kind] + record.id;
          },
          value);
      const auto first = logged.find(label);
      ASSERT_NE(first, std::string::npos) << label;
      EXPECT_EQ(logged.find(label, first + label.size()), std::string::npos)
          << label;
      activate("Objects", label.c_str(), static_cast<int>(id));
      EXPECT_EQ(document.selection(), id) << label;
    }
  }
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorUiInteraction, CharacterButtonsAndShortcutsShareCompoundHistory) {
  ASSERT_TRUE(document.open("resources/levels/scripted-characters.level.json"));
  frame();
  const auto original = *document.document();
  activate("Objects", "Add actor");
  ASSERT_EQ(document.document()->characters.actors.size(), 2U);
  ASSERT_EQ(document.document()->characters.marks.size(),
            original.characters.marks.size() + 1);
  const auto actor = document.selection();
  const auto added = *document.document();
  key(ImGuiKey_D, true);
  ASSERT_EQ(document.document()->characters.actors.size(), 3U);
  ASSERT_EQ(document.document()->characters.marks.size(),
            original.characters.marks.size() + 2);
  const auto duplicate = document.selection();
  const auto copy =
      std::get<CharacterActorDefinition>(*document.object(duplicate));
  EXPECT_NE(
      copy.initial_mark,
      std::get<CharacterActorDefinition>(*document.object(actor)).initial_mark);
  EXPECT_FALSE(copy.initial_route);
  EXPECT_FALSE(copy.footstep_source);
  EXPECT_FALSE(copy.interaction_source);
  key(ImGuiKey_Delete);
  EXPECT_EQ(document.document()->characters.actors.size(), 2U);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(document.selection(), duplicate);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(document.selection(), actor);
  EXPECT_EQ(*document.document(), added);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
  key(ImGuiKey_Y, true);
  EXPECT_EQ(*document.document(), added);
  key(ImGuiKey_Z, true);
  activate("Objects", "Add mark");
  ASSERT_EQ(document.document()->characters.marks.size(),
            original.characters.marks.size() + 1);
  activate("Objects", "Add route");
  ASSERT_EQ(document.document()->characters.routes.size(),
            original.characters.routes.size() + 1);
  key(ImGuiKey_Z, true);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorUiInteraction, CharacterUnknownReferencesStayVisibleUntilChosen) {
  ASSERT_TRUE(document.open("resources/levels/scripted-characters.level.json"));
  const auto actor_id =
      document.characterIds(EditorCharacterKind::Actor).front();
  const auto original =
      std::get<CharacterActorDefinition>(*document.object(actor_id));
  auto actor = original;
  actor.model = "missing-model";
  actor.initial_mark = "missing-mark";
  actor.initial_route = "missing-route";
  actor.footstep_source = "missing-step";
  actor.interaction_source = "missing-touch";
  ASSERT_TRUE(document.replaceObject(actor_id, actor));
  document.select(actor_id);
  frame();
  const auto invalid = *document.document();
  const auto logged = loggedFrame();
  for (const auto* missing : {"missing-model", "missing-mark", "missing-route",
                              "missing-step", "missing-touch"})
    EXPECT_NE(logged.find(missing), std::string::npos);
  EXPECT_EQ(*document.document(), invalid);
  EXPECT_FALSE(document.valid());
  activate("Properties", "Initial mark");
  choosePopup(original.initial_mark.c_str());
  EXPECT_EQ(document.document()->characters.actors.front().initial_mark,
            original.initial_mark);
  EXPECT_EQ(document.document()->characters.actors.front().model, actor.model);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), invalid);
  EXPECT_EQ(document.selection(), actor_id);
  activate("Properties", "Character model");
  choosePopup(original.model.c_str());
  EXPECT_EQ(document.document()->characters.actors.front().model,
            original.model);
  EXPECT_EQ(document.document()->characters.actors.front().initial_mark,
            actor.initial_mark);
}

TEST_F(EditorUiInteraction, CharacterRouteControlsKeepOrderedLinksUndoable) {
  ASSERT_TRUE(document.open("resources/levels/scripted-characters.level.json"));
  const auto route_id =
      document.characterIds(EditorCharacterKind::Route).front();
  document.select(route_id);
  frame();
  const auto original = *document.document();
  const auto route = original.characters.routes.front();
  ASSERT_GE(route.marks.size(), 2U);
  activate("Properties", "Down", 0);
  auto reordered = route.marks;
  std::swap(reordered[0], reordered[1]);
  EXPECT_EQ(document.document()->characters.routes.front().marks, reordered);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  activate("Properties", "Remove mark", 0);
  EXPECT_EQ(document.document()->characters.routes.front().marks.size(),
            route.marks.size() - 1);
  key(ImGuiKey_Z, true);
  activate("Properties", "Add route mark");
  choosePopup("start");
  EXPECT_EQ(document.document()->characters.routes.front().marks.back(),
            "start");
  EXPECT_EQ(document.document()->characters.routes.front().marks.size(),
            route.marks.size() + 1);
  key(ImGuiKey_Z, true);
  activate("Properties", "Final clip");
  choosePopup("None");
  EXPECT_FALSE(document.document()->characters.routes.front().final_clip);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorUiInteraction,
       CharacterDraftRenameCommitsBeforeSelectingInitialMark) {
  ASSERT_TRUE(document.open("resources/levels/scripted-characters.level.json"));
  const auto actor_id =
      document.characterIds(EditorCharacterKind::Actor).front();
  document.select(actor_id);
  frame();
  const auto original = *document.document();
  activate("Properties", "Actor ID");
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("renamed-walker");
  frame();
  key(ImGuiKey_D, true);
  EXPECT_EQ(*document.document(), original);
  activate("Properties", "Select/edit initial mark");
  EXPECT_EQ(document.document()->characters.actors.front().id,
            "renamed-walker");
  EXPECT_EQ(document.document()->characters.routes.front().actor,
            "renamed-walker");
  ASSERT_TRUE(std::holds_alternative<CharacterMarkDefinition>(
      *document.object(document.selection())));
  EXPECT_EQ(
      std::get<CharacterMarkDefinition>(*document.object(document.selection()))
          .id,
      original.characters.actors.front().initial_mark);
  const auto logged = loggedFrame();
  EXPECT_NE(logged.find("Actor renamed-walker: initial mark"),
            std::string::npos);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorUiInteraction,
       CharacterSpeedRejectsNonFiniteAndRetainsRepairableEdits) {
  ASSERT_TRUE(document.open("resources/levels/scripted-characters.level.json"));
  document.select(document.characterIds(EditorCharacterKind::Actor).front());
  frame();
  const auto original = *document.document();
  const auto* properties = ImGui::FindWindowByName("Properties");
  const ImVec2 speed{properties->Pos.x + properties->WindowPadding.x + 40,
                     properties->Pos.y + properties->TitleBarHeight +
                         properties->WindowPadding.y +
                         2 * ImGui::GetFrameHeightWithSpacing() + 8};
  for (const auto* value : {"1e39", "-1"}) {
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
    click(speed);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
    frame();
    ImGui::GetIO().AddInputCharactersUTF8(value);
    frame();
    key(ImGuiKey_Enter);
    if (value[0] == '1') {
      EXPECT_EQ(*document.document(), original);
      EXPECT_FALSE(document.editError().empty());
      EXPECT_FALSE(document.dirty());
    } else {
      EXPECT_FLOAT_EQ(document.document()->characters.actors.front().speed, -1);
      EXPECT_FALSE(document.valid());
      EXPECT_TRUE(document.dirty());
    }
  }
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorCharacterUiInteraction,
       ClipControlsAndScrubbingKeepAuthoredStateAndHistoryClean) {
  const auto original = *document.document();
  const auto revision = document.revision();
  const auto selection_revision = document.selectionRevision();
  ASSERT_GT(original.characters.marks.size(), 1U);
  const auto inspection_mark = original.characters.marks.back();
  inspect("Inspection mark");
  choosePopup(inspection_mark.id.c_str());
  inspect("Preview clip");
  choosePopup("interact");
  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  EXPECT_EQ(preview.clip(), "interact");
  EXPECT_EQ(preview.actor(), actor);
  const std::array<float, 3> expected_position{inspection_mark.feet_position.x,
                                               inspection_mark.feet_position.y,
                                               inspection_mark.feet_position.z};
  EXPECT_EQ(preview.placement().position, expected_position);
  inspect("Pause preview");
  ASSERT_TRUE(preview.paused());
  const auto paused_time = preview.time();
  const auto paused_pose = preview.pose();
  frame();
  frame();
  EXPECT_DOUBLE_EQ(preview.time(), paused_time);
  EXPECT_EQ(preview.pose(), paused_pose);

  numericInput("Character inspection", "Clip time");
  expectCapturedNavigation();
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("0.5");
  frame();
  key(ImGuiKey_D, true);  // Input owns this shortcut; no actor duplication.
  key(ImGuiKey_Enter);
  ASSERT_TRUE(preview.paused());
  EXPECT_NEAR(preview.time(), .5, .001);
  EXPECT_NE(preview.pose(), paused_pose);
  const auto scrubbed_pose = preview.pose();
  frame();
  EXPECT_EQ(preview.pose(), scrubbed_pose);

  inspect("Preview clip");
  choosePopup("walk");
  ASSERT_TRUE(preview.active());
  EXPECT_TRUE(preview.paused());
  EXPECT_EQ(preview.clip(), "walk");
  inspect("Restart preview");
  EXPECT_DOUBLE_EQ(preview.time(), 0);
  inspect("Resume preview");
  EXPECT_FALSE(preview.paused());
  EXPECT_GT(preview.time(), 0);
  inspect("Stop preview");
  EXPECT_FALSE(preview.active());
  frame();
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(document.selection(), actor);
  EXPECT_EQ(document.selectionRevision(), selection_revision);
  EXPECT_EQ(document.revision(), revision);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
  EXPECT_FALSE(document.canUndo());
  EXPECT_FALSE(document.canRedo());
}

TEST_F(EditorCharacterUiInteraction,
       FieldFocusCommitAndHistoryStopSnapshotsWithoutLeakingInput) {
  const auto original = *document.document();
  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  activate("Properties", "Actor ID");
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("inspected-walker");
  frame();
  expectCapturedNavigation();
  key(ImGuiKey_D, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
  ASSERT_TRUE(preview.active());
  key(ImGuiKey_Enter);
  frame();
  ASSERT_FALSE(preview.active());
  const auto renamed = *document.document();
  EXPECT_EQ(renamed.characters.actors.front().id, "inspected-walker");
  EXPECT_EQ(renamed.characters.routes.front().actor, "inspected-walker");
  EXPECT_TRUE(document.dirty());

  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  key(ImGuiKey_Z, true);
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
  frame();
  EXPECT_FALSE(preview.active());
  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  key(ImGuiKey_Y, true);
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(*document.document(), renamed);
  EXPECT_EQ(document.selection(), actor);
  frame();
  EXPECT_FALSE(preview.active());
}

TEST_F(EditorCharacterUiInteraction,
       SelectingAnotherRecordCommitsDraftAndRequiresExplicitSnapshotStart) {
  const auto original = *document.document();
  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  activate("Properties", "Actor ID");
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("selected-walker");
  frame();
  selectCharacter(route, "Route: ");
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(document.document()->characters.actors.front().id,
            "selected-walker");
  EXPECT_EQ(document.document()->characters.routes.front().actor,
            "selected-walker");
  EXPECT_TRUE(document.dirty());
  frame();
  EXPECT_FALSE(preview.active());

  const auto renamed = *document.document();
  const auto text = loggedFrame();
  EXPECT_NE(text.find("no collision, door operation or sound"),
            std::string::npos);
  EXPECT_NE(text.find("Final action: interact"), std::string::npos);
  inspect("Start route snapshot");
  ASSERT_TRUE(preview.active());
  EXPECT_EQ(preview.mode(), EditorCharacterPreviewMode::Route);
  inspect("Pause preview");
  ASSERT_TRUE(preview.paused());
  const auto position = preview.placement().position;
  frame();
  EXPECT_EQ(preview.placement().position, position);
  inspect("Resume preview");
  EXPECT_FALSE(preview.paused());
  selectCharacter(actor, "Actor: ");
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(*document.document(), renamed);
  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  EXPECT_EQ(preview.mode(), EditorCharacterPreviewMode::Clip);
  EXPECT_EQ(preview.clip(), "idle");
  key(ImGuiKey_Z, true);
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
}

TEST_F(EditorCharacterUiInteraction,
       UnavailableResourcesAndBrokenRouteDisableExplicitStart) {
  const auto original = *document.document();
  resources_current = false;
  frame();
  inspect("Start clip snapshot");
  EXPECT_FALSE(preview.active());
  EXPECT_NE(loggedFrame().find("requires the current usable scene"),
            std::string::npos);
  resources_current = true;
  frame();
  EXPECT_FALSE(preview.active());
  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  selectCharacter(route, "Route: ");
  EXPECT_FALSE(preview.active());

  const auto missing_mark = document.characterIds(EditorCharacterKind::Mark)[1];
  const auto missing_id =
      std::get<CharacterMarkDefinition>(*document.object(missing_mark)).id;
  selectCharacter(missing_mark, "Mark: ");
  key(ImGuiKey_Delete);
  selectCharacter(route, "Route: ");
  const auto invalid = *document.document();
  EXPECT_NE(loggedFrame().find("missing mark '" + missing_id + "'"),
            std::string::npos);
  inspect("Start route snapshot");
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(*document.document(), invalid);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.dirty());
  selectCharacter(route, "Route: ");
  EXPECT_FALSE(preview.active());
  inspect("Start route snapshot");
  EXPECT_TRUE(preview.active());
}

TEST_F(EditorCharacterUiInteraction,
       CharacterDraftCommitsBeforeSelectingAnAudioRecord) {
  const auto original = *document.document();
  ASSERT_FALSE(original.audio.sources.empty());
  inspect("Start clip snapshot");
  ASSERT_TRUE(preview.active());
  activate("Properties", "Actor ID");
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("audio-selected-walker");
  frame();
  const auto source = document.audioIds(EditorAudioKind::Source).front();
  const auto label = "Source: " + original.audio.sources.front().id;
  activate("Audio authoring", label.c_str());
  EXPECT_EQ(document.selection(), source);
  EXPECT_FALSE(preview.active());
  EXPECT_EQ(document.document()->characters.actors.front().id,
            "audio-selected-walker");
  EXPECT_EQ(document.document()->characters.routes.front().actor,
            "audio-selected-walker");
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), original);
  EXPECT_EQ(document.selection(), actor);
  EXPECT_FALSE(document.dirty());
  EXPECT_FALSE(preview.active());
}
