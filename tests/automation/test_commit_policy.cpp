#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <stdexcept>

#include "editor/automation/semantic_ui.hpp"
#include "editor/editor_ui.hpp"
#include "editor/editor_widget_metadata.hpp"

namespace editor_automation {
namespace {
class SemanticCommitPolicy : public testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800};
    io.DeltaTime = 1.0F / 60;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  }
  void TearDown() override { ImGui::DestroyContext(); }
  template<class Draw>
  void frame(Draw draw) {
    ImGui::NewFrame();
    semantic.beginFrame(document);
    EditorWidgets::Begin("Commit policy");
    draw();
    EditorWidgets::End();
    snapshot = semantic.finishFrame(document);
    ImGui::Render();
  }
  const Json& byKey(std::string_view key) const {
    for (const auto& item : snapshot.items)
      if (item.record["key"] == Json(key)) return item.record;
    throw std::runtime_error("Missing control: " + std::string(key));
  }
  EditorDocument document;
  SemanticUi semantic{"commit-policy-test"};
  SemanticSnapshot snapshot;
};

TEST_F(SemanticCommitPolicy, ImmediateHandlerPolicyKeepsExplicitInputGesturesAndIsConsumedOnce) {
  float seek = 1, offset = 2, draft = 3, enter = 4;
  const auto draw = [&] {
    EditorWidgetMetadata::next("seek", "character.time", "s", "preview", true);
    EditorWidgets::SliderFloat("Seek", &seek, 0, 10);
    EditorWidgetMetadata::next("offset", {}, "m", "object", true);
    EditorWidgets::InputFloat("Offset", &offset);
    EditorWidgets::InputFloat("Draft", &draft);
    EditorWidgets::InputFloat("Enter", &enter, 0, 0, "%.3f",
                              ImGuiInputTextFlags_EnterReturnsTrue);
  };
  frame(draw); frame(draw);
  EXPECT_EQ(byKey("seek")["commit"]["policy"], "immediate");
  EXPECT_EQ(byKey("seek")["applied_binding"]["projection"], "preview");
  EXPECT_EQ(byKey("offset")["commit"]["policy"], "immediate");
  EXPECT_EQ(byKey("offset")["commit"]["methods"], (Json{"enter", "tab"}));
  EXPECT_TRUE(byKey("offset")["applied_binding"].is_null());
  EXPECT_EQ(byKey("offset")["value"]["unit"], "m");
  EXPECT_EQ(byKey("draft")["commit"]["policy"], "deactivate");
  EXPECT_EQ(byKey("enter")["commit"]["policy"], "enter");
}

TEST_F(SemanticCommitPolicy, CompoundComponentsInheritImmediatePolicyWithoutAffectingFollowingControls) {
  float immediate[3]{1, 2, 3}, deferred[3]{4, 5, 6};
  const auto draw = [&] {
    EditorWidgetMetadata::next("immediate", {}, "m", "object", true);
    EditorWidgets::DragFloat3("Immediate", immediate);
    EditorWidgets::DragFloat3("Deferred", deferred);
  };
  frame(draw); frame(draw);
  for (const auto& component : byKey("immediate")["components"]) {
    const auto& record = semantic.resolve({{"ref", component["ref"]}}, snapshot).record;
    EXPECT_EQ(record["commit"]["policy"], "immediate");
    EXPECT_EQ(record["commit"]["methods"], (Json{"enter", "tab"}));
  }
  for (const auto& component : byKey("deferred")["components"])
    EXPECT_EQ(semantic.resolve({{"ref", component["ref"]}}, snapshot)
                  .record["commit"]["policy"], "deactivate");
}

TEST_F(SemanticCommitPolicy, ReplacingAnnotationAndStartingFrameClearPendingPolicy) {
  float value = 1;
  const auto draw = [&] {
    EditorWidgetMetadata::next("unused", {}, {}, "object", true);
    EditorWidgetMetadata::next("replacement");
    EditorWidgets::InputFloat("Replacement", &value);
    EditorWidgetMetadata::next("unused-next-frame", {}, {}, "object", true);
  };
  frame(draw); frame(draw);
  EXPECT_EQ(byKey("replacement")["commit"]["policy"], "deactivate");
  frame([&] { EditorWidgets::InputFloat("Next frame", &value); });
  EXPECT_EQ(byKey("next-frame")["commit"]["policy"], "deactivate");
}

class ActualEditorCommitPolicy : public SemanticCommitPolicy {
 protected:
  void SetUp() override {
    SemanticCommitPolicy::SetUp();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {1600, 1000};
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    ASSERT_TRUE(document.open("resources/levels/prototype.level.json"));
    editorFrame(); editorFrame();
  }
  void editorFrame() {
    ImGui::NewFrame();
    semantic.beginFrame(document);
    ui.draw(document);
    snapshot = semantic.finishFrame(document);
    ImGui::Render();
  }
  void enablePlacement() {
    const auto checkbox = semantic.resolve(
        {{"ref", byKey("place-on-surface")["ref"]}}, snapshot);
    ASSERT_EQ(checkbox.record["state"]["enabled"], true);
    ASSERT_EQ(checkbox.record["state"]["selected"], false);
    auto* window = ImGui::FindWindowByName(checkbox.window.c_str());
    ASSERT_NE(window, nullptr);
    // ImGui navigation activation executes the actual checkbox and EditorUi's
    // normal placement handler; no private EditorUi state is set by the test.
    ImGui::FocusWindow(window);
    ImGui::ActivateItemByID(checkbox.id);
    editorFrame(); editorFrame();
    ASSERT_EQ(byKey("place-on-surface")["state"]["selected"], true);
  }
  void expectImmediatePlacement(std::string_view key, std::string_view label) {
    const auto& record = byKey(key);
    EXPECT_EQ(record["label"], Json(label));
    EXPECT_EQ(record["kind"], "number");
    EXPECT_EQ(record["state"]["enabled"], true);
    EXPECT_EQ(record["value"]["availability"], "known");
    EXPECT_TRUE(record["value"]["draft"].is_number());
    EXPECT_EQ(record["value"]["unit"], "m");
    EXPECT_EQ(record["commit"]["policy"], "immediate");
    EXPECT_EQ(record["commit"]["methods"], (Json{"enter", "tab"}));
    EXPECT_TRUE(record["applied_binding"].is_null());
  }
  EditorUi ui;
};

TEST_F(ActualEditorCommitPolicy, LightPlacementInputsPublishImmediatePolicyAndUnits) {
  ASSERT_FALSE(document.lightIds().empty());
  document.select(document.lightIds().front());
  editorFrame(); editorFrame();
  const auto revision = document.revision();
  const auto dirty = document.dirty();
  const auto applied = *document.document();
  ASSERT_NO_FATAL_FAILURE(enablePlacement());
  expectImmediatePlacement("height-above-floor-m", "Height above floor (m)");
  expectImmediatePlacement("wall-offset-m", "Wall offset (m)");
  EXPECT_EQ(byKey("ambient-0-to-0-20")["commit"]["policy"], "enter");
  EXPECT_EQ(document.revision(), revision);
  EXPECT_EQ(document.dirty(), dirty);
  EXPECT_EQ(*document.document(), applied);
}

TEST_F(ActualEditorCommitPolicy, DoorPlacementInputPublishesImmediatePolicyAndUnits) {
  // Document preparation is fixture setup; the placement action still uses
  // the real editor checkbox and its ordinary handler.
  ASSERT_TRUE(document.addDoor());
  editorFrame(); editorFrame();
  const auto revision = document.revision();
  const auto dirty = document.dirty();
  const auto applied = *document.document();
  ASSERT_NO_FATAL_FAILURE(enablePlacement());
  expectImmediatePlacement("door-floor-clearance-m", "Door floor clearance (m)");
  EXPECT_EQ(document.revision(), revision);
  EXPECT_EQ(document.dirty(), dirty);
  EXPECT_EQ(*document.document(), applied);
}
}  // namespace
}  // namespace editor_automation
