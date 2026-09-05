#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "editor/editor_camera.hpp"
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
    static_cast<void>(ui.updateViewport(
        document, EditorCamera{}.frame(1600.0F / 900), navigating));
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
  for (const auto id : {editor_spawn, editor_first_light,
                        editor_first_light + 1, editor_prop}) {
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
                   top + 2 * row + 8};
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
  EXPECT_NE(document.document()->player_spawn.yaw_degrees,
            original.player_spawn.yaw_degrees);
  ASSERT_TRUE(document.undo());
  EXPECT_EQ(*document.document(), original);
  EXPECT_FALSE(document.canUndo());
}
