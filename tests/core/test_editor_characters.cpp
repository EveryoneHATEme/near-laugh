#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <set>

#include "core/world/characters.hpp"
#include "editor/editor_playtest.hpp"
#include "editor/editor_property_edit.hpp"

namespace {
class EditorCharacters : public testing::Test {
 protected:
  EditorDocument editor;
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("editor-characters-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".json");
  void SetUp() override {
    ASSERT_TRUE(editor.open("resources/levels/scripted-characters.level.json"));
  }
  void TearDown() override { std::filesystem::remove(path); }
  EditorObjectId first(EditorCharacterKind kind) {
    return editor.characterIds(kind).front();
  }
};
}  // namespace

TEST_F(EditorCharacters, EveryRecordHasOneStableHandleAndSelectionDoesNotEdit) {
  const auto original = *editor.document();
  const auto revision = editor.revision();
  std::set<EditorObjectId> seen;
  const auto& c = original.characters;
  const std::array sizes{c.actors.size(), c.marks.size(), c.routes.size()};
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    const auto kind = static_cast<EditorCharacterKind>(i);
    ASSERT_EQ(editor.characterIds(kind).size(), sizes[i]);
    for (const auto id : editor.characterIds(kind)) {
      EXPECT_TRUE(seen.insert(id).second);
      ASSERT_TRUE(editor.object(id));
      EXPECT_EQ(editorCharacterKind(*editor.object(id)), kind);
      editor.select(id);
      EXPECT_EQ(editor.selection(), id);
    }
  }
  for (const auto& ids : {editor.solidIds(), editor.entryIds(),
                          editor.lightIds(), editor.doorIds()})
    for (const auto id : ids) EXPECT_FALSE(seen.contains(id));
  EXPECT_EQ(*editor.document(), original);
  EXPECT_EQ(editor.revision(), revision);
  EXPECT_FALSE(editor.dirty());
  EXPECT_FALSE(editor.canUndo());
  ASSERT_TRUE(editor.open("resources/levels/prototype.level.json"));
  for (int i = 0; i < 3; ++i)
    EXPECT_TRUE(
        editor.characterIds(static_cast<EditorCharacterKind>(i)).empty());
  EXPECT_EQ(editor.selection(), editor_no_object);
}

TEST_F(EditorCharacters,
       SafeInvalidDraftsRemainRepairableAndNonfiniteCommitsAreRejected) {
  const auto original = *editor.document();
  const auto actor_id = first(EditorCharacterKind::Actor);
  editor.select(actor_id);
  EditorPropertyEdit draft;
  draft.synchronize(editor);
  auto& actor = std::get<CharacterActorDefinition>(*draft.value());
  actor.model = "unknown-model";
  actor.initial_mark = "unknown-mark";
  actor.initial_route = "unknown-route";
  actor.footstep_source = "unknown-footsteps";
  actor.interaction_source = "unknown-interaction";
  actor.speed = -1;
  EXPECT_EQ(*editor.document(), original);
  ASSERT_TRUE(draft.commit(editor));
  EXPECT_FALSE(editor.valid());
  EXPECT_TRUE(editor.dirty());
  EXPECT_EQ(
      std::get<CharacterActorDefinition>(*editor.object(actor_id)).initial_mark,
      "unknown-mark");
  EXPECT_FALSE(editor.saveAs(path));
  EditorPlaytest play;
  EXPECT_FALSE(play.request(editor, false));
  EXPECT_FALSE(play.consume());
  const auto invalid = *editor.document();
  const auto revision = editor.revision();
  auto unsafe = std::get<CharacterActorDefinition>(*editor.object(actor_id));
  unsafe.speed = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(editor.replaceObject(actor_id, unsafe));
  EXPECT_FALSE(editor.editError().empty());
  EXPECT_EQ(*editor.document(), invalid);
  EXPECT_EQ(editor.revision(), revision);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
  const auto mark_id = first(EditorCharacterKind::Mark);
  auto mark = std::get<CharacterMarkDefinition>(*editor.object(mark_id));
  mark.feet_position.y += 100;
  ASSERT_TRUE(editor.replaceObject(mark_id, mark));
  EXPECT_FALSE(editor.valid());
  const auto finite_invalid = *editor.document();
  mark.yaw_degrees = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(editor.replaceObject(mark_id, mark));
  EXPECT_EQ(*editor.document(), finite_invalid);
  ASSERT_TRUE(editor.undo());
  auto route = std::get<CharacterRouteDefinition>(
      *editor.object(first(EditorCharacterKind::Route)));
  route.actor = "unknown-owner";
  route.marks = {"unknown-endpoint"};
  route.final_clip = "unknown-clip";
  ASSERT_TRUE(editor.replaceObject(first(EditorCharacterKind::Route), route));
  EXPECT_EQ(std::get<CharacterRouteDefinition>(
                *editor.object(first(EditorCharacterKind::Route))),
            route);
  EXPECT_FALSE(editor.valid());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
}

TEST_F(EditorCharacters,
       ActorCreationAndIndependentDuplicationUseOneHistoryEntry) {
  const auto original = *editor.document();
  editor.select(first(EditorCharacterKind::Actor));
  const auto selected = editor.selection();
  const auto actor =
      std::get<CharacterActorDefinition>(*editor.object(selected));
  ASSERT_TRUE(actor.initial_route);
  ASSERT_TRUE(actor.footstep_source);
  ASSERT_TRUE(actor.interaction_source);
  ASSERT_TRUE(editor.duplicateSelected());
  const auto handle = editor.selection();
  const auto copy = std::get<CharacterActorDefinition>(*editor.object(handle));
  EXPECT_NE(copy.id, actor.id);
  EXPECT_NE(copy.initial_mark, actor.initial_mark);
  EXPECT_EQ(copy.model, actor.model);
  EXPECT_EQ(copy.speed, actor.speed);
  EXPECT_FALSE(copy.initial_route);
  EXPECT_FALSE(copy.footstep_source);
  EXPECT_FALSE(copy.interaction_source);
  const auto* mark =
      findCharacterMark(editor.document()->characters, copy.initial_mark);
  ASSERT_NE(mark, nullptr);
  const auto* old_mark =
      findCharacterMark(original.characters, actor.initial_mark);
  ASSERT_NE(old_mark, nullptr);
  EXPECT_EQ(mark->yaw_degrees, old_mark->yaw_degrees);
  EXPECT_NE(mark->feet_position, old_mark->feet_position);
  const auto added = *editor.document();
  const auto mark_handle =
      editor.characterIds(EditorCharacterKind::Mark).back();
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_EQ(editor.selection(), selected);
  EXPECT_FALSE(editor.dirty());
  EXPECT_FALSE(editor.canUndo());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(*editor.document(), added);
  EXPECT_EQ(editor.selection(), handle);
  EXPECT_EQ(editor.characterIds(EditorCharacterKind::Mark).back(), mark_handle);
  auto changed_mark =
      std::get<CharacterMarkDefinition>(*editor.object(mark_handle));
  changed_mark.feet_position.z += 1;
  ASSERT_TRUE(editor.replaceObject(mark_handle, changed_mark));
  EXPECT_EQ(
      *findCharacterMark(editor.document()->characters, actor.initial_mark),
      *old_mark);
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.undo());
  editor.select(editor_no_object);
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  EXPECT_EQ(editor.document()->characters.marks.size(),
            original.characters.marks.size() + 1);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  editor.select(first(EditorCharacterKind::Mark));
  const auto existing_mark =
      std::get<CharacterMarkDefinition>(*editor.object(editor.selection())).id;
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  EXPECT_EQ(editor.document()->characters.marks.size(),
            original.characters.marks.size());
  EXPECT_EQ(
      std::get<CharacterActorDefinition>(*editor.object(editor.selection()))
          .initial_mark,
      existing_mark);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
}

TEST_F(
    EditorCharacters,
    CollectionAndCompoundCapacityFailuresLeaveHistorySelectionAndDataUntouched) {
  const auto unchangedFailure = [&](auto action) {
    const auto before = *editor.document();
    const auto selection = editor.selection();
    const auto revision = editor.revision();
    const bool undo = editor.canUndo(), redo = editor.canRedo(),
               dirty = editor.dirty();
    EXPECT_FALSE(action());
    EXPECT_FALSE(editor.editError().empty());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), selection);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(editor.canUndo(), undo);
    EXPECT_EQ(editor.canRedo(), redo);
    EXPECT_EQ(editor.dirty(), dirty);
  };
  while (editor.characterIds(EditorCharacterKind::Mark).size() <
         level_maximum_character_mark_count)
    ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Mark));
  unchangedFailure(
      [&] { return editor.addCharacter(EditorCharacterKind::Mark); });
  editor.select(first(EditorCharacterKind::Actor));
  unchangedFailure(
      [&] { return editor.addCharacter(EditorCharacterKind::Actor); });
  unchangedFailure([&] { return editor.duplicateSelected(); });
  // Full marks permit explicitly reusing a selected mark until actors are full.
  while (editor.characterIds(EditorCharacterKind::Actor).size() <
         level_maximum_actor_count) {
    editor.select(first(EditorCharacterKind::Mark));
    ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  }
  ASSERT_TRUE(editor.undo());
  editor.select(first(EditorCharacterKind::Actor));
  unchangedFailure([&] { return editor.duplicateSelected(); });
  EXPECT_TRUE(editor.canRedo());
  ASSERT_TRUE(editor.redo());
  unchangedFailure(
      [&] { return editor.addCharacter(EditorCharacterKind::Actor); });
  while (editor.characterIds(EditorCharacterKind::Route).size() <
         level_maximum_character_route_count)
    ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Route));
  unchangedFailure(
      [&] { return editor.addCharacter(EditorCharacterKind::Route); });
  unchangedFailure([&] { return editor.duplicateSelected(); });
}

TEST_F(EditorCharacters,
       EveryKindDuplicatesAndDeletesWithoutCascadesAndRestoresSelection) {
  for (const auto kind : {EditorCharacterKind::Actor, EditorCharacterKind::Mark,
                          EditorCharacterKind::Route}) {
    const auto before = *editor.document();
    const auto id = first(kind);
    editor.select(id);
    ASSERT_TRUE(editor.duplicateSelected());
    const auto duplicate_id = editor.selection();
    const auto duplicate = *editor.object(duplicate_id);
    if (kind == EditorCharacterKind::Route) {
      const auto a = std::get<CharacterRouteDefinition>(*editor.object(id));
      const auto& b = std::get<CharacterRouteDefinition>(duplicate);
      EXPECT_EQ(a.actor, b.actor);
      EXPECT_EQ(a.marks, b.marks);
      EXPECT_EQ(a.final_clip, b.final_clip);
    }
    ASSERT_TRUE(editor.removeSelected());
    EXPECT_FALSE(editor.object(duplicate_id));
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(editor.selection(), duplicate_id);
    EXPECT_EQ(editor.object(duplicate_id), duplicate);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(editor.selection(), id);
    EXPECT_EQ(*editor.document(), before);
    EXPECT_FALSE(editor.dirty());
    ASSERT_TRUE(editor.removeSelected());
    EXPECT_FALSE(editor.valid());
    EXPECT_FALSE(editor.saveAs(path));
    if (kind == EditorCharacterKind::Actor) {
      EXPECT_EQ(editor.document()->characters.routes, before.characters.routes);
      EXPECT_EQ(editor.document()->characters.marks, before.characters.marks);
    } else if (kind == EditorCharacterKind::Mark) {
      EXPECT_EQ(editor.document()->characters.routes, before.characters.routes);
      EXPECT_EQ(editor.document()->characters.actors, before.characters.actors);
    } else
      EXPECT_EQ(editor.document()->characters.actors, before.characters.actors);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), id);
    EXPECT_FALSE(editor.dirty());
  }
}

TEST_F(EditorCharacters,
       RenamesRewriteEveryConsumerAndUndoPreservesPreexistingBrokenLinks) {
  const auto original = *editor.document();
  const auto actor_id = first(EditorCharacterKind::Actor);
  const auto mark_id = first(EditorCharacterKind::Mark);
  const auto route_id = first(EditorCharacterKind::Route);
  auto route = std::get<CharacterRouteDefinition>(*editor.object(route_id));
  const auto old_mark =
      std::get<CharacterMarkDefinition>(*editor.object(mark_id)).id;
  route.marks = {old_mark, "renamed-mark", old_mark};
  ASSERT_TRUE(editor.replaceObject(route_id, route));
  editor.select(route_id);
  ASSERT_TRUE(editor.duplicateSelected());
  const auto before_mark = *editor.document();
  auto mark = std::get<CharacterMarkDefinition>(*editor.object(mark_id));
  mark.id = "renamed-mark";
  ASSERT_TRUE(editor.replaceObject(mark_id, mark));
  const auto& renamed_routes = editor.document()->characters.routes;
  EXPECT_EQ(renamed_routes.front().marks,
            (std::vector<std::string>{mark.id, mark.id, mark.id}));
  EXPECT_EQ(renamed_routes.back().marks,
            (std::vector<std::string>{mark.id, mark.id, mark.id}));
  for (std::size_t i = 0; i < renamed_routes.size(); ++i) {
    EXPECT_EQ(renamed_routes[i].marks.size(),
              before_mark.characters.routes[i].marks.size());
    EXPECT_EQ(std::count(renamed_routes[i].marks.begin(),
                         renamed_routes[i].marks.end(), old_mark),
              0);
  }
  EXPECT_EQ(editor.document()->characters.actors.front().initial_mark, mark.id);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before_mark);
  ASSERT_TRUE(editor.redo());
  auto actor = std::get<CharacterActorDefinition>(*editor.object(actor_id));
  actor.id = "renamed-actor";
  const auto before_actor = *editor.document();
  ASSERT_TRUE(editor.replaceObject(actor_id, actor));
  for (const auto& r : editor.document()->characters.routes)
    EXPECT_EQ(r.actor, actor.id);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before_actor);
  route = std::get<CharacterRouteDefinition>(*editor.object(route_id));
  route.id = "renamed-route";
  ASSERT_TRUE(editor.replaceObject(route_id, route));
  EXPECT_EQ(editor.document()->characters.actors.front().initial_route,
            route.id);
  ASSERT_TRUE(editor.undo());
  while (editor.canUndo()) ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorCharacters,
       SourceRenameRewritesBothRolesEvenInRepairableInvalidData) {
  const auto actor_id = first(EditorCharacterKind::Actor);
  auto actor = std::get<CharacterActorDefinition>(*editor.object(actor_id));
  const auto source_name = *actor.footstep_source;
  actor.interaction_source = source_name;
  ASSERT_TRUE(editor.replaceObject(actor_id, actor));
  const auto before = *editor.document();
  const auto& sources = editor.document()->audio.sources;
  const auto it =
      std::find_if(sources.begin(), sources.end(),
                   [&](const auto& s) { return s.id == source_name; });
  ASSERT_NE(it, sources.end());
  const auto source_id =
      editor.audioIds(EditorAudioKind::Source)[it - sources.begin()];
  auto source = *it;
  source.id = "renamed-source";
  editor.select(source_id);
  ASSERT_TRUE(editor.replaceObject(source_id, source));
  EXPECT_EQ(editor.document()->characters.actors.front().footstep_source,
            source.id);
  EXPECT_EQ(editor.document()->characters.actors.front().interaction_source,
            source.id);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  EXPECT_EQ(editor.selection(), source_id);
  ASSERT_TRUE(editor.redo());
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(editor.document()->characters.actors.front().footstep_source,
            source.id);
  EXPECT_FALSE(editor.valid());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.selection(), source_id);
}

TEST_F(EditorCharacters,
       LinkOrderAndRenameEditsRestoreSavedRevisionAndRejectDuplicateIds) {
  const auto route_id = first(EditorCharacterKind::Route);
  editor.select(route_id);
  auto route = std::get<CharacterRouteDefinition>(*editor.object(route_id));
  ASSERT_GT(route.marks.size(), 1U);
  std::swap(route.marks[0], route.marks[1]);
  ASSERT_TRUE(editor.replaceObject(route_id, route));
  ASSERT_TRUE(editor.saveAs(path));
  const auto saved = *editor.document();
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(*editor.document(), saved);
  EXPECT_EQ(editor.selection(), route_id);
  EXPECT_FALSE(editor.dirty());
  EditorDocument reopened;
  ASSERT_TRUE(reopened.open(path));
  EXPECT_EQ(*reopened.document(), saved);
  EXPECT_EQ(reopened.sourceVersion(), 9U);
  route.marks.clear();
  ASSERT_TRUE(editor.replaceObject(route_id, route));
  EXPECT_FALSE(editor.valid());
  ASSERT_TRUE(editor.undo());
  EXPECT_FALSE(editor.dirty());
  route = std::get<CharacterRouteDefinition>(*editor.object(route_id));
  route.marks.resize(level_maximum_route_mark_count + 1, route.marks.front());
  EXPECT_FALSE(editor.replaceObject(route_id, route));
  EXPECT_EQ(*editor.document(), saved);
  for (const auto kind : {EditorCharacterKind::Actor, EditorCharacterKind::Mark,
                          EditorCharacterKind::Route}) {
    editor.select(first(kind));
    ASSERT_TRUE(editor.duplicateSelected());
    const auto duplicate_id = editor.selection();
    auto value = *editor.object(duplicate_id);
    const auto old = *editor.object(first(kind));
    std::visit(
        [&](auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (requires { v.id; }) v.id = std::get<T>(old).id;
        },
        value);
    const auto before = *editor.document();
    EXPECT_FALSE(editor.replaceObject(duplicate_id, value));
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), duplicate_id);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), saved);
    EXPECT_FALSE(editor.dirty());
  }
}

TEST_F(EditorCharacters,
       NewIdentitiesNeverSilentlyReconnectDeletedCharacterOrSourceLinks) {
  for (const auto kind : {EditorCharacterKind::Actor, EditorCharacterKind::Mark,
                          EditorCharacterKind::Route}) {
    const auto id = first(kind);
    auto value = *editor.object(id);
    const std::array names{"actor-1", "mark-1", "route-1"};
    const std::string name = names[static_cast<std::size_t>(kind)];
    std::visit(
        [&](auto& v) {
          if constexpr (requires { v.id; }) v.id = name;
        },
        value);
    ASSERT_TRUE(editor.replaceObject(id, value));
    editor.select(id);
    ASSERT_TRUE(editor.removeSelected());
    const auto broken = editor.document()->characters;
    ASSERT_TRUE(editor.addCharacter(kind));
    const auto added = *editor.object(editor.selection());
    std::visit(
        [&](const auto& v) {
          if constexpr (requires { v.id; }) EXPECT_NE(v.id, name);
        },
        added);
    const auto& c = editor.document()->characters;
    if (kind == EditorCharacterKind::Actor)
      EXPECT_EQ(c.routes, broken.routes);
    else if (kind == EditorCharacterKind::Mark) {
      EXPECT_EQ(c.routes, broken.routes);
      EXPECT_EQ(c.actors, broken.actors);
    } else
      EXPECT_EQ(c.actors, broken.actors);
    EXPECT_FALSE(editor.valid());
    ASSERT_TRUE(editor.undo());
    ASSERT_TRUE(editor.undo());
    ASSERT_TRUE(editor.undo());
    EXPECT_FALSE(editor.dirty());
  }
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Source));
  const auto source_id = editor.selection();
  const auto source =
      std::get<AudioSourceDefinition>(*editor.object(source_id));
  const auto actor_id = first(EditorCharacterKind::Actor);
  auto actor = std::get<CharacterActorDefinition>(*editor.object(actor_id));
  actor.footstep_source = source.id;
  ASSERT_TRUE(editor.replaceObject(actor_id, actor));
  editor.select(source_id);
  ASSERT_TRUE(editor.removeSelected());
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Source));
  EXPECT_NE(
      std::get<AudioSourceDefinition>(*editor.object(editor.selection())).id,
      source.id);
  EXPECT_EQ(editor.document()->characters.actors.front().footstep_source,
            source.id);
}
