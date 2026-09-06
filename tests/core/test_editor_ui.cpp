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
    ui.draw(document);
    static_cast<void>(
        ui.updateViewport(document, camera.frame(1600.0F / 900), navigating));
    ui.finishFrame();
  }
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
  EditorDocument document;
  EditorUi ui;
  EditorCamera camera;
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
       FixedObjectsAndCameraNavigationSuppressMutationShortcuts) {
  const auto original = *document.document();
  for (const auto id : {editor_first_light, editor_first_light + 1}) {
    document.select(id);
    frame();
    key(ImGuiKey_Delete);
    key(ImGuiKey_D, true);
    EXPECT_EQ(*document.document(), original);
  }
  document.select(document.solidIds()[0]);
  key(ImGuiKey_Delete, false, true);
  key(ImGuiKey_D, true, true);
  EXPECT_EQ(*document.document(), original);
}

TEST_F(EditorUiInteraction,
       DoorButtonAndKeyboardEditsRetainIdentityThroughUndo) {
  const auto start_count = document.doorIds().size();
  const auto add = addButtonCenter();
  click({add.x, add.y + 2 * ImGui::GetFrameHeightWithSpacing()});
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
  click({add.x, add.y + row});
  ASSERT_EQ(document.selection(), editor_light_switch);
  click({add.x, add.y + 4 * row});
  click({800, 700});
  ASSERT_TRUE(document.document()->light_switch);
  EXPECT_FLOAT_EQ(document.document()->light_switch->position.y, 1.4F);
  EXPECT_NE(document.document()->light_switch->position.z, 2.0F);
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
      {objects->Pos.x + 30, top + 4 * ImGui::GetFrameHeightWithSpacing() + 8});
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

TEST_F(EditorUiInteraction, SwitchButtonsPropertiesAndInputCapture) {
  document.select(editor_light_switch);
  frame();
  key(ImGuiKey_D, true);
  EXPECT_FALSE(document.dirty());
  key(ImGuiKey_Delete, false, true);
  EXPECT_TRUE(document.document()->light_switch);
  key(ImGuiKey_Delete);
  ASSERT_FALSE(document.document()->light_switch);
  const auto* objects = ImGui::FindWindowByName("Objects");
  const float object_top =
      objects->Pos.y + objects->TitleBarHeight + objects->WindowPadding.y;
  const float row = ImGui::GetFrameHeightWithSpacing();
  click({objects->Pos.x + 60, object_top + row + 8});
  ASSERT_TRUE(document.document()->light_switch);
  EXPECT_EQ(document.selection(), editor_light_switch);
  const auto created = *document.document();
  click({objects->Pos.x + 60, object_top + row + 8});
  EXPECT_EQ(*document.document(), created);
  const auto* properties = ImGui::FindWindowByName("Properties");
  const float top = properties->Pos.y + properties->TitleBarHeight +
                    properties->WindowPadding.y;
  click({properties->Pos.x + 40, top + 8});  // collapse Terrain
  click({properties->Pos.x + 40, top + 4 * row + 8});
  ASSERT_FALSE(document.document()->light_switch->initially_on);
  EXPECT_FALSE(document.terrainStrokeActive());
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), created);
  // Select Point light 2 through the real combo popup.
  click({properties->Pos.x + 70, top + 3 * row + 8});
  frame();
  frame();
  const ImGuiWindow* popup = nullptr;
  for (auto* window : ImGui::GetCurrentContext()->Windows)
    if (window->Active && (window->Flags & ImGuiWindowFlags_Popup))
      popup = window;
  ASSERT_NE(popup, nullptr);
  click({popup->Pos.x + 60, popup->Pos.y + popup->WindowPadding.y +
                                ImGui::GetTextLineHeightWithSpacing() + 6});
  ASSERT_EQ(document.document()->light_switch->point_light_index, 1U);
  key(ImGuiKey_Z, true);
  EXPECT_EQ(*document.document(), created);
  // A yaw drag edits a draft and commits once when released.
  const ImVec2 yaw{properties->Pos.x + 40, top + 2 * row + 8};
  ImGui::GetIO().AddMousePosEvent(yaw.x, yaw.y);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, true);
  frame();
  ImGui::GetIO().AddMousePosEvent(yaw.x + 40, yaw.y);
  frame();
  EXPECT_EQ(*document.document(), created);
  ImGui::GetIO().AddMouseButtonEvent(0, false);
  frame();
  EXPECT_NE(document.document()->light_switch->yaw_degrees,
            created.light_switch->yaw_degrees);
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
      EXPECT_FLOAT_EQ(document.document()->light_switch->yaw_degrees, 45);
    }
  }
}
