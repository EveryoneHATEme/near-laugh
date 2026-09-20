#include <gtest/gtest.h>
#include <imgui_internal.h>
#include <imgui_te_context.h>

#include <functional>
#include <set>

#include "editor/automation/engine_session.hpp"
#include "editor/automation/semantic_ui.hpp"
#include "editor/automation/widgets.hpp"
#include "editor/editor_ui.hpp"
#include "editor/editor_widget_metadata.hpp"

namespace editor_automation {
namespace {
class SemanticEditorUi : public testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1600, 1000};
    io.DeltaTime = 1.0F / 60;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ASSERT_TRUE(document.open("resources/levels/prototype.level.json"));
    frame(); frame();
  }
  void TearDown() override { ImGui::DestroyContext(); }
  void frame() {
    ImGui::NewFrame();
    semantic.beginFrame(document);
    ui.draw(document);
    snapshot = semantic.finishFrame(document);
    ImGui::Render();
    Json items = Json::array();
    for (const auto& item : snapshot.items) items.push_back(item.record);
    const Json stamp{{"snapshot_id", "test"}, {"frame", "1"}, {"document_generation", "1"},
                     {"document_revision", "1"}, {"selection_revision", "1"},
                     {"preview_revision", "1"}, {"stale", false}};
    const Json result{{"protocol_version", 1}, {"request_id", "test"},
      {"session_id", "test"}, {"build_fingerprint", "test"}, {"ok", true},
      {"snapshot", stamp}, {"active_scope", snapshot.active_scope},
      {"modal_scope", snapshot.modal_scope}, {"items", items},
      {"coverage", snapshot.coverage}, {"next_cursor", nullptr}};
    EXPECT_NO_THROW(validateResult("ui_observe", result));
  }
  const SemanticItem& byKey(std::string_view key, std::string_view window = "Properties") {
    const SemanticItem* result{};
    for (const auto& item : snapshot.items)
      if (item.record["key"] == Json(key) && item.window == window) {
        if (result) throw std::runtime_error("Ambiguous test item");
        result = &item;
      }
    if (!result) throw std::runtime_error("Missing test item: " + std::string(key));
    return *result;
  }
  void activate(const SemanticItem& item, bool input = false) {
    auto* window = ImGui::FindWindowByName(item.window.c_str());
    ASSERT_NE(window, nullptr);
    ImGui::FocusWindow(window);
    ImGui::ActivateItemByID(item.id);
    if (input) ImGui::GetCurrentContext()->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
    frame(); frame();
  }
  void key(ImGuiKey key, bool control = false) {
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, control);
    io.AddKeyEvent(key, true); frame();
    io.AddKeyEvent(key, false); io.AddKeyEvent(ImGuiMod_Ctrl, false); frame();
  }
  EditorDocument document;
  EditorUi ui;
  SemanticUi semantic{"semantic-test"};
  SemanticSnapshot snapshot;
};

TEST_F(SemanticEditorUi, ActualEditorFramesPublishOwnersBindingsComponentsAndViewportExclusion) {
  const auto id = document.solidIds().front();
  document.select(id); frame();
  const auto& center = byKey("center");
  EXPECT_EQ(center.record["owner"], snapshot.object_refs.at(id));
  EXPECT_EQ(center.record["applied_binding"]["field"], "center");
  ASSERT_EQ(center.record["components"].size(), 3U);
  const auto x = semantic.resolve({{"ref", center.record["components"][0]["ref"]}}, snapshot);
  EXPECT_EQ(x.record["applied_binding"]["field"], "center.x");
  EXPECT_NE(x.id, center.id);
  EXPECT_EQ(x.record["value"]["draft"], std::get<PrototypeSolid>(*document.object(id)).center.x);
  EXPECT_EQ(byKey("tint").record["value"]["type"], "color");
  EXPECT_EQ(center.record["engine"]["availability"], "unknown");
  EXPECT_FALSE(center.engine_geometry.has_value());
  const auto& viewport = byKey("scene-viewport", "Debug##Default");
  EXPECT_TRUE(viewport.record["capabilities"].empty());
  EXPECT_TRUE(viewport.record.contains("unsupported_reason"));
  EXPECT_EQ(snapshot.scope_parents.at(center.record["scope"].get<std::string>()), "root");
}

TEST_F(SemanticEditorUi, PassiveObservationPreservesFocusedUncommittedTextAndAppliedDocument) {
  const auto id = document.entryIds().front();
  document.select(id); frame();
  const auto original = std::get<LevelEntry>(*document.object(id)).id;
  activate(byKey("entry-id"));
  const auto active = ImGui::GetActiveID();
  ASSERT_EQ(active, byKey("entry-id").id);
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("renamed-entry");
  frame();
  const auto before = byKey("entry-id").record;
  const auto revision = document.revision();
  frame(); frame();
  EXPECT_EQ(ImGui::GetActiveID(), active);
  EXPECT_EQ(document.revision(), revision);
  EXPECT_EQ(std::get<LevelEntry>(*document.object(id)).id, original);
  EXPECT_EQ(byKey("entry-id").record["input"]["text"], "renamed-entry");
  EXPECT_TRUE(byKey("entry-id").record["input"]["uncommitted"].get<bool>());
  EXPECT_EQ(byKey("entry-id").record["value"], before["value"]);
  key(ImGuiKey_Tab);
  EXPECT_EQ(std::get<LevelEntry>(*document.object(id)).id, "renamed-entry");
}

TEST_F(SemanticEditorUi, OpaqueReferencesSurviveRenameAndInvalidateDeletionAndGeneration) {
  const auto id = document.entryIds().front();
  document.select(id); frame();
  const auto owner = snapshot.object_refs.at(id);
  const auto ref = byKey("entry-id").record["ref"];
  auto entry = std::get<LevelEntry>(*document.object(id));
  entry.id = "changed-name";
  ASSERT_TRUE(document.replaceObject(id, entry));
  frame();
  EXPECT_EQ(snapshot.object_refs.at(id), owner);
  EXPECT_EQ(byKey("entry-id").record["ref"], ref);
  ASSERT_TRUE(document.addEntry({{2, 0, 2}, 0}));
  const auto added = document.selection(); frame();
  const auto deleted_ref = byKey("entry-id").record["ref"];
  ASSERT_TRUE(document.removeSelected()); frame();
  EXPECT_FALSE(snapshot.object_refs.contains(added));
  EXPECT_THROW(static_cast<void>(semantic.resolve({{"ref", deleted_ref}}, snapshot)), ProtocolError);
  ASSERT_TRUE(document.open("resources/levels/prototype.level.json"));
  document.select(document.entryIds().front()); frame();
  EXPECT_NE(byKey("entry-id").record["ref"], ref);
  EXPECT_THROW(static_cast<void>(semantic.resolve({{"ref", ref}}, snapshot)), ProtocolError);
}

TEST_F(SemanticEditorUi, ExactSelectorsRejectAmbiguousObjectRowsAndAcceptSelectionOwner) {
  const auto id = document.solidIds().front(); document.select(id); frame();
  const auto& selected = byKey("center");
  const Json target{{"selector", {{"scope", selected.record["scope"]}, {"key", "center"}, {"owner", "selection"}}}};
  EXPECT_EQ(semantic.resolve(target, snapshot).record["ref"], selected.record["ref"]);
  const auto object_scope = byKey("window", "Objects").record["scope"];
  try {
    static_cast<void>(semantic.resolve({{"selector", {{"scope", object_scope}, {"key", "select"}}}}, snapshot));
    FAIL() << "Expected ambiguity";
  } catch (const ProtocolError& error) { EXPECT_EQ(error.code(), "ambiguous_target"); }
}

TEST_F(SemanticEditorUi, BuiltinComboPublishesRealOptionsOnlyWhileOpenWithParentTopology) {
  document.select(document.solidIds().front()); frame();
  const auto ref = byKey("kind").record["ref"];
  activate(byKey("kind"));
  ASSERT_TRUE(byKey("kind").record["state"]["open"].get<bool>());
  const SemanticItem* option{};
  for (const auto& item : snapshot.items) if (item.record["key"] == "option-1") option = &item;
  ASSERT_NE(option, nullptr);
  EXPECT_EQ(Json(snapshot.scope_items.at(option->record["scope"].get<std::string>())), ref);
  EXPECT_NE(option->id, 0U);
  activate(*option);
  EXPECT_EQ(std::get<PrototypeSolid>(*document.object(document.selection())).kind, PrototypeSolidKind::Boundary);
  for (const auto& item : snapshot.items) EXPECT_NE(item.record["key"], "option-1");
}

TEST_F(SemanticEditorUi, ActualNumericComponentInputExposesIncompleteTextWithoutInventingAValue) {
  const auto id = document.solidIds().front(); document.select(id); frame();
  const auto original = document.object(id);
  activate(byKey("center.x"), true);
  ASSERT_EQ(ImGui::GetActiveID(), byKey("center.x").id);
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("-"); frame();
  EXPECT_EQ(byKey("center.x").record["input"]["text"], "-");
  EXPECT_EQ(byKey("center.x").record["value"]["availability"], "known");
  EXPECT_EQ(byKey("center.x").record["input_validation"]["status"], "invalid");
  EXPECT_FLOAT_EQ(byKey("center.x").record["value"]["draft"].get<float>(),
                  std::get<PrototypeSolid>(*original).center.x);
  EXPECT_EQ(document.object(id), original);
  frame();
  EXPECT_EQ(byKey("center.x").record["input"]["text"], "-");
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("3.125"); frame();
  EXPECT_EQ(byKey("center.x").record["input"]["text"], "3.125");
  // Pinned TempInputScalar applies its float when editing ends. The copied
  // typed draft must remain the actual retained float until that moment.
  EXPECT_FLOAT_EQ(byKey("center.x").record["value"]["draft"].get<float>(),
                  std::get<PrototypeSolid>(*original).center.x);
  key(ImGuiKey_Tab);
  EXPECT_FLOAT_EQ(std::get<PrototypeSolid>(*document.object(id)).center.x, 3.125F);
}

TEST_F(SemanticEditorUi, ActualColorChildInputUsesGuiUnitsAndCommitsRoundedRgb) {
  const auto id = document.solidIds().front(); document.select(id); frame();
  activate(byKey("tint.r"), true);
  ASSERT_EQ(ImGui::GetActiveID(), byKey("tint.r").id);
  key(ImGuiKey_A, true);
  ImGui::GetIO().AddInputCharactersUTF8("127"); frame();
  EXPECT_EQ(byKey("tint.r").record["value"]["type"], "integer");
  EXPECT_EQ(byKey("tint.r").record["input"]["text"], "127");
  key(ImGuiKey_Tab);
  EXPECT_EQ(std::get<PrototypeSolid>(*document.object(id)).color[0], 127);
}

TEST_F(SemanticEditorUi, IndexOnlyCollisionRowsInvalidateOnArrayChangesButNotOwnerScalarEdits) {
  const auto id = document.propIds().front(); document.select(id);
  auto prop = std::get<PrototypeStaticProp>(*document.object(id));
  prop.collision_boxes = {{{0, .5F, 0}, {.1F, .1F, .1F}}, {{1, .5F, 0}, {.1F, .1F, .1F}}};
  ASSERT_TRUE(document.replaceObject(id, prop)); frame();
  const auto row_ref = byKey("collision_boxes[0]/proxy-center").record["ref"];
  prop.yaw_degrees += 5;
  ASSERT_TRUE(document.replaceObject(id, prop)); frame();
  EXPECT_EQ(byKey("collision_boxes[0]/proxy-center").record["ref"], row_ref);
  std::swap(prop.collision_boxes[0], prop.collision_boxes[1]);
  ASSERT_TRUE(document.replaceObject(id, prop)); frame();
  EXPECT_NE(byKey("collision_boxes[0]/proxy-center").record["ref"], row_ref);
  EXPECT_THROW(static_cast<void>(semantic.resolve({{"ref", row_ref}}, snapshot)), ProtocolError);
  const auto reordered_ref = byKey("collision_boxes[0]/proxy-center").record["ref"];
  // Equal-sized replacement and an edit combined with reordering must never
  // redirect a ref to a different row. Conservatively invalidate any array edit.
  std::swap(prop.collision_boxes[0], prop.collision_boxes[1]);
  prop.collision_boxes[0].center.x += 2;
  ASSERT_TRUE(document.replaceObject(id, prop)); frame();
  EXPECT_NE(byKey("collision_boxes[0]/proxy-center").record["ref"], reordered_ref);
  EXPECT_THROW(static_cast<void>(semantic.resolve({{"ref", reordered_ref}}, snapshot)), ProtocolError);
}

TEST_F(SemanticEditorUi, ExplicitStructuralGestureInvalidatesEvenIdenticalRows) {
  const auto id = document.propIds().front(); document.select(id);
  auto prop = std::get<PrototypeStaticProp>(*document.object(id));
  prop.collision_boxes = {{{0, .5F, 0}, {.1F, .1F, .1F}}, {{0, .5F, 0}, {.1F, .1F, .1F}}};
  ASSERT_TRUE(document.replaceObject(id, prop)); frame();
  const auto ref = byKey("collision_boxes[0]/proxy-center").record["ref"];
  ImGui::NewFrame(); semantic.beginFrame(document);
  semantic.pushOwner(id); semantic.structureChanged("collision_boxes"); semantic.popOwner();
  ui.draw(document); snapshot = semantic.finishFrame(document); ImGui::Render();
  EXPECT_NE(byKey("collision_boxes[0]/proxy-center").record["ref"], ref);
  EXPECT_THROW(static_cast<void>(semantic.resolve({{"ref", ref}}, snapshot)), ProtocolError);
}

class EngineSemanticUi : public SemanticEditorUi {
 protected:
  void SetUp() override {
    SemanticEditorUi::SetUp();
    engine.start();
    for (int i = 0; i != 8; ++i) frame();
  }
  void TearDown() override {
    engine.stop();
    SemanticEditorUi::TearDown();
  }
  template<class Draw>
  void controls(Draw draw) {
    ImGui::NewFrame(); semantic.beginFrame(document);
    widgets::Begin("Engine facts"); draw(); widgets::End();
    ImGui::Render(); snapshot = semantic.finishFrame(document);
  }
  void perform(std::function<void(ImGuiTestContext*)> setup) {
    setup_ = std::move(setup); setup_complete_ = false;
    engine.setDispatcher([](void* owner, ImGuiTestContext* context) {
      auto& test = *static_cast<EngineSemanticUi*>(owner);
      if (!test.setup_) return;
      auto setup = std::move(test.setup_);
      setup(context);
      test.setup_complete_ = true;
    }, this);
    for (int i = 0; i != 240 && !setup_complete_; ++i) frame();
    engine.setDispatcher(nullptr, nullptr);
    EXPECT_TRUE(setup_complete_);
    EXPECT_EQ(engine.snapshot().state, EngineSessionState::Running);
  }
  EngineSession engine;
  std::function<void(ImGuiTestContext*)> setup_;
  bool setup_complete_{};
};

TEST_F(EngineSemanticUi, RealEngineFactsAreCurrentAndOverrideDisabledMetadata) {
  bool checked = true;
  const auto draw = [&] {
    widgets::BeginDisabled();
    widgets::Checkbox("Checked", &checked);
    widgets::MenuItem("Disabled through outer scope");
    widgets::EndDisabled();
    widgets::Selectable("Disabled by widget flag", false, ImGuiSelectableFlags_Disabled);
  };
  controls(draw); controls(draw); controls(draw);
  const auto& check = byKey("checked", "Engine facts");
  EXPECT_EQ(check.record["engine"]["availability"], "known");
  EXPECT_EQ(check.record["engine"]["frame"], std::to_string(ImGui::GetFrameCount()));
  EXPECT_EQ(check.record["engine"]["status_frame"], std::to_string(ImGui::GetFrameCount()));
  EXPECT_EQ(check.record["provenance"]["state"], "test_engine");
  EXPECT_TRUE(check.record["state"]["selected"].get<bool>());
  ASSERT_TRUE(check.engine_geometry.has_value());
  EXPECT_EQ(check.engine_geometry->frame, ImGui::GetFrameCount());
  EXPECT_GT(check.engine_geometry->max.x, check.engine_geometry->min.x);
  for (const auto* key : {"checked", "disabled-through-outer-scope", "disabled-by-widget-flag"})
    EXPECT_FALSE(byKey(key, "Engine facts").record["state"]["enabled"].get<bool>());
  EXPECT_FALSE(check.record["engine"].contains("id"));
  EXPECT_FALSE(check.record["engine"].contains("rect"));
}

TEST_F(EngineSemanticUi, MissingCurrentStatusNeverReusesPreviouslyCheckedFlag) {
  bool checked = true;
  const auto draw = [&] { widgets::Checkbox("Checked", &checked); };
  controls(draw); controls(draw); controls(draw);
  auto metadata = byKey("checked", "Engine facts").record;
  ASSERT_TRUE(metadata["state"]["selected"].get<bool>());
  const auto ref = metadata["ref"];
  metadata["state"]["selected"] = false;
  controls([&] {
    // A real ItemAdd submits current geometry but no ItemInfo status callback.
    // A concrete custom adapter may still know its state independently.
    auto* window = ImGui::GetCurrentWindow();
    const auto id = ImGui::GetID("Checked");
    const ImVec2 start = window->DC.CursorPos;
    const ImRect bounds(start, ImVec2(start.x + 100, start.y + 20));
    ImGui::ItemSize(bounds); ImGui::ItemAdd(bounds, id);
    static_cast<void>(semantic.record(metadata, id, window->Name, "checked"));
  });
  const auto& observed = byKey("checked", "Engine facts");
  EXPECT_EQ(observed.record["ref"], ref);
  EXPECT_EQ(observed.record["engine"]["availability"], "known");
  EXPECT_FALSE(observed.record["engine"].contains("status_frame"));
  EXPECT_TRUE(observed.record["engine"].contains("reason"));
  EXPECT_FALSE(observed.record["state"]["selected"].get<bool>());
}

TEST_F(EngineSemanticUi, UnsubmittedItemDoesNotReuseStaleEngineFacts) {
  const auto draw = [&] { widgets::Button("Conditional"); };
  controls(draw); controls(draw); controls(draw);
  auto metadata = byKey("conditional", "Engine facts").record;
  const auto ref = metadata["ref"];
  controls([&] {
    auto* window = ImGui::GetCurrentWindow();
    metadata["state"]["submitted"] = false;
    static_cast<void>(semantic.record(metadata, ImGui::GetID("Conditional"), window->Name, "conditional"));
  });
  const auto& observed = byKey("conditional", "Engine facts");
  EXPECT_EQ(observed.record["ref"], ref);
  EXPECT_EQ(observed.record["engine"]["availability"], "unknown");
  EXPECT_FALSE(observed.engine_geometry.has_value());
  EXPECT_FALSE(observed.record["state"]["submitted"].get<bool>());
}

TEST_F(EngineSemanticUi, UnsupportedColorModesPublishNoInventedRgbComponents) {
  float color[]{.25F, .5F, .75F};
  for (const auto mode : {ImGuiColorEditFlags_NoInputs, ImGuiColorEditFlags_DisplayHSV,
                          ImGuiColorEditFlags_DisplayHex, ImGuiColorEditFlags_InputHSV}) {
    SCOPED_TRACE(mode);
    controls([&] { widgets::ColorEdit3("Configured color", color, mode | ImGuiColorEditFlags_NoPicker); });
    const auto& observed = byKey("configured-color", "Engine facts").record;
    EXPECT_EQ(observed["kind"], "color");
    EXPECT_EQ(observed["value"]["availability"], "unavailable");
    EXPECT_TRUE(observed.contains("unsupported_reason"));
    EXPECT_EQ(observed["capabilities"], Json::array({"assert"}));
    EXPECT_FALSE(observed.contains("components"));
    for (const auto& item : snapshot.items)
      EXPECT_FALSE(item.record["key"].get<std::string>().starts_with("configured-color."));
  }
}

TEST_F(EngineSemanticUi, InheritedColorModesAreCheckedBeforeAdvertisingComponentActions) {
  float color[]{.25F, .5F, .75F};
  const auto initial = ImGui::GetIO().ConfigColorEditFlags;
  for (const auto mode : {ImGuiColorEditFlags_DisplayHSV, ImGuiColorEditFlags_DisplayHex,
                          ImGuiColorEditFlags_InputHSV}) {
    SCOPED_TRACE(mode);
    const auto mask = mode == ImGuiColorEditFlags_InputHSV ?
        ImGuiColorEditFlags_InputMask_ : ImGuiColorEditFlags_DisplayMask_;
    ImGui::GetIO().ConfigColorEditFlags = (initial & ~mask) | mode;
    controls([&] { widgets::ColorEdit3("Configured color", color, ImGuiColorEditFlags_NoPicker); });
    const auto& unsupported = byKey("configured-color", "Engine facts").record;
    EXPECT_TRUE(unsupported.contains("unsupported_reason"));
    EXPECT_FALSE(unsupported.contains("components"));
    for (const auto& item : snapshot.items)
      EXPECT_FALSE(item.record["key"].get<std::string>().starts_with("configured-color."));

    // Concrete widget flags override global defaults, as in the real widget.
    controls([&] { widgets::ColorEdit3("Configured color", color, ImGuiColorEditFlags_NoPicker |
        ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB); });
    const auto& supported = byKey("configured-color", "Engine facts").record;
    EXPECT_FALSE(supported.contains("unsupported_reason"));
    EXPECT_EQ(supported.at("components").size(), 3U);
  }
  ImGui::GetIO().ConfigColorEditFlags = initial;
}

TEST_F(EngineSemanticUi, PassiveEngineFramesPreserveRealEditorDraftClipboardAndHistory) {
  // Prepare a document with both undo and redo available before exercising UI.
  ASSERT_TRUE(document.setAmbient(.07F)); ASSERT_TRUE(document.setAmbient(.08F));
  ASSERT_TRUE(document.undo());
  const auto id = document.entryIds().front(); document.select(id); frame(); frame();
  const auto original = std::get<LevelEntry>(*document.object(id)).id;
  const auto field = byKey("entry-id");
  perform([target = field.id](ImGuiTestContext* context) {
    context->ItemInput(ImGuiTestRef(target));
    context->KeyCharsReplace("uncommitted-engine-entry");
  });
  frame();
  ASSERT_TRUE(engine.snapshot().dispatcher_active);
  ASSERT_EQ(ImGui::GetActiveID(), field.id);
  ASSERT_EQ(byKey("entry-id").record["input"]["text"], "uncommitted-engine-entry");
  ASSERT_EQ(byKey("entry-id").record["engine"]["availability"], "known");
  // A running Test Engine installs a private clipboard; no OS clipboard access.
  ImGui::SetClipboardText("passive-read-sentinel");
  const auto revision = document.revision();
  const auto dirty = document.dirty(), undo = document.canUndo(), redo = document.canRedo();
  const auto active = ImGui::GetActiveID(), focused = ImGui::GetCurrentContext()->NavId;
  const auto popup_count = ImGui::GetCurrentContext()->OpenPopupStack.Size;
  const auto before = byKey("entry-id").record;
  const auto frame_before = ImGui::GetFrameCount();
  for (int i = 0; i != 6; ++i) {
    frame();
    const auto& observed = byKey("entry-id").record;
    EXPECT_EQ(observed["ref"], before["ref"]);
    EXPECT_EQ(observed["input"], before["input"]);
    EXPECT_EQ(observed["value"], before["value"]);
    EXPECT_EQ(observed["engine"]["frame"], std::to_string(ImGui::GetFrameCount()));
    EXPECT_EQ(ImGui::GetActiveID(), active);
    EXPECT_EQ(ImGui::GetCurrentContext()->NavId, focused);
    EXPECT_EQ(ImGui::GetCurrentContext()->OpenPopupStack.Size, popup_count);
    EXPECT_STREQ(ImGui::GetClipboardText(), "passive-read-sentinel");
    EXPECT_EQ(document.revision(), revision); EXPECT_EQ(document.dirty(), dirty);
    EXPECT_EQ(document.canUndo(), undo); EXPECT_EQ(document.canRedo(), redo);
    EXPECT_EQ(std::get<LevelEntry>(*document.object(id)).id, original);
  }
  EXPECT_GT(ImGui::GetFrameCount(), frame_before);
}

TEST_F(EngineSemanticUi, PassiveEngineFramesKeepModalOpenAndComboClosed) {
  bool open_once = true;
  char text[32] = "Popup draft";
  const auto draw = [&] {
    if (open_once) { widgets::OpenPopup("Passive popup"); open_once = false; }
    if (widgets::BeginPopupModal("Passive popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      widgets::InputText("Popup field", text, sizeof(text));
      if (widgets::BeginCombo("Closed choices", "Alpha")) {
        widgets::Selectable("Alpha", true); widgets::EndCombo();
      }
      widgets::EndPopup();
    }
  };
  controls(draw); controls(draw); controls(draw);
  const auto modal = snapshot.modal_scope;
  ASSERT_FALSE(modal.is_null());
  const auto stack = ImGui::GetCurrentContext()->OpenPopupStack.Size;
  ASSERT_EQ(stack, 1);
  const auto popup_id = ImGui::GetCurrentContext()->OpenPopupStack.back().PopupId;
  const auto choices = byKey("closed-choices", "Passive popup").record;
  ASSERT_FALSE(choices["state"]["open"].get<bool>());
  const auto revision = document.revision(), selection = document.selection();
  const auto dirty = document.dirty(), undo = document.canUndo(), redo = document.canRedo();
  ASSERT_TRUE(engine.snapshot().dispatcher_active);
  ImGui::SetClipboardText("modal-observation-sentinel");
  for (int i = 0; i != 5; ++i) {
    controls(draw);
    EXPECT_EQ(snapshot.modal_scope, modal);
    EXPECT_EQ(ImGui::GetCurrentContext()->OpenPopupStack.Size, stack);
    EXPECT_EQ(ImGui::GetCurrentContext()->OpenPopupStack.back().PopupId, popup_id);
    EXPECT_EQ(byKey("closed-choices", "Passive popup").record["ref"], choices["ref"]);
    EXPECT_FALSE(byKey("closed-choices", "Passive popup").record["state"]["open"].get<bool>());
    EXPECT_STREQ(ImGui::GetClipboardText(), "modal-observation-sentinel");
    EXPECT_EQ(document.revision(), revision); EXPECT_EQ(document.selection(), selection);
    EXPECT_EQ(document.dirty(), dirty); EXPECT_EQ(document.canUndo(), undo); EXPECT_EQ(document.canRedo(), redo);
  }
}

TEST_F(SemanticEditorUi, EveryFixtureObjectKindProducesSchemaValidMetadata) {
  std::set<std::string> kinds;
  for (const auto* path : {"resources/levels/prototype.level.json",
                           "resources/levels/interior-lighting.level.json",
                           "resources/levels/audio-captions.level.json",
                           "resources/levels/scripted-characters.level.json",
                           "resources/levels/household-interactions.level.json"}) {
    ASSERT_TRUE(document.open(path)); frame();
    const auto owners = snapshot.object_refs;
    for (const auto& [id, ref] : owners) {
      (void)ref;
      const auto kind = objectRecordType(*document.object(id));
      if (kinds.contains(kind)) continue;
      document.select(id); frame();
      kinds.insert(kind);
      std::set<std::string> refs;
      for (const auto& item : snapshot.items) {
        EXPECT_TRUE(refs.insert(item.record["ref"].get<std::string>()).second)
            << item.record["key"] << " has a duplicate semantic identity";
      }
    }
  }
  EXPECT_EQ(kinds.size(), 16U);
}

TEST_F(SemanticEditorUi, ReadableProjectionCopiesTheDisplayedLastGoodPageAndDiagnostics) {
  ASSERT_TRUE(document.open("resources/levels/household-interactions.level.json"));
  const auto id = document.householdIds(EditorHouseholdKind::Document).front();
  document.select(id); frame();
  EXPECT_TRUE(ui.readablePreview(document).selected);
  EXPECT_FALSE(ui.readablePreview(document).available);
  ui.setReadableFont(std::make_shared<CaptionFont>("resources")); frame();
  const auto good = ui.readablePreview(document);
  ASSERT_TRUE(good.available);
  auto readable = std::get<HouseholdDocumentDefinition>(*document.object(id));
  EXPECT_EQ(good.title, readable.title);
  EXPECT_EQ(good.text, readable.pages.front());
  readable.pages[0] = std::string(480, 'W');
  ASSERT_TRUE(document.replaceObject(id, readable)); frame();
  const auto failed = ui.readablePreview(document);
  EXPECT_TRUE(failed.available);
  EXPECT_TRUE(failed.stale);
  EXPECT_FALSE(failed.layout_diagnostics.empty());
  EXPECT_EQ(failed.text, good.text);
  document.select(document.solidIds().front()); frame();
  EXPECT_FALSE(ui.readablePreview(document).selected);
}
}  // namespace
}  // namespace editor_automation
