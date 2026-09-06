#include <gtest/gtest.h>

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
}  // namespace

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
