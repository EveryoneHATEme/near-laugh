#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <thread>

#include "editor/editor_playtest.hpp"

namespace {
std::string bytes(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
class EditorPlay : public testing::Test {
 protected:
  void SetUp() override {
    root = std::filesystem::temp_directory_path() /
           ("near_laugh_play_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    editor.requestNewInterior();
  }
  void TearDown() override { std::filesystem::remove_all(root); }
  std::filesystem::path root;
  EditorDocument editor;
  EditorPlaytest play;
};

class EditorCharacterPlay : public EditorPlay {
 protected:
  void SetUp() override {
    EditorPlay::SetUp();
    saved_path = root / std::filesystem::path(u8"сцена & (literal); '$ %.json");
    package = root / std::filesystem::path(u8"ресурсы & package");
    executable = root / std::filesystem::path(u8"игра & literal; [] !");
    executable += std::filesystem::path(EDITOR_ARGUMENT_PROBE_PATH).extension();
    std::filesystem::copy_file(EDITOR_ARGUMENT_PROBE_PATH, executable);
    auto entry = editor.document()->entries.front();
    entry.id = "probe-launch";
    ASSERT_TRUE(editor.replaceObject(editor.entryIds().front(), entry));
    ASSERT_TRUE(editor.selectLaunchEntry(entry.id));
    ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
    auto mark = editor.document()->characters.marks.front();
    mark.feet_position = {2, 0, 0};
    ASSERT_TRUE(editor.replaceObject(
        editor.characterIds(EditorCharacterKind::Mark).front(), mark));
    ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Mark));
    auto endpoint = editor.document()->characters.marks.back();
    endpoint.feet_position = {2, 0, 2};
    ASSERT_TRUE(editor.replaceObject(editor.selection(), endpoint));
    ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Route));
    auto route = editor.document()->characters.routes.front();
    route.marks = {mark.id, endpoint.id};
    route.final_clip = "interact";
    ASSERT_TRUE(editor.replaceObject(editor.selection(), route));
    const auto source =
        loadLevelDocument("resources/levels/scripted-characters.level.json");
    ASSERT_TRUE(source);
    for (const auto& cue : source.document->audio.cues) {
      ASSERT_TRUE(editor.addAudio(EditorAudioKind::Cue));
      ASSERT_TRUE(editor.replaceObject(editor.selection(), cue));
    }
    for (const auto& sound : source.document->audio.sources) {
      ASSERT_TRUE(editor.addAudio(EditorAudioKind::Source));
      ASSERT_TRUE(editor.replaceObject(editor.selection(), sound));
    }
    auto actor = editor.document()->characters.actors.front();
    actor.initial_route = route.id;
    actor.footstep_source = "walker-step";
    actor.interaction_source = "walker-touch";
    ASSERT_TRUE(editor.replaceObject(
        editor.characterIds(EditorCharacterKind::Actor).front(), actor));
    ASSERT_TRUE(editor.valid()) << formatLevelDiagnostics(editor.diagnostics());
  }

  void copySelectedResources() {
    for (const auto* relative :
         {"characters/test-mannequin.glb", "audio/character-footstep.wav",
          "audio/character-interaction.wav",
          "captions/character-interaction.captions",
          "fonts/NotoSans-Regular.ttf"}) {
      const auto target = package / relative;
      std::filesystem::create_directories(target.parent_path());
      std::filesystem::copy_file(std::filesystem::path("resources") / relative,
                                 target);
    }
  }

  void dirtyActor() {
    auto actor = editor.document()->characters.actors.front();
    actor.speed = .8F;
    ASSERT_TRUE(editor.replaceObject(
        editor.characterIds(EditorCharacterKind::Actor).front(), actor));
  }

  bool dispatch() {
    const auto request = play.consume();
    if (!request) return false;
    try {
      return launchEditorPlay(editor, *request, package, executable, process);
    } catch (const std::exception& error) {
      launch_error = error.what();
      return false;
    }
  }

  void waitForChild() {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (process.active() && std::chrono::steady_clock::now() < deadline) {
      process.poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_FALSE(process.active()) << process.status();
  }

  std::size_t childCount() const {
    const auto launches =
        bytes(std::filesystem::path(saved_path).concat(".launches"));
    return static_cast<std::size_t>(
        std::count(launches.begin(), launches.end(), '\n'));
  }

  std::filesystem::path saved_path, package, executable;
  EditorGameProcess process;
  std::string launch_error;
};

class EditorCharacterPlayFailure
    : public EditorCharacterPlay,
      public testing::WithParamInterface<std::string> {};
}  // namespace

TEST_P(EditorCharacterPlayFailure, FailedOrCanceledTransactionCreatesNoChild) {
  const auto& failure = GetParam();
  const bool needs_save_as =
      failure == "CancelSaveAs" || failure == "FailedSaveAs";
  if (!needs_save_as) {
    ASSERT_TRUE(editor.saveAs(saved_path));
    dirtyActor();
  }
  const auto before = *editor.document();
  const auto revision = editor.revision();
  const auto path = editor.path();
  const auto selected_entry = editor.launchEntry();
  const auto disk_before = bytes(saved_path);
  if (failure == "InvalidCharacter" || failure == "InvalidAudio") {
    auto actor = editor.document()->characters.actors.front();
    if (failure == "InvalidCharacter")
      actor.initial_mark = "absent-mark";
    else
      actor.interaction_source = "absent-source";
    ASSERT_TRUE(editor.replaceObject(
        editor.characterIds(EditorCharacterKind::Actor).front(), actor));
    EXPECT_FALSE(play.request(editor, false));
    EXPECT_FALSE(editor.save());
  } else {
    ASSERT_TRUE(play.request(editor, false));
    ASSERT_EQ(play.state(), EditorPlayState::ConfirmSave);
    if (failure == "CancelSave") {
      play.cancel();
    } else if (needs_save_as) {
      ASSERT_TRUE(play.saveAndPlay(editor));
      ASSERT_EQ(play.state(), EditorPlayState::SaveAs);
      if (failure == "CancelSaveAs") {
        play.cancel();
        EXPECT_FALSE(play.saveAsAndPlay(editor, saved_path));
      } else {
        EXPECT_FALSE(
            play.saveAsAndPlay(editor, root / "absent" / "level.json"));
      }
    } else if (failure == "FailedSave") {
      ASSERT_TRUE(std::filesystem::remove(saved_path));
      ASSERT_TRUE(std::filesystem::create_directory(saved_path));
      EXPECT_FALSE(play.saveAndPlay(editor));
    } else if (failure == "ChangedDocument") {
      auto actor = editor.document()->characters.actors.front();
      actor.speed = .7F;
      ASSERT_TRUE(editor.replaceObject(
          editor.characterIds(EditorCharacterKind::Actor).front(), actor));
      EXPECT_FALSE(play.saveAndPlay(editor));
    }
  }
  EXPECT_FALSE(dispatch());
  EXPECT_FALSE(dispatch());
  process.poll();
  EXPECT_FALSE(process.active());
  EXPECT_EQ(childCount(), 0U);
  EXPECT_TRUE(editor.dirty());
  EXPECT_EQ(editor.path(), path);
  EXPECT_EQ(editor.launchEntry(), selected_entry);
  if (failure == "CancelSave" || needs_save_as) {
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(bytes(saved_path), disk_before);
  }
  if (failure != "CancelSave" && failure != "CancelSaveAs")
    EXPECT_FALSE(play.error().empty());
}

INSTANTIATE_TEST_SUITE_P(
    CharacterTransactions, EditorCharacterPlayFailure,
    testing::Values("CancelSave", "CancelSaveAs", "FailedSave", "FailedSaveAs",
                    "ChangedDocument", "InvalidCharacter", "InvalidAudio"),
    [](const testing::TestParamInfo<std::string>& info) { return info.param; });

class EditorCharacterPlayAssetFailure
    : public EditorCharacterPlay,
      public testing::WithParamInterface<std::string> {};

TEST_P(EditorCharacterPlayAssetFailure,
       SelectedFailureAfterSavingNeedsFreshPlay) {
  copySelectedResources();
  ASSERT_TRUE(editor.saveAs(saved_path));
  dirtyActor();
  const auto authored = *editor.document();
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  ASSERT_FALSE(editor.dirty());
  const auto& failure = GetParam();
  std::filesystem::path damaged;
  std::string identity;
  if (failure == "MissingCharacter" || failure == "InvalidCharacterClip") {
    damaged = package / "characters/test-mannequin.glb";
    identity = "test-mannequin";
  } else if (failure == "MissingAudio" || failure == "InvalidActorAudio") {
    damaged = package / "audio/character-footstep.wav";
    identity = failure == "MissingAudio" ? "character-footstep" : "walker-step";
  } else if (failure == "MissingCaption") {
    damaged = package / "captions/character-interaction.captions";
    identity = "character-interaction";
  } else {
    damaged = package / "fonts/NotoSans-Regular.ttf";
    identity = "font";
  }
  const auto original = bytes(damaged);
  if (failure == "InvalidCharacterClip") {
    auto invalid = original;
    const auto clip = invalid.find("\"idle\"");
    ASSERT_NE(clip, std::string::npos);
    invalid.replace(clip, 6, "\"none\"");
    std::ofstream(damaged, std::ios::binary) << invalid;
  } else if (failure == "InvalidActorAudio") {
    std::filesystem::copy_file(
        "resources/audio/radio.wav", damaged,
        std::filesystem::copy_options::overwrite_existing);
  } else {
    ASSERT_TRUE(std::filesystem::remove(damaged));
  }
  EXPECT_FALSE(dispatch());
  EXPECT_NE(launch_error.find(identity), std::string::npos) << launch_error;
  EXPECT_FALSE(dispatch());
  process.poll();
  EXPECT_FALSE(process.active());
  EXPECT_EQ(childCount(), 0U);
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(*editor.document(), authored);
  EXPECT_EQ(*loadLevelDocument(saved_path).document, authored);
  std::ofstream(damaged, std::ios::binary) << original;
  // Repair cannot replay the consumed launch on a subsequent frame.
  EXPECT_FALSE(dispatch());
  EXPECT_EQ(childCount(), 0U);
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(dispatch()) << launch_error;
  waitForChild();
  EXPECT_EQ(childCount(), 1U);
  EXPECT_FALSE(dispatch());
  EXPECT_EQ(childCount(), 1U);
}

INSTANTIATE_TEST_SUITE_P(
    CharacterAssets, EditorCharacterPlayAssetFailure,
    testing::Values("MissingCharacter", "InvalidCharacterClip", "MissingAudio",
                    "InvalidActorAudio", "MissingCaption", "MissingFont"),
    [](const testing::TestParamInfo<std::string>& info) { return info.param; });

TEST_F(EditorCharacterPlay,
       DirtySaveAsLaunchesOneNativeChildWithFreshUnicodeScene) {
  copySelectedResources();
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  ASSERT_TRUE(play.saveAsAndPlay(editor, saved_path));
  const auto saved_bytes = bytes(saved_path);
  const auto authored = *editor.document();
  const auto original_cwd = std::filesystem::current_path();
  struct RestoreDirectory {
    std::filesystem::path path;
    ~RestoreDirectory() { std::filesystem::current_path(path); }
  } restore{original_cwd};
  std::filesystem::current_path(root);
  ASSERT_TRUE(dispatch()) << launch_error;
  EXPECT_FALSE(play.request(editor, process.active()));
  EXPECT_FALSE(dispatch());
  waitForChild();
  EXPECT_NE(process.status().find("code 0"), std::string::npos)
      << process.status();
  EXPECT_EQ(childCount(), 1U);
  EXPECT_EQ(bytes(std::filesystem::path(saved_path).concat(".observed")),
            saved_bytes);
  const auto native_path = saved_path.u8string();
  const auto native_cwd = root.u8string();
  EXPECT_EQ(bytes(std::filesystem::path(saved_path).concat(".arguments")),
            std::string(native_path.begin(), native_path.end()) +
                "\nprobe-launch\n" +
                std::string(native_cwd.begin(), native_cwd.end()) + "\n");
  EXPECT_EQ(bytes(saved_path), saved_bytes);
  EXPECT_EQ(*editor.document(), authored);
  EXPECT_FALSE(editor.dirty());
  EXPECT_FALSE(dispatch());
  EXPECT_EQ(childCount(), 1U);
}

TEST_F(EditorCharacterPlay,
       SavedCharacterFieldsAndEntryStayFreshUntilNativeLaunch) {
  ASSERT_TRUE(editor.saveAs(saved_path));
  const auto authored = *editor.document();
  for (int field = 0; field < 6; ++field) {
    SCOPED_TRACE(field);
    ASSERT_TRUE(editor.save());
    ASSERT_TRUE(play.request(editor, false));
    auto changed = authored;
    if (field == 0) changed.characters.actors.front().speed = .6F;
    if (field == 1) changed.characters.marks.back().yaw_degrees += 20;
    if (field == 2)
      std::reverse(changed.characters.routes.front().marks.begin(),
                   changed.characters.routes.front().marks.end());
    if (field == 3) changed.audio.sources.front().gain = .5F;
    if (field == 4) {
      changed.entries.front().id = "replaced-entry";
      changed.default_entry = "replaced-entry";
    }
    if (field == 5)
      ASSERT_TRUE(std::filesystem::remove(saved_path));
    else
      ASSERT_TRUE(saveLevelDocument(saved_path, changed));
    EXPECT_FALSE(dispatch());
    EXPECT_FALSE(launch_error.empty());
    if (field < 4)
      EXPECT_NE(launch_error.find("differs"), std::string::npos)
          << launch_error;
    if (field == 4)
      EXPECT_NE(launch_error.find("chosen entry"), std::string::npos)
          << launch_error;
    EXPECT_FALSE(dispatch());
    process.poll();
    EXPECT_FALSE(process.active());
    EXPECT_EQ(childCount(), 0U);
    EXPECT_EQ(*editor.document(), authored);
    EXPECT_FALSE(editor.dirty());
  }
}

TEST_F(EditorCharacterPlay, UnselectedCharacterAndAudioFilesAreNotRequired) {
  copySelectedResources();
  editor.select(editor.characterIds(EditorCharacterKind::Route).front());
  ASSERT_TRUE(editor.removeSelected());
  editor.select(editor.characterIds(EditorCharacterKind::Actor).front());
  ASSERT_TRUE(editor.removeSelected());
  for (const auto kind : {EditorAudioKind::Source, EditorAudioKind::Cue}) {
    while (!editor.audioIds(kind).empty()) {
      editor.select(editor.audioIds(kind).front());
      ASSERT_TRUE(editor.removeSelected());
    }
  }
  for (const auto* relative :
       {"characters/test-mannequin.glb", "audio/character-footstep.wav",
        "audio/character-interaction.wav",
        "captions/character-interaction.captions"})
    ASSERT_TRUE(std::filesystem::remove(package / relative));
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  ASSERT_TRUE(play.saveAsAndPlay(editor, saved_path));
  ASSERT_TRUE(dispatch()) << launch_error;
  waitForChild();
  EXPECT_EQ(childCount(), 1U);
  EXPECT_EQ(*loadLevelDocument(saved_path).document, *editor.document());
}

TEST_F(EditorPlay,
       UnsavedRequiresSaveConfirmationAndSaveAsThenOneConsumedIntent) {
  ASSERT_TRUE(play.request(editor, false));
  EXPECT_EQ(play.state(), EditorPlayState::ConfirmSave);
  EXPECT_FALSE(play.consume());
  EXPECT_FALSE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  EXPECT_EQ(play.state(), EditorPlayState::SaveAs);
  EXPECT_FALSE(play.consume());
  const auto path = root / std::filesystem::path(u8"комната & test.json");
  ASSERT_TRUE(play.saveAsAndPlay(editor, path));
  const auto request = play.consume();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->level_path, path);
  EXPECT_EQ(request->entry_id, "default");
  EXPECT_FALSE(editor.dirty());
  EXPECT_FALSE(play.consume());
  EXPECT_EQ(*loadLevelDocument(path).document, *editor.document());
}

TEST_F(EditorPlay, CancelFailedSaveAndChangedDocumentNeverArmALaterLaunch) {
  ASSERT_TRUE(play.request(editor, false));
  play.cancel();
  EXPECT_FALSE(play.consume());
  EXPECT_FALSE(editor.path());
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  play.cancel();
  EXPECT_FALSE(play.saveAsAndPlay(editor, root / "cancelled.json"));
  EXPECT_FALSE(std::filesystem::exists(root / "cancelled.json"));
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  EXPECT_FALSE(play.saveAsAndPlay(editor, root / "missing" / "fail.json"));
  EXPECT_EQ(play.state(), EditorPlayState::Idle);
  EXPECT_FALSE(play.error().empty());
  EXPECT_FALSE(play.consume());
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(editor.addEntry(editor.document()->entries[0].pose));
  EXPECT_FALSE(play.saveAndPlay(editor));
  EXPECT_FALSE(play.consume());
}

TEST_F(EditorPlay, ConsumedLaunchRechecksTheSavedDocumentBeforeAssetPreflight) {
  ASSERT_TRUE(editor.saveAs(root / "level.json"));
  ASSERT_TRUE(play.request(editor, false));
  const auto launch = play.consume();
  ASSERT_TRUE(launch);
  EXPECT_EQ(loadEditorPlayDocument(editor, *launch), *editor.document());
  ASSERT_TRUE(editor.addProp("apartment-phone"));
  EXPECT_THROW(static_cast<void>(loadEditorPlayDocument(editor, *launch)),
               std::runtime_error);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(loadEditorPlayDocument(editor, *launch), *editor.document());
  auto external = *editor.document();
  external.environment_light.ambient_intensity += .02F;
  ASSERT_TRUE(saveLevelDocument(launch->level_path, external));
  EXPECT_THROW(static_cast<void>(loadEditorPlayDocument(editor, *launch)),
               std::runtime_error);
}

TEST_F(EditorPlay, BrokenLightingLinksRefuseLaunchAndRepairSavesTheV8Snapshot) {
  ASSERT_TRUE(editor.addLightSwitch());
  ASSERT_TRUE(
      editor.saveAs(root / std::filesystem::path(u8"Свет и двери.json")));
  editor.select(editor.lightIds().front());
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_FALSE(editor.valid());
  EXPECT_FALSE(play.request(editor, false));
  EXPECT_FALSE(play.consume());
  ASSERT_TRUE(editor.undo());
  auto light = editor.document()->environment_light.point_lights.front();
  light.id = "renamed-source";
  light.initially_on = false;
  light.casts_shadows = true;
  ASSERT_TRUE(editor.replaceObject(editor.lightIds().front(), light));
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  const auto launch = play.consume();
  ASSERT_TRUE(launch);
  const auto snapshot = loadEditorPlayDocument(editor, *launch);
  EXPECT_EQ(snapshot, *editor.document());
  EXPECT_EQ(snapshot.light_switches.front().light_id, "renamed-source");
  EXPECT_FALSE(snapshot.environment_light.point_lights.front().initially_on);
  EXPECT_NE(bytes(launch->level_path).find("\"version\": 9"),
            std::string::npos);
}

TEST_F(EditorPlay, CleanAndDirtyPlayUseChosenEntryAndRejectExternalChanges) {
  ASSERT_TRUE(editor.addEntry(editor.document()->entries[0].pose));
  ASSERT_TRUE(editor.saveAs(root / "level.json"));
  ASSERT_TRUE(editor.selectLaunchEntry("entry-1"));
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(play.request(editor, false));
  EXPECT_EQ(play.consume()->entry_id, "entry-1");
  EXPECT_FALSE(play.request(editor, true));
  EXPECT_FALSE(play.consume());
  auto external = *editor.document();
  external.environment_light.point_lights[0].intensity += .2F;
  ASSERT_TRUE(saveLevelDocument(*editor.path(), external));
  EXPECT_FALSE(play.request(editor, false));
  EXPECT_NE(play.error().find("differs"), std::string::npos);
  EXPECT_FALSE(play.consume());
  ASSERT_TRUE(editor.save());
  auto entry = editor.document()->entries[1];
  entry.pose.yaw_degrees += 5;
  ASSERT_TRUE(editor.replaceObject(editor.entryIds()[1], entry));
  ASSERT_TRUE(play.request(editor, false));
  EXPECT_EQ(play.state(), EditorPlayState::ConfirmSave);
  play.cancel();
  EXPECT_TRUE(editor.dirty());
  ASSERT_TRUE(play.request(editor, false));
  ASSERT_TRUE(play.saveAndPlay(editor));
  EXPECT_EQ(play.consume()->entry_id, "entry-1");
  EXPECT_EQ(*loadLevelDocument(*editor.path()).document, *editor.document());
  entry.pose.foot_position.y += 5;
  ASSERT_TRUE(editor.replaceObject(editor.entryIds()[1], entry));
  ASSERT_TRUE(editor.selectLaunchEntry("default"));
  EXPECT_FALSE(play.request(editor, false));
  EXPECT_FALSE(play.consume());
}

TEST_F(EditorPlay, NativeChildReceivesLiteralUnicodeArgumentsAndReportsExit) {
  const auto extension =
      std::filesystem::path(EDITOR_ARGUMENT_PROBE_PATH).extension();
  auto executable = root / std::filesystem::path(u8"игра & literal; [] !");
  executable += extension;
  std::filesystem::copy_file(EDITOR_ARGUMENT_PROBE_PATH, executable);
  const auto output =
      root / std::filesystem::path(u8"комната & (literal); ' $ %.json");
  EditorGameProcess process;
  ASSERT_TRUE(process.start(executable, {output, "wait"})) << process.status();
  EXPECT_TRUE(process.active());
  EXPECT_FALSE(process.start(executable, {output, "default"}));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (process.active() && std::chrono::steady_clock::now() < deadline) {
    process.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_FALSE(process.active());
  const auto native_text = output.u8string();
  EXPECT_EQ(bytes(output),
            std::string(native_text.begin(), native_text.end()) + "\nwait\n");
  EXPECT_NE(process.status().find("code 0"), std::string::npos);
  ASSERT_TRUE(process.start(executable, {output, "exit-seven"}));
  while (process.active() && std::chrono::steady_clock::now() < deadline) {
    process.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_FALSE(process.active());
  EXPECT_NE(process.status().find("code 7"), std::string::npos);
  EXPECT_FALSE(process.start(root / "missing.exe", {output, "default"}));
  EXPECT_NE(process.status().find("missing.exe"), std::string::npos);
  EXPECT_NE(process.status().find("default"), std::string::npos);
  const auto invalid_executable = root / "invalid-executable.exe";
  {
    std::ofstream invalid(invalid_executable);
    invalid << "not an executable";
  }
  EXPECT_FALSE(process.start(invalid_executable, {output, "default"}));
  EXPECT_NE(process.status().find("invalid-executable.exe"), std::string::npos);
  EXPECT_FALSE(process.active());
}

TEST_F(EditorPlay, ClosingTheOwnerLeavesItsCreatedChildRunningIndependently) {
  const auto output = root / "child.json";
  auto begin = std::chrono::steady_clock::now();
  {
    EditorGameProcess process;
    ASSERT_TRUE(process.start(std::filesystem::path(EDITOR_ARGUMENT_PROBE_PATH),
                              {output, "wait"}));
    begin = std::chrono::steady_clock::now();
  }
  EXPECT_LT(std::chrono::steady_clock::now() - begin,
            std::chrono::milliseconds(300));
  const auto done = std::filesystem::path(output).concat(".done");
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!std::filesystem::exists(done) &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_EQ(bytes(done), "completed");
}
