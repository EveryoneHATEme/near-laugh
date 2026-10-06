#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <limits>
#include <set>

#include "editor/editor_document.hpp"
#include "editor/editor_picking.hpp"
#include "editor/editor_playtest.hpp"
#include "editor/editor_property_edit.hpp"

namespace {
class EditorNarrative : public testing::Test {
 protected:
  void SetUp() override {
    editor.requestNewInterior();
    ASSERT_TRUE(editor.saveAs(path))
        << formatLevelDiagnostics(editor.diagnostics());
  }
  void TearDown() override { std::filesystem::remove(path); }
  EditorObjectId add(EditorNarrativeKind kind) {
    EXPECT_TRUE(editor.addNarrative(kind));
    return editor.selection();
  }
  template <class T>
  T value(EditorObjectId id) {
    return std::get<T>(*editor.object(id));
  }
  EditorDocument editor;
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("editor-narrative-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".json");
};

TEST_F(EditorNarrative, CollectionsSelectionDuplicateCapacityAndRedoIdentity) {
  const std::array limits{level_maximum_narrative_fact_count,
                          level_maximum_narrative_region_count,
                          level_maximum_narrative_event_count};
  for (std::size_t k = 0; k < limits.size(); ++k) {
    const auto kind = static_cast<EditorNarrativeKind>(k);
    const auto original = add(kind);
    const auto original_value = *editor.object(original);
    ASSERT_TRUE(editor.duplicateSelected());
    const auto duplicate = editor.selection();
    EXPECT_NE(duplicate, original);
    auto duplicate_value = *editor.object(duplicate);
    std::visit(
        [&](auto& v) {
          if constexpr (requires { v.id; })
            v.id = std::visit(
                [](const auto& old) -> std::string {
                  if constexpr (requires { old.id; }) return old.id;
                  return {};
                },
                original_value);
        },
        duplicate_value);
    EXPECT_EQ(duplicate_value, original_value);
    EXPECT_TRUE(editor.undo());
    EXPECT_FALSE(editor.object(duplicate));
    EXPECT_TRUE(editor.redo());
    EXPECT_EQ(editor.selection(), duplicate);
    for (std::size_t n = 2; n < limits[k]; ++n)
      ASSERT_TRUE(editor.addNarrative(kind));
    const auto before = *editor.document();
    const auto revision = editor.revision();
    const auto selected = editor.selection();
    EXPECT_FALSE(editor.addNarrative(kind));
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(editor.selection(), selected);
    EXPECT_FALSE(editor.duplicateSelected());
    EXPECT_EQ(*editor.document(), before);
  }
}

TEST_F(EditorNarrative, TypedRenameIsAtomicAndDeletionKeepsRepairableLinks) {
  const auto fact = add(EditorNarrativeKind::Fact);
  const auto region = add(EditorNarrativeKind::Region);
  const auto event = add(EditorNarrativeKind::Event);
  auto f = value<NarrativeFactDefinition>(fact);
  f.id = "same";
  ASSERT_TRUE(editor.replaceObject(fact, f));
  auto r = value<NarrativeRegionDefinition>(region);
  r.id = "same";
  ASSERT_TRUE(editor.replaceObject(region, r));
  auto e = value<NarrativeEventDefinition>(event);
  e.trigger = NarrativeRegionEntryTrigger{"same"};
  e.guards = {NarrativeFactPredicate{"same", true},
              NarrativeRegionPredicate{"same", true}};
  e.cancel =
      std::vector<NarrativePredicate>{NarrativeFactPredicate{"same", false}};
  e.steps = {NarrativeSetFactStep{"same", true},
             NarrativeWaitUntilStep{{NarrativeFactPredicate{"same", true}}}};
  ASSERT_TRUE(editor.replaceObject(event, e));
  ASSERT_TRUE(editor.save());
  const auto baseline = *editor.document();
  f.id = "renamed";
  ASSERT_TRUE(editor.replaceObject(fact, f));
  const auto renamed = value<NarrativeEventDefinition>(event);
  EXPECT_EQ(std::get<NarrativeRegionEntryTrigger>(renamed.trigger).region,
            "same");
  EXPECT_EQ(std::get<NarrativeFactPredicate>(renamed.guards[0]).fact,
            "renamed");
  EXPECT_EQ(std::get<NarrativeRegionPredicate>(renamed.guards[1]).region,
            "same");
  EXPECT_EQ(std::get<NarrativeFactPredicate>(renamed.cancel->front()).fact,
            "renamed");
  EXPECT_EQ(std::get<NarrativeSetFactStep>(renamed.steps[0]).fact, "renamed");
  EXPECT_EQ(
      std::get<NarrativeFactPredicate>(
          std::get<NarrativeWaitUntilStep>(renamed.steps[1]).predicates[0])
          .fact,
      "renamed");
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), baseline);
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  EXPECT_TRUE(editor.dirty());
  editor.select(fact);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(value<NarrativeEventDefinition>(event), renamed);
  EXPECT_FALSE(editor.valid());
  EXPECT_FALSE(editor.save());
  EXPECT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), baseline);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorNarrative, ExistingTypedSourcesRewriteEveryIncomingNarrativeLink) {
  const auto event = add(EditorNarrativeKind::Event);
  ASSERT_TRUE(editor.addPointLight());
  const auto light = editor.selection();
  ASSERT_TRUE(editor.addLightSwitch());
  const auto sw = editor.selection();
  ASSERT_TRUE(editor.addDoor());
  const auto door = editor.selection();
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Source));
  const auto source = editor.selection();
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  const auto actor = editor.selection();
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Route));
  const auto route = editor.selection();
  ASSERT_TRUE(editor.addHousehold(EditorHouseholdKind::Box));
  const auto box = editor.selection();
  ASSERT_TRUE(editor.addHousehold(EditorHouseholdKind::Document));
  const auto document = editor.selection();
  ASSERT_TRUE(editor.addHousehold(EditorHouseholdKind::Radio));
  const auto radio = editor.selection();
  const auto name = [&](EditorObjectId id) {
    return std::visit(
        [](const auto& v) -> std::string {
          if constexpr (requires { v.id; }) return v.id;
          return {};
        },
        *editor.object(id));
  };
  auto e = value<NarrativeEventDefinition>(event);
  e.trigger =
      NarrativeInteractionTrigger{NarrativeInteractionTarget::Switch, name(sw),
                                  NarrativeInteractionAction::SwitchActivate};
  e.guards = {NarrativeLightPredicate{name(light), true},
              NarrativeDoorEndpointPredicate{name(door), true},
              NarrativeDoorLockedPredicate{name(door), false},
              NarrativeBoxPredicate{name(box), true},
              NarrativeDocumentPredicate{name(document), true},
              NarrativeActorPredicate{name(actor), NarrativeActorState::Idle},
              NarrativeRadioPredicate{name(radio), true}};
  e.steps = {NarrativeSetLightStep{name(light), true},
             NarrativeSetDoorOpenStep{name(door), true},
             NarrativeSetDoorLockedStep{name(door), false},
             NarrativeSetRadioStep{name(radio), true},
             NarrativePlayCueStep{name(source)},
             NarrativeRunRouteStep{name(actor), name(route)}};
  ASSERT_TRUE(editor.replaceObject(event, e));
  for (const auto id :
       {light, sw, door, source, actor, route, box, document, radio}) {
    const auto before = *editor.document();
    auto v = *editor.object(id);
    std::visit(
        [](auto& item) {
          if constexpr (requires { item.id; }) item.id += "-new";
        },
        v);
    ASSERT_TRUE(editor.replaceObject(id, v)) << editor.editError();
    EXPECT_NE(value<NarrativeEventDefinition>(event), e);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);

    const auto old_name = name(id);
    editor.select(id);
    ASSERT_TRUE(editor.removeSelected());
    const bool added = std::visit(
        [&](const auto& item) {
          using T = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<T, PrototypePointLight>)
            return editor.addPointLight();
          else if constexpr (std::is_same_v<T, PrototypeLightSwitch>)
            return editor.addLightSwitch();
          else if constexpr (std::is_same_v<T, DoorDefinition>)
            return editor.addDoor();
          else if constexpr (std::is_same_v<T, AudioSourceDefinition>)
            return editor.addAudio(EditorAudioKind::Source);
          else if constexpr (std::is_same_v<T, CharacterActorDefinition>)
            return editor.addCharacter(EditorCharacterKind::Actor);
          else if constexpr (std::is_same_v<T, CharacterRouteDefinition>)
            return editor.addCharacter(EditorCharacterKind::Route);
          else if constexpr (std::is_same_v<T, HouseholdBoxDefinition>)
            return editor.addHousehold(EditorHouseholdKind::Box);
          else if constexpr (std::is_same_v<T, HouseholdDocumentDefinition>)
            return editor.addHousehold(EditorHouseholdKind::Document);
          else if constexpr (std::is_same_v<T, HouseholdRadioDefinition>)
            return editor.addHousehold(EditorHouseholdKind::Radio);
          else
            return false;
        },
        v);
    ASSERT_TRUE(added);
    EXPECT_NE(name(editor.selection()), old_name);
    EXPECT_EQ(value<NarrativeEventDefinition>(event), e);
    if (id == door) {
      ASSERT_TRUE(editor.duplicateSelected());
      EXPECT_NE(name(editor.selection()), old_name);
      EXPECT_EQ(value<NarrativeEventDefinition>(event), e);
      ASSERT_TRUE(editor.undo());
    }
    ASSERT_TRUE(editor.undo());
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
  }
}

TEST_F(EditorNarrative, AutomaticIdsNeverRepairDeletedNarrativeLinks) {
  const auto fact = add(EditorNarrativeKind::Fact);
  const auto region = add(EditorNarrativeKind::Region);
  const auto target_event = add(EditorNarrativeKind::Event);
  const auto owner = add(EditorNarrativeKind::Event);
  auto event = value<NarrativeEventDefinition>(owner);
  event.trigger =
      NarrativeRegionEntryTrigger{value<NarrativeRegionDefinition>(region).id};
  event.steps = {
      NarrativeSetFactStep{value<NarrativeFactDefinition>(fact).id, true},
      NarrativeWaitUntilStep{{NarrativeEventPredicate{
          value<NarrativeEventDefinition>(target_event).id,
          NarrativeEventTerminalState::Completed}}}};
  ASSERT_TRUE(editor.replaceObject(owner, event));
  ASSERT_TRUE(editor.save());
  for (const auto id : {fact, region, target_event}) {
    const auto original = *editor.object(id);
    const auto kind = *editorNarrativeKind(original);
    const auto name = [](const EditorObjectValue& record) {
      return std::visit(
          [](const auto& item) -> std::string {
            if constexpr (requires { item.id; }) return item.id;
            return {};
          },
          record);
    };
    editor.select(id);
    ASSERT_TRUE(editor.removeSelected());
    ASSERT_FALSE(editor.valid());
    ASSERT_TRUE(editor.addNarrative(kind));
    EXPECT_NE(name(*editor.object(editor.selection())), name(original));
    EXPECT_EQ(value<NarrativeEventDefinition>(owner), event);
    EXPECT_FALSE(editor.valid());
    EXPECT_FALSE(editor.save());
    ASSERT_TRUE(editor.undo());
    ASSERT_TRUE(editor.undo());
    EXPECT_TRUE(editor.valid());
    EXPECT_FALSE(editor.dirty());
  }
}

TEST_F(EditorNarrative,
       OrderedEditsSaveReopenAndUnknownReferencesRetainExactData) {
  const auto fact = add(EditorNarrativeKind::Fact);
  (void)fact;
  const auto event = add(EditorNarrativeKind::Event);
  auto e = value<NarrativeEventDefinition>(event);
  e.steps = {NarrativeDelayStep{1}, NarrativeSetFactStep{"fact-1", true},
             NarrativeDelayStep{3}};
  e.guards = {NarrativeElapsedPredicate{0},
              NarrativeFactPredicate{"fact-1", false}};
  ASSERT_TRUE(editor.replaceObject(event, e));
  ASSERT_TRUE(editor.save());
  const auto baseline = *editor.document();
  std::swap(e.steps[0], e.steps[2]);
  std::swap(e.guards[0], e.guards[1]);
  ASSERT_TRUE(editor.replaceObject(event, e));
  EXPECT_TRUE(editor.dirty());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), baseline);
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  ASSERT_TRUE(editor.save());
  EditorDocument reopened;
  ASSERT_TRUE(reopened.open(path));
  EXPECT_EQ(reopened.document()->narrative.events.front(), e);
  EXPECT_FALSE(reopened.dirty());
  e.steps.insert(e.steps.begin(), NarrativeSetFactStep{"missing", true});
  ASSERT_TRUE(editor.replaceObject(event, e));
  EXPECT_FALSE(editor.valid());
  EXPECT_EQ(std::get<NarrativeSetFactStep>(
                value<NarrativeEventDefinition>(event).steps[0])
                .fact,
            "missing");
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorNarrative, InvalidDraftAndCanceledPlacementNeverCommit) {
  const auto region = add(EditorNarrativeKind::Region);
  ASSERT_TRUE(editor.save());
  const auto baseline = *editor.document();
  EditorPropertyEdit draft;
  draft.synchronize(editor);
  std::get<NarrativeRegionDefinition>(*draft.value()).half_extent.x = -1;
  EXPECT_FALSE(draft.commit(editor));
  EXPECT_EQ(*editor.document(), baseline);
  EXPECT_FALSE(editor.dirty());
  auto bounds = value<NarrativeRegionDefinition>(region);
  bounds.center.x = std::numeric_limits<float>::max();
  bounds.half_extent.x = bounds.center.x;
  EXPECT_FALSE(editor.replaceObject(region, bounds));
  EXPECT_EQ(*editor.document(), baseline);
  const auto revision = editor.revision();
  const EditorRay ray{{2, 10, 0}, {0, -1, 0}};
  (void)updateEditorPlacementViewport(editor, ray, false, false, false,
                                      EditorPlacementMode::SceneSurfaces, {});
  (void)updateEditorPlacementViewport(editor, ray, true, false, true,
                                      EditorPlacementMode::SceneSurfaces, {});
  EXPECT_EQ(editor.revision(), revision);
  EXPECT_EQ(*editor.document(), baseline);
}

TEST_F(EditorNarrative, RegionUpperFloorPlacementAndNearerWallRejection) {
  const auto region = add(EditorNarrativeKind::Region);
  auto r = value<NarrativeRegionDefinition>(region);
  r.center = {100, 2, 100};
  r.half_extent = {2, 1.5F, 3};
  ASSERT_TRUE(editor.replaceObject(region, r));
  ASSERT_TRUE(editor.addSolid({{100, 4, 100},
                               {8, .25F, 8},
                               {100, 100, 100, 255},
                               PrototypeSolidKind::Floor}));
  editor.select(region);
  const auto before = *editor.document();
  const EditorRay ray{{100, 12, 100}, {0, -1, 0}};
  const auto hit =
      pickEditorSurface(editor, ray, EditorPlacementMode::SceneSurfaces);
  ASSERT_TRUE(hit);
  EXPECT_FLOAT_EQ(hit->position.y, 4.25F);
  ASSERT_TRUE(editor.placeSelected(*hit, {}));
  const auto placed = value<NarrativeRegionDefinition>(region);
  EXPECT_EQ(placed.id, r.id);
  EXPECT_EQ(placed.half_extent, r.half_extent);
  EXPECT_FLOAT_EQ(placed.center.y, 5.75F);
  // Regions pick by their wire edges; the interior stays transparent so
  // content inside a trigger volume remains selectable.
  EXPECT_EQ(pickEditorObject(editor, {{102, 12, 100}, {0, -1, 0}}), region);
  EXPECT_NE(pickEditorObject(editor, {{100, 12, 100}, {0, -1, 0}}), region);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  ASSERT_TRUE(editor.addSolid({{100, 8, 95},
                               {8, 4, .25F},
                               {100, 100, 100, 255},
                               PrototypeSolidKind::Boundary}));
  editor.select(region);
  const auto wall = pickEditorSurface(editor, {{100, 9, 90}, {0, -.3F, 1}},
                                      EditorPlacementMode::SceneSurfaces);
  ASSERT_TRUE(wall);
  EXPECT_NE(wall->face, EditorSurfaceFace::Top);
  const auto revision = editor.revision();
  EXPECT_FALSE(editor.placeSelected(*wall, {}));
  EXPECT_EQ(editor.revision(), revision);
}

TEST_F(EditorNarrative,
       DirtyPlayCancellationPreservesAuthoredStateAndSavedSnapshot) {
  const auto event = add(EditorNarrativeKind::Event);
  ASSERT_TRUE(editor.save());
  EditorPlaytest play;
  ASSERT_TRUE(play.request(editor, false));
  const auto launch = play.consume();
  ASSERT_TRUE(launch);
  const auto saved = loadEditorPlayDocument(editor, *launch);
  auto e = value<NarrativeEventDefinition>(event);
  e.steps = {NarrativeDelayStep{9}};
  ASSERT_TRUE(editor.replaceObject(event, e));
  const auto revision = editor.revision();
  const auto draft = *editor.document();
  ASSERT_TRUE(play.request(editor, false));
  EXPECT_EQ(play.state(), EditorPlayState::ConfirmSave);
  play.cancel();
  EXPECT_FALSE(play.consume());
  EXPECT_EQ(*editor.document(), draft);
  EXPECT_EQ(editor.revision(), revision);
  EXPECT_TRUE(editor.dirty());
  const auto disk = loadLevelDocument(path);
  ASSERT_TRUE(disk);
  EXPECT_EQ(*disk.document, saved);
  ASSERT_TRUE(editor.undo());
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(*editor.document(), saved);
}

TEST_F(EditorNarrative,
       SaveOwnershipValidationAndMissingPlayResourceHaveSeparateBoundaries) {
  ASSERT_TRUE(editor.open("resources/levels/narrative-t4.level.json"));
  ASSERT_TRUE(editor.saveAs(path));
  const auto event = editor.narrativeIds(EditorNarrativeKind::Event).front();
  const auto baseline = *editor.document();
  auto e = value<NarrativeEventDefinition>(event);
  e.steps = {
      NarrativePlayCueStep{editor.document()->household.radios.front().source}};
  ASSERT_TRUE(editor.replaceObject(event, e));
  EXPECT_FALSE(editor.valid());
  EXPECT_FALSE(editor.save());
  EditorPlaytest play;
  EXPECT_FALSE(play.request(editor, false));
  EXPECT_FALSE(play.consume());
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.valid());
  ASSERT_TRUE(editor.save());  // Saving does not require a newly invented
                               // resource policy.
  ASSERT_TRUE(play.request(editor, false));
  const auto request = play.consume();
  ASSERT_TRUE(request);
  EditorGameProcess process(false);
  EXPECT_THROW(
      (void)launchEditorPlay(editor, *request, path / "missing-resources",
                             path / "missing-executable", process),
      std::exception);
  EXPECT_FALSE(process.active());
  EXPECT_EQ(*editor.document(), baseline);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorNarrative,
       EventRenameUpdatesOtherTerminalReferencesAndUndoRestoresThem) {
  const auto first = add(EditorNarrativeKind::Event);
  const auto second = add(EditorNarrativeKind::Event);
  auto event = value<NarrativeEventDefinition>(second);
  event.guards = {NarrativeEventPredicate{
      "event-1", NarrativeEventTerminalState::Completed}};
  ASSERT_TRUE(editor.replaceObject(second, event));
  const auto baseline = *editor.document();
  auto renamed = value<NarrativeEventDefinition>(first);
  renamed.id = "earlier-event";
  ASSERT_TRUE(editor.replaceObject(first, renamed));
  EXPECT_EQ(std::get<NarrativeEventPredicate>(
                value<NarrativeEventDefinition>(second).guards[0])
                .event,
            "earlier-event");
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), baseline);
  ASSERT_TRUE(editor.redo());
  editor.select(first);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(std::get<NarrativeEventPredicate>(
                value<NarrativeEventDefinition>(second).guards[0])
                .event,
            "earlier-event");
  EXPECT_FALSE(editor.valid());
}

TEST_F(EditorNarrative, DurationAndListBoundsRejectWithoutPartialEdit) {
  const auto event = add(EditorNarrativeKind::Event);
  const auto original = value<NarrativeEventDefinition>(event);
  for (const auto seconds :
       {-1.F, 3601.F, std::numeric_limits<float>::infinity()}) {
    auto e = original;
    e.steps = {NarrativeDelayStep{seconds}};
    EXPECT_FALSE(editor.replaceObject(event, e));
    EXPECT_EQ(value<NarrativeEventDefinition>(event), original);
  }
  auto e = original;
  e.steps.resize(level_maximum_narrative_step_count + 1, NarrativeDelayStep{});
  EXPECT_FALSE(editor.replaceObject(event, e));
  EXPECT_EQ(value<NarrativeEventDefinition>(event), original);
  e = original;
  e.guards.resize(level_maximum_narrative_predicate_count + 1,
                  NarrativeElapsedPredicate{});
  EXPECT_FALSE(editor.replaceObject(event, e));
  EXPECT_EQ(value<NarrativeEventDefinition>(event), original);
}
}  // namespace
