#include <gtest/gtest.h>

#include <chrono>

#include "core/world/prototype_level.hpp"
#include "editor/editor_audio_audition.hpp"
#include "editor/editor_overlay.hpp"
#include "editor/editor_picking.hpp"

namespace {
class EditorAudio : public testing::Test {
 protected:
  EditorDocument editor;
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("editor-audio-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  void SetUp() override {
    std::filesystem::create_directory(root);
    ASSERT_TRUE(editor.open("resources/levels/audio-captions.level.json"));
  }
  void TearDown() override { std::filesystem::remove_all(root); }
};
}  // namespace

TEST_F(EditorAudio,
       RenameReferencesUndoAtomicallyAndPreservePreexistingBrokenReferences) {
  const auto source_id = editor.audioIds(EditorAudioKind::Source)[1];
  auto source = std::get<AudioSourceDefinition>(*editor.object(source_id));
  source.cue = "renamed";
  ASSERT_TRUE(editor.replaceObject(source_id, source));
  const auto before = *editor.document();
  const auto cue_id = editor.audioIds(EditorAudioKind::Cue)[0];
  auto cue = std::get<AudioCueDefinition>(*editor.object(cue_id));
  cue.id = "renamed";
  ASSERT_TRUE(editor.replaceObject(cue_id, cue));
  EXPECT_EQ(editor.document()->audio.sources[0].cue, "renamed");
  EXPECT_EQ(editor.document()->audio.sources[1].cue, "renamed");
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  ASSERT_TRUE(editor.redo());
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.undo());
  const auto original = *editor.document();
  const auto room_id = editor.audioIds(EditorAudioKind::Room)[0];
  auto room = std::get<AudioRoomDefinition>(*editor.object(room_id));
  room.id = "renamed-room";
  ASSERT_TRUE(editor.replaceObject(room_id, room));
  EXPECT_EQ(editor.document()->audio.connections[0].room_a, "renamed-room");
  const auto door_id = editor.doorIds()[0];
  auto door = std::get<DoorDefinition>(*editor.object(door_id));
  door.id = "renamed-door";
  ASSERT_TRUE(editor.replaceObject(door_id, door));
  EXPECT_EQ(editor.document()->audio.connections[0].door, "renamed-door");
  EXPECT_TRUE(editor.valid());
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorAudio,
       EveryAudioKindKeepsAllocatedIdentityThroughDuplicateDeleteAndHistory) {
  for (const auto kind : {EditorAudioKind::Cue, EditorAudioKind::Source,
                          EditorAudioKind::Room, EditorAudioKind::Connection}) {
    const auto before = *editor.document();
    editor.select(editor.audioIds(kind)[0]);
    ASSERT_TRUE(editor.duplicateSelected());
    const auto handle = editor.selection();
    const auto value = *editor.object(handle);
    const auto count = editor.audioIds(kind).size();
    ASSERT_TRUE(editor.removeSelected());
    EXPECT_FALSE(editor.object(handle));
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(editor.object(handle), value);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_FALSE(editor.dirty());
    ASSERT_TRUE(editor.redo());
    EXPECT_EQ(editor.selection(), handle);
    EXPECT_EQ(editor.object(handle), value);
    EXPECT_EQ(editor.audioIds(kind).size(), count);
    ASSERT_TRUE(editor.undo());
  }
  for (const auto kind : {EditorAudioKind::Cue, EditorAudioKind::Room}) {
    editor.select(editor.audioIds(kind)[0]);
    const auto before = *editor.document();
    ASSERT_TRUE(editor.removeSelected());
    EXPECT_FALSE(editor.valid());
    EXPECT_FALSE(editor.saveAs(root / "invalid.json"));
    ASSERT_TRUE(editor.undo());
    EXPECT_TRUE(editor.valid());
    EXPECT_EQ(*editor.document(), before);
  }
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Source));
  ASSERT_TRUE(editor.saveAs(root / std::filesystem::path(u8"Звуки.json")));
  const auto saved = *editor.document();
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(*editor.document(), saved);
}

TEST_F(EditorAudio,
       SourcesUseNearestSurfaceOffsetsAndRoomsPickOnlyTheirWireEdges) {
  editor.requestNewInterior();
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Cue));
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Source));
  const auto source_id = editor.selection();
  const EditorRay down{{0, 8, 0}, {0, -1, 0}};
  const auto original = *editor.document();
  EXPECT_FALSE(updateEditorPlacementViewport(editor, down, true, false, true,
                                             EditorPlacementMode::SceneSurfaces,
                                             {1.25F, .2F}));
  EXPECT_EQ(*editor.document(), original);
  ASSERT_TRUE(updateEditorPlacementViewport(editor, down, false, false, true,
                                            EditorPlacementMode::SceneSurfaces,
                                            {1.25F, .2F}));
  const auto position = editor.document()->audio.sources[0].position;
  EXPECT_EQ(position, (WorldPosition{0, 1.25F, 0}));
  EXPECT_EQ(pickEditorObject(editor, {{0, 1.25F, 4}, {0, 0, -1}}), source_id);
  const auto wall = editorPlacedObject(
      *editor.object(source_id),
      {{3, 2, 0}, {-1, 0, 0}, 1, 0, EditorSurfaceFace::NegativeX}, {1, .2F});
  ASSERT_TRUE(wall);
  EXPECT_FLOAT_EQ(std::get<AudioSourceDefinition>(*wall).position.x, 2.8F);
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Room));
  const auto room_id = editor.selection();
  auto room = std::get<AudioRoomDefinition>(*editor.object(room_id));
  room.center = {3, 2, 0};
  room.half_extent = {1, 1, 1};
  ASSERT_TRUE(editor.replaceObject(room_id, room));
  EXPECT_EQ(pickEditorObject(editor, {{2, 2, 4}, {0, 0, -1}}), room_id);
  EXPECT_NE(pickEditorObject(editor, {{3, 2, 4}, {0, 0, -1}}), room_id);
  EXPECT_FALSE(editorPlacedObject(
      room, {{0, 0, 0}, {0, 1, 0}, 1, 0, EditorSurfaceFace::Top}, {}));
}

TEST_F(EditorAudio,
       AuditionIsExplicitUsesSnapshotAndStopsOnEveryDocumentRevision) {
  const CaptionFont font("resources");
  EditorAudioAudition audition;
  const auto source = editor.audioIds(EditorAudioKind::Source)[3];
  const auto original = *editor.document();
  const auto revision = editor.revision();
  EXPECT_FALSE(audition.cues());
  ASSERT_TRUE(audition.start(editor, source, "resources", font, 0,
                             AudioOutput::Offline))
      << audition.error();
  audition.update(editor, {2.2F, 3.9F, -4}, {0, 0, -1}, 1);
  ASSERT_TRUE(audition.cues());
  EXPECT_FALSE(audition.cues()->captions().foreground.text.empty());
  const auto gain = audition.cues()->effectiveGain(audition.source());
  audition.update(editor, {100, 100, 100}, {1, 0, 0}, 1);
  EXPECT_LT(audition.cues()->effectiveGain(audition.source()), gain);
  audition.mute();
  audition.pause(1);
  audition.update(editor, {}, {0, 0, -1}, 1001);
  EXPECT_EQ(audition.cues()->offset(audition.source()), 1);
  audition.pause(2001);
  audition.update(editor, {}, {0, 0, -1}, 2002);
  EXPECT_EQ(audition.cues()->offset(audition.source()), 2);
  EXPECT_EQ(editor.revision(), revision);
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
  auto v = std::get<AudioSourceDefinition>(*editor.object(source));
  v.gain = .7F;
  ASSERT_TRUE(editor.replaceObject(source, v));
  audition.update(editor, {}, {0, 0, -1}, 2002);
  EXPECT_FALSE(audition.cues());
  ASSERT_TRUE(audition.start(editor, source, "resources", font, 0,
                             AudioOutput::Silent));
  ASSERT_TRUE(editor.undo());
  audition.update(editor, {}, {0, 0, -1}, 1);
  EXPECT_FALSE(audition.cues());
  ASSERT_TRUE(audition.start(editor, source, "resources", font, 0,
                             AudioOutput::Silent));
  ASSERT_TRUE(editor.redo());
  audition.update(editor, {}, {0, 0, -1}, 1);
  EXPECT_FALSE(audition.cues());
  ASSERT_TRUE(audition.start(editor, source, "resources", font, 0,
                             AudioOutput::Silent));
  ASSERT_TRUE(editor.open("resources/levels/audio-captions.level.json"));
  audition.update(editor, {}, {0, 0, -1}, 1);
  EXPECT_FALSE(audition.cues());
  EXPECT_FALSE(
      audition.start(editor, source, root, font, 0, AudioOutput::Silent));
  EXPECT_FALSE(audition.error().empty());
  EXPECT_TRUE(editor.valid());
  EXPECT_FALSE(editor.dirty());
}
