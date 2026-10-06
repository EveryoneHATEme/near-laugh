#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

#include "editor/automation/engine_session.hpp"
#include "editor/automation/session.hpp"
#include "editor/editor_ui.hpp"
#include "editor/editor_widget_metadata.hpp"
#include "launcher/executable_path.hpp"

namespace editor_automation {
namespace {
class SemanticSession : public testing::Test {
 protected:
  virtual std::string fixture() const { return "apartment-stairs"; }
  virtual void drawFixtureUi() {}
  void SetUp() override {
    root_ = std::filesystem::temp_directory_path() /
        ("near-laugh-session-" + std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root_);
    const auto resources = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "resources";
    std::filesystem::copy_file(resources / "levels" / (fixture() + ".level.json"), root_ / "input.level.json");
    HANDLE input{}, output{};
    ASSERT_TRUE(CreatePipe(&input, &input_write_, nullptr, 0));
    ASSERT_TRUE(CreatePipe(&output_read_, &output, nullptr, 0));
    const auto old_input = GetStdHandle(STD_INPUT_HANDLE), old_output = GetStdHandle(STD_OUTPUT_HANDLE);
    SetStdHandle(STD_INPUT_HANDLE, input); SetStdHandle(STD_OUTPUT_HANDLE, output);
    try { channel_ = std::make_unique<Channel>(); }
    catch (...) {
      SetStdHandle(STD_INPUT_HANDLE, old_input); SetStdHandle(STD_OUTPUT_HANDLE, old_output);
      CloseHandle(input); CloseHandle(output); throw;
    }
    SetStdHandle(STD_INPUT_HANDLE, old_input); SetStdHandle(STD_OUTPUT_HANDLE, old_output);
    CloseHandle(input); CloseHandle(output);
    auto pathString = [](const std::filesystem::path& path) {
      const auto bytes = path.u8string(); return std::string(bytes.begin(), bytes.end());
    };
    Json slots = Json::array();
    for (int i = 0; i != 9; ++i) {
      const auto name = i == 0 ? std::string("input") : "output-" + std::to_string(i);
      slots.push_back({{"slot", name}, {"path", pathString(root_ / (name + ".level.json"))}, {"writable", true}});
    }
    session_ = std::make_unique<SessionController>(*channel_, Json{
        {"kind", "start"}, {"arguments", {{"protocol_version", 1}, {"request_id", "start"},
            {"op", "start"}, {"fixture", fixture()}, {"environment", "in-memory-test"}}},
        {"session_id", "test-session"}, {"root", pathString(root_)},
        {"resource_root", pathString(launcher::executableResourceRoot())},
        {"input_path", pathString(root_ / "input.level.json")}, {"file_slots", slots}});
    document_ = std::make_unique<EditorDocument>(session_->filePolicy());
    ASSERT_TRUE(document_->open(root_ / "input.level.json"));
    ImGui::CreateContext(); context_created_ = true;
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = {1600, 900}; io.DeltaTime = 1.F / 60.F;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    attachment_ = std::make_unique<SessionAttachment>(engine_, session_.get());
    engine_.start();
    const auto ready = receive("start");
    ASSERT_TRUE(ready.at("ok").get<bool>()) << ready.dump();
    for (int i = 0; i != 5; ++i) frame();
  }
  void TearDown() override {
    attachment_.reset();
    document_.reset();
    if (context_created_) { ImGui::DestroyContext(); context_created_ = false; }
    session_.reset(); channel_.reset();
    if (input_write_) CloseHandle(input_write_);
    if (output_read_) CloseHandle(output_read_);
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }
  void frame(bool available = true) {
    session_->service(available);
    if (!available || session_->closing()) return;
    ImGui::NewFrame(); session_->beginFrame(*document_);
    ui_.draw(*document_); drawFixtureUi(); ui_.finishFrame(); session_->publish(*document_, Json::object());
    if (monitor_drag_ && ImGui::GetIO().MouseDown[0]) {
      saw_held_drag_ = true;
      EXPECT_EQ(document_->revision(), drag_revision_);
      EXPECT_EQ(ImGui::GetInputTextState(ImGui::GetActiveID()), nullptr);
    }
  }
  void send(std::string_view tool, Json arguments) {
    arguments["protocol_version"] = 1; arguments["session_id"] = "test-session";
    const auto bytes = encodeMessage({{"kind", "call"}, {"tool", tool}, {"arguments", arguments}});
    DWORD written{};
    ASSERT_TRUE(WriteFile(input_write_, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr));
    ASSERT_EQ(written, bytes.size());
  }
  Json receive(std::string_view request, bool available = true) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    std::size_t received_bytes{}, buffered_before_read{};
    const auto diagnostic = [&](std::string_view reason) {
      return "Session receive request=" + std::string(request.substr(0, 64)) +
          " received_bytes=" + std::to_string(received_bytes) +
          " buffered_bytes=" + std::to_string(decoder_.bufferedBytes()) +
          " buffered_before_read=" + std::to_string(buffered_before_read) +
          ": " + std::string(reason.substr(0, 256));
    };
    try {
      while (std::chrono::steady_clock::now() < until) {
        // A response can exceed the pipe buffer. Drain it independently of UI
        // rendering, otherwise one expensive frame per fragment consumes the
        // decoder's unchanged five-second partial-line deadline.
        while (std::chrono::steady_clock::now() < until) {
          DWORD count{};
          if (!PeekNamedPipe(output_read_, nullptr, 0, nullptr, &count, nullptr))
            throw std::runtime_error("PeekNamedPipe failed: " + std::to_string(GetLastError()));
          if (!count) break;
          std::string bytes(std::min<DWORD>(count, 4096), '\0'); DWORD read{};
          if (!ReadFile(output_read_, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || !read)
            throw std::runtime_error("ReadFile failed: " + std::to_string(GetLastError()));
          bytes.resize(read);
          received_bytes += read;
          buffered_before_read = decoder_.bufferedBytes();
          for (auto& message : decoder_.feed(bytes)) {
            const auto& result = message.at("result");
            validateResult(message.at("tool").get<std::string>(), result);
            responses_[result.at("request_id").get<std::string>()] = result;
          }
        }
        if (const auto found = responses_.find(std::string(request)); found != responses_.end()) {
          auto result = found->second; responses_.erase(found); return result;
        }
        decoder_.expire();
        // Once a reply has started, its writer needs pipe space, not another
        // ImGui frame. Still expire stalled partial replies while yielding.
        if (decoder_.bufferedBytes() == 0) frame(available);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } catch (const std::exception& error) {
      throw std::runtime_error(diagnostic(error.what()));
    }
    throw std::runtime_error(diagnostic("No session result before the 15-second deadline"));
  }
  Json call(std::string_view tool, Json arguments) {
    const auto id = arguments.at("request_id").get<std::string>(); send(tool, std::move(arguments)); return receive(id);
  }
  Json observe() {
    auto result = call("ui_observe", {{"request_id", "observe-" + std::to_string(++reads_)},
        {"scope", "root"}, {"depth", 16}, {"page_size", 256}});
    if (!result.at("ok").get<bool>()) throw std::runtime_error(result.dump());
    auto cursor = result.at("next_cursor");
    while (!cursor.is_null()) {
      const auto page = call("ui_observe", {{"request_id", "observe-" + std::to_string(++reads_)}, {"cursor", cursor}});
      if (!page.at("ok").get<bool>()) throw std::runtime_error(page.dump());
      for (const auto& item : page.at("items")) result["items"].push_back(item);
      cursor = page.at("next_cursor");
    }
    return result;
  }
  Json item(std::string_view key) {
    const auto result = observe();
    for (const auto& item : result.at("items"))
      if (item.at("key") == std::string(key)) return item;
    throw std::runtime_error("Missing semantic control " + std::string(key));
  }
  Json execute(int id, Json steps) {
    return call("ui_execute", {{"request_id", std::to_string(id)}, {"steps", std::move(steps)},
        {"policy", {{"auto_scroll", true}, {"auto_focus", true}}}});
  }
  std::string selectRecord(int request_id, std::string_view type) {
    const auto objects = call("app_inspect", {{"request_id", "objects-" + std::to_string(++reads_)},
        {"projection", "objects"}, {"fields", {"record_type"}}});
    std::string owner;
    for (const auto& field : objects.at("values"))
      if (field.at("value").at("value") == std::string(type)) {
        owner = field.at("object_ref").get<std::string>(); break;
      }
    if (owner.empty()) throw std::runtime_error("No fixture object of the requested type");
    const auto observed = observe();
    for (const auto& row : observed.at("items")) {
      if (row.at("kind") == "selectable" && row.at("owner") == owner) {
        const auto selected = execute(request_id, Json::array({{{"op", "select"}, {"target", {{"ref", row.at("ref")}}}}}));
        if (!selected.at("ok").get<bool>()) throw std::runtime_error(selected.dump());
        return owner;
      }
    }
    throw std::runtime_error("Fixture object has no submitted selectable row");
  }
  EngineSession engine_;
  std::filesystem::path root_;
  HANDLE input_write_{}, output_read_{};
  std::unique_ptr<Channel> channel_;
  std::unique_ptr<SessionController> session_;
  std::unique_ptr<EditorDocument> document_;
  EditorUi ui_;
  std::unique_ptr<SessionAttachment> attachment_;
  bool context_created_{};
  JsonLineDecoder decoder_;
  std::map<std::string, Json> responses_;
  int reads_{};
  bool monitor_drag_{}, saw_held_drag_{};
  std::uint64_t drag_revision_{};
};

TEST_F(SemanticSession, LargeReadyResponseDoesNotDependOnFurtherUiFrames) {
  auto observed = call("ui_observe", {{"request_id", "large-observation"},
      {"scope", "root"}, {"depth", 16}, {"page_size", 256}});
  ASSERT_TRUE(observed.at("ok").get<bool>()) << observed.dump();
  observed["request_id"] = "large-ready-response";
  const Json message{{"kind", "result"}, {"tool", "ui_observe"}, {"result", observed}};
  // Exercise the real writer and a real schema-valid observation spanning
  // multiple pipe/reader fragments, with no request needing more UI frames.
  ASSERT_GT(encodeMessage(message).size(), 4U * 4096U);
  channel_->send(message);
  DWORD pending{};
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!pending && std::chrono::steady_clock::now() < until) {
    ASSERT_TRUE(PeekNamedPipe(output_read_, nullptr, 0, nullptr, &pending, nullptr));
    if (!pending) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_GT(pending, 0U);
  const auto frame_before = ImGui::GetFrameCount();
  EXPECT_EQ(receive("large-ready-response"), observed);
  EXPECT_EQ(ImGui::GetFrameCount(), frame_before);
}

TEST_F(SemanticSession, RealAmbientDraftSurvivesObservationAndASecondBatch) {
  const auto ambient = item("ambient-0-to-0-20");
  const Json target{{"ref", ambient.at("ref")}};
  const float old = document_->document()->environment_light.ambient_intensity;
  const auto revision = document_->revision();
  const auto edit = execute(1, Json::array({{{"op", "edit"}, {"target", target}, {"text", "0.075"}}}));
  ASSERT_TRUE(edit.at("ok").get<bool>()) << edit.dump();
  const auto draft = item("ambient-0-to-0-20");
  EXPECT_EQ(draft.at("input").at("text"), "0.075");
  EXPECT_TRUE(draft.at("state").at("active").get<bool>());
  EXPECT_EQ(document_->revision(), revision);
  EXPECT_FLOAT_EQ(document_->document()->environment_light.ambient_intensity, old);
  const auto inspected = call("app_inspect", {{"request_id", "inspect"}, {"projection", "document"},
      {"fields", {"ambient_intensity", "dirty"}}});
  EXPECT_TRUE(inspected.at("ok").get<bool>());
  EXPECT_TRUE(item("ambient-0-to-0-20").at("state").at("active").get<bool>());
  const auto commit = execute(2, Json::array({{{"op", "commit"}, {"target", target}, {"method", "enter"}}}));
  ASSERT_TRUE(commit.at("ok").get<bool>()) << commit.dump();
  EXPECT_FLOAT_EQ(document_->document()->environment_light.ambient_intensity, .075F);
  EXPECT_GT(document_->revision(), revision);
  EXPECT_EQ(ImGui::GetIO().KeyMods, 0);
}

TEST_F(SemanticSession, FailingAssertionStopsTheSuffixAndRetriesDoNotMutate) {
  const auto ambient = item("ambient-0-to-0-20");
  Json steps = Json::array({
      {{"op", "edit"}, {"target", {{"ref", ambient.at("ref")}}}, {"value", .08}, {"commit", "enter"}},
      {{"op", "assert"}, {"condition", {{"source", "app"}, {"projection", "document"},
          {"field", "ambient_intensity"}, {"predicate", "equals"}, {"expected", .19}}}},
      {{"op", "edit"}, {"target", {{"ref", ambient.at("ref")}}}, {"value", .12}, {"commit", "enter"}}});
  const auto result = execute(1, steps);
  ASSERT_FALSE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(result.at("error").at("code"), "assertion_failed");
  EXPECT_EQ(result["steps"][0]["status"], "passed");
  EXPECT_EQ(result["steps"][1]["status"], "failed");
  EXPECT_EQ(result["steps"][2]["status"], "not_run");
  EXPECT_FLOAT_EQ(document_->document()->environment_light.ambient_intensity, .08F);
  EXPECT_LT(std::stoull(result["error"]["snapshot"]["frame"].get<std::string>()),
            std::stoull(result["after"]["frame"].get<std::string>()));
  const auto revision = document_->revision();
  EXPECT_EQ(execute(1, steps), result);
  EXPECT_EQ(document_->revision(), revision);
  steps[0]["value"] = .1;
  EXPECT_EQ(execute(1, steps)["error"]["code"], "request_id_conflict");
  EXPECT_TRUE(observe().at("ok").get<bool>());
}

TEST_F(SemanticSession, DragAndReleaseCommitsThroughTheRealVectorWidget) {
  const auto owner = selectRecord(1, "solid");
  const auto target = Json{{"ref", item("center.x").at("ref")}};
  const auto before = std::get<PrototypeSolid>(*document_->object(document_->selection())).center.x;
  monitor_drag_ = true; drag_revision_ = document_->revision();
  const auto result = execute(2, Json::array({
      {{"op", "drag"}, {"target", target}, {"direction", "increase"}, {"fraction", .4}, {"frames", 8}},
      {{"op", "assert"}, {"condition", {{"source", "app"}, {"projection", "object"},
          {"object_ref", owner}, {"field", "center.x"}, {"predicate", "not_equals"}, {"expected", before}}}}}));
  monitor_drag_ = false;
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_TRUE(saw_held_drag_);
  EXPECT_FALSE(ImGui::GetIO().MouseDown[0]);
  EXPECT_GT(document_->revision(), drag_revision_);
  EXPECT_TRUE(document_->canUndo());
  EXPECT_GT(std::get<PrototypeSolid>(*document_->object(document_->selection())).center.x, before);
}

TEST_F(SemanticSession, CancelDuringHeldDragReleasesInputAndDoesNotRunSuffix) {
  selectRecord(1, "solid");
  const auto target = Json{{"ref", item("center.x").at("ref")}};
  send("ui_execute", {{"request_id", "2"}, {"policy", {{"auto_scroll", true}, {"auto_focus", true}}},
      {"steps", {{{"op", "drag"}, {"target", target}, {"direction", "increase"}, {"fraction", .5}, {"frames", 120}},
                 {{"op", "edit"}, {"target", target}, {"value", 999}, {"commit", "tab"}}}}});
  for (int i = 0; i < 500 && !ImGui::GetIO().MouseDown[0]; ++i) {
    frame(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_TRUE(ImGui::GetIO().MouseDown[0]);
  send("ui_session", {{"request_id", "cancel-held"}, {"op", "cancel"}, {"active_request_id", "2"}});
  const auto result = receive("2");
  EXPECT_EQ(result["error"]["code"], "cancelled") << result.dump();
  EXPECT_EQ(result["steps"][1]["status"], "not_run");
  EXPECT_EQ(result["cleanup"], "released");
  EXPECT_FALSE(ImGui::GetIO().MouseDown[0]);
  EXPECT_EQ(ImGui::GetIO().KeyMods, 0);
  EXPECT_NE(std::get<PrototypeSolid>(*document_->object(document_->selection())).center.x, 999);
}

TEST_F(SemanticSession, EscapeIsSeparateFromCommitAndWaitTimeoutReportsActualValue) {
  const Json target{{"ref", item("ambient-0-to-0-20").at("ref")}};
  const auto old = document_->document()->environment_light.ambient_intensity;
  const auto revision = document_->revision();
  ASSERT_TRUE(execute(1, Json::array({{{"op", "edit"}, {"target", target}, {"text", "0.123"}}})).at("ok").get<bool>());
  const auto result = execute(2, Json::array({
      {{"op", "key"}, {"target", target}, {"chord", "Escape"}},
      {{"op", "wait_until"}, {"timeout_ms", 50}, {"condition", {{"source", "app"}, {"projection", "document"},
          {"field", "dirty"}, {"predicate", "equals"}, {"expected", true}}}},
      {{"op", "edit"}, {"target", target}, {"value", .12}, {"commit", "enter"}}}));
  EXPECT_EQ(result["error"]["code"], "timeout") << result.dump();
  EXPECT_EQ(result["error"]["expected"]["value"], true);
  EXPECT_EQ(result["error"]["observed"]["value"], false);
  EXPECT_EQ(result["steps"][2]["status"], "not_run");
  EXPECT_FLOAT_EQ(document_->document()->environment_light.ambient_intensity, old);
  EXPECT_EQ(document_->revision(), revision);
  EXPECT_EQ(ImGui::GetIO().KeyMods, 0);
}

class IgnoredHandlerSession : public SemanticSession {
 protected:
  void drawFixtureUi() override {
    ImGui::SetNextWindowPos({600, 400}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300, 140}, ImGuiCond_Always);
    EditorWidgets::Begin("Fault injection fixture");
    EditorWidgetMetadata::next("ignored-ambient", "ambient_intensity", "", "document");
    // Intentionally broken test handler. The actual document projection must
    // never echo this UI draft or turn accepted input into applied-state proof.
    if (EditorWidgets::InputFloat("Ignored ambient", &draft_, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
      ++handler_calls_;
    EditorWidgets::InputText("Text only", text_, sizeof(text_));
    EditorWidgets::End();
  }
  float draft_{};
  int handler_calls_{};
  char text_[32]{};
};

TEST_F(IgnoredHandlerSession, DocumentAssertionFailsWhenHandlerIntentionallyDoesNotApplyInput) {
  const auto target = Json{{"ref", item("ignored-ambient").at("ref")}};
  const auto original = document_->document()->environment_light.ambient_intensity;
  const Json request{{"protocol_version", 1}, {"session_id", "test-session"}, {"request_id", "1"},
      {"policy", {{"auto_scroll", true}, {"auto_focus", true}}}, {"steps", Json::array({
      {{"op", "edit"}, {"target", target}, {"value", .123}, {"commit", "enter"}},
      {{"op", "assert"}, {"condition", {{"source", "app"}, {"projection", "document"},
          {"field", "ambient_intensity"}, {"predicate", "approx"}, {"expected", .123},
          {"tolerance", {{"absolute", .0001}}}}}},
      {{"op", "edit"}, {"target", target}, {"value", .15}}})}};
  const auto result = call("ui_execute", request);
  RecordProperty("request", request.dump());
  RecordProperty("response", result.dump());
  EXPECT_GT(handler_calls_, 0);
  EXPECT_FLOAT_EQ(draft_, .123F);
  EXPECT_EQ(result["steps"][0]["status"], "passed");
  EXPECT_EQ(result["steps"][1]["status"], "failed");
  EXPECT_EQ(result["steps"][2]["status"], "not_run");
  EXPECT_EQ(result["error"]["code"], "assertion_failed");
  EXPECT_FLOAT_EQ(result["error"]["observed"]["value"].get<float>(), original);
  EXPECT_FLOAT_EQ(document_->document()->environment_light.ambient_intensity, original);
}

TEST_F(IgnoredHandlerSession, WrongValueTypeDoesNotActivateTheWidget) {
  const auto target = Json{{"ref", item("text-only").at("ref")}};
  const auto active = ImGui::GetActiveID();
  const auto result = execute(1, Json::array({{{"op", "edit"}, {"target", target}, {"value", 12}}}));
  EXPECT_EQ(result["error"]["code"], "invalid_request");
  EXPECT_EQ(result["effects"], "none");
  EXPECT_EQ(ImGui::GetActiveID(), active);
  EXPECT_STREQ(text_, "");
}

class OversizedTextSession : public SemanticSession {
 protected:
  void drawFixtureUi() override {
    ImGui::SetNextWindowPos({600, 400}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300, 180}, ImGuiCond_Always);
    EditorWidgets::Begin("Long text fixture");
    EditorWidgetMetadata::next("long-text");
    EditorWidgets::InputText("Long text", text_.data(), text_.size() + 1);
    EditorWidgetMetadata::next("long-label");
    EditorWidgets::Button(text_.c_str());
    EditorWidgets::End();
  }
  // The UTF-8 character straddles the byte budget; truncation must keep valid UTF-8.
  std::string text_ = std::string(16383, 'a') + "\xD0\xAF" + "tail";
};

TEST_F(OversizedTextSession, TruncatedInactiveTextAndLabelsCannotPassFullComparisons) {
  const auto active = ImGui::GetActiveID();
  const auto revision = document_->revision();
  const auto text = item("long-text");
  const auto label = item("long-label");
  EXPECT_EQ(text["input"]["availability"], "truncated");
  EXPECT_EQ(text["input"]["text"], std::string(16383, 'a'));
  EXPECT_EQ(text["value"]["availability"], "truncated");
  EXPECT_EQ(label["label_availability"], "truncated");
  EXPECT_EQ(label["label"], std::string(16383, 'a'));
  for (int index = 0; index != 3; ++index) {
    const auto& target = index == 2 ? label : text;
    const char* field = index == 2 ? "label" : index == 1 ? "value.draft" : "input.text";
    const auto result = execute(index + 1, Json::array({
        {{"op", "assert"}, {"condition", {{"source", "ui"}, {"target", {{"ref", target["ref"]}}},
            {"field", field}, {"predicate", index == 1 ? "not_equals" : "equals"},
            {"expected", std::string(16383, 'a')}}}},
        {{"op", "activate"}, {"target", {{"ref", label["ref"]}}}}}));
    EXPECT_EQ(result["error"]["code"], "value_unavailable") << result.dump();
    EXPECT_EQ(result["error"]["observed"]["availability"], "truncated");
    EXPECT_EQ(result["steps"][1]["status"], "not_run");
    EXPECT_EQ(result["cleanup"], "released");
    EXPECT_EQ(result["effects"], "none");
  }
  EXPECT_EQ(ImGui::GetActiveID(), active);
  EXPECT_EQ(document_->revision(), revision);
}

TEST_F(SemanticSession, WholeBatchValidationPrecedesInputAndGuardPrecedesMutation) {
  const auto ambient = item("ambient-0-to-0-20");
  const auto revision = document_->revision();
  const auto result = call("ui_execute", {{"request_id", "1"},
      {"if_state", {{"document_revision", "999999"}}},
      {"steps", {{{"op", "edit"}, {"target", {{"ref", ambient.at("ref")}}}, {"value", .08}, {"commit", "enter"}}}}});
  EXPECT_EQ(result["error"]["code"], "state_conflict");
  EXPECT_EQ(result["steps"][0]["status"], "not_run");
  EXPECT_EQ(document_->revision(), revision);
}

TEST_F(SemanticSession, MinimizedObservationIsUnavailableAndCancelIsServiced) {
  send("ui_observe", {{"request_id", "minimized"}, {"scope", "root"}});
  const auto missing = receive("minimized", false);
  EXPECT_EQ(missing["error"]["code"], "ui_unavailable");
  EXPECT_TRUE(missing["error"]["snapshot"]["stale"].get<bool>());
  const Json condition{{"source", "app"}, {"projection", "document"}, {"field", "dirty"},
                       {"predicate", "equals"}, {"expected", true}};
  send("ui_execute", {{"request_id", "1"},
      {"steps", Json::array({{{"op", "wait_until"}, {"condition", condition}}})}});
  for (int i = 0; i != 10; ++i) frame();
  send("ui_session", {{"request_id", "cancel"}, {"op", "cancel"}, {"active_request_id", "1"}});
  EXPECT_TRUE(receive("cancel", false).at("ok").get<bool>());
  const auto result = receive("1", false);
  EXPECT_FALSE(result.at("ok").get<bool>());
  EXPECT_EQ(result["cleanup"], "unverified");
  EXPECT_EQ(result["effects"], "unknown");
  EXPECT_EQ(result["steps"][0]["status"], "unknown");
}

TEST_F(SemanticSession, MissingAndUnsupportedTargetsLeaveTheSessionRecoverable) {
  const auto missing = execute(1, Json::array({{{"op", "activate"},
      {"target", {{"selector", {{"scope", "root"}, {"key", "absent-control"}}}}}}}));
  EXPECT_EQ(missing["error"]["code"], "not_found");
  EXPECT_EQ(missing["effects"], "none");
  EXPECT_EQ(missing["cleanup"], "released");
  const auto items = observe().at("items");
  std::map<std::pair<std::string, std::string>, std::size_t> identities;
  for (const auto& item : items)
    if (item.at("kind") == "selectable")
      ++identities[{item.at("scope").get<std::string>(), item.at("key").get<std::string>()}];
  bool found_ambiguity{};
  for (const auto& [identity, count] : identities) if (count > 1) {
    const auto ambiguous = execute(2, Json::array({{{"op", "select"}, {"target", {{"selector", {
        {"scope", identity.first}, {"key", identity.second}}}}}}}));
    EXPECT_EQ(ambiguous["error"]["code"], "ambiguous_target");
    EXPECT_GE(ambiguous["error"]["candidates"].size(), 2U);
    EXPECT_EQ(ambiguous["effects"], "none");
    found_ambiguity = true; break;
  }
  ASSERT_TRUE(found_ambiguity);
  for (const auto& item : items) if (item.at("kind") == "viewport") {
    const auto result = execute(3, Json::array({{{"op", "activate"}, {"target", {{"ref", item.at("ref")}}}}}));
    EXPECT_EQ(result["error"]["code"], "unsupported");
    EXPECT_EQ(result["effects"], "none");
    return;
  }
  FAIL() << "The actual workspace did not expose its excluded viewport";
}

TEST_F(SemanticSession, EarlyCancellationPrecedesMatchingBatchInput) {
  const auto ambient = item("ambient-0-to-0-20");
  const auto revision = document_->revision();
  const auto value = document_->document()->environment_light.ambient_intensity;
  const auto cancelled = call("ui_session", {{"request_id", "early-cancel"}, {"op", "cancel"}, {"active_request_id", "1"}});
  ASSERT_TRUE(cancelled.at("ok").get<bool>());
  const Json edit{{"op", "edit"}, {"target", {{"ref", ambient.at("ref")}}},
                  {"value", .091}, {"commit", "enter"}};
  const auto result = execute(1, Json::array({edit, edit}));
  ASSERT_FALSE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(result["error"]["code"], "cancelled");
  EXPECT_EQ(result["steps"][0]["status"], "failed");
  EXPECT_EQ(result["steps"][1]["status"], "not_run");
  EXPECT_EQ(result["effects"], "none");
  EXPECT_EQ(result["cleanup"], "released");
  EXPECT_EQ(document_->revision(), revision);
  EXPECT_FLOAT_EQ(document_->document()->environment_light.ambient_intensity, value);
  EXPECT_FALSE(channel_->cancellationRequested("test-session", "1"));
  const auto next = execute(2, Json::array({{{"op", "assert"}, {"condition", {
      {"source", "app"}, {"projection", "document"}, {"field", "valid"},
      {"predicate", "equals"}, {"expected", true}}}}}));
  EXPECT_TRUE(next.at("ok").get<bool>()) << next.dump();
}

TEST_F(SemanticSession, EarlyCancellationStorageIsBoundedAndDuplicateIdsShareCapacity) {
  for (int id = 1; id <= 16; ++id) {
    const auto result = call("ui_session", {{"request_id", "early-" + std::to_string(id)},
        {"op", "cancel"}, {"active_request_id", std::to_string(id)}});
    ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  }
  const auto duplicate = call("ui_session", {{"request_id", "duplicate-cancel"},
      {"op", "cancel"}, {"active_request_id", "1"}});
  ASSERT_TRUE(duplicate.at("ok").get<bool>()) << duplicate.dump();
  EXPECT_FALSE(channel_->closed());
  send("ui_session", {{"request_id", "overflow-cancel"}, {"op", "cancel"}, {"active_request_id", "17"}});
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!channel_->closed() && std::chrono::steady_clock::now() < until)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  ASSERT_TRUE(channel_->closed());
  EXPECT_EQ(channel_->error(), "Private cancellation queue is full");
}

TEST_F(SemanticSession, RepeatedCancelCannotExtendAStalledCleanupDeadline) {
  send("ui_execute", {{"request_id", "1"}, {"steps", {{{"op", "wait_until"}, {"condition", {
      {"source", "app"}, {"projection", "document"}, {"field", "dirty"},
      {"predicate", "equals"}, {"expected", true}}}}}}});
  for (int i = 0; i != 10; ++i) frame();
  const auto started = std::chrono::steady_clock::now();
  for (int i = 0; i != 5; ++i) {
    const auto id = "cancel-" + std::to_string(i);
    send("ui_session", {{"request_id", id}, {"op", "cancel"}, {"active_request_id", "1"}});
    ASSERT_TRUE(receive(id, false).at("ok").get<bool>());
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
  }
  const auto result = receive("1", false);
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(3));
  EXPECT_EQ(result["cleanup"], "unverified");
  EXPECT_EQ(result["steps"][0]["status"], "unknown");
}

TEST_F(SemanticSession, CancellationOfAQueuedBatchPrecedesItsFirstInput) {
  const auto ambient = item("ambient-0-to-0-20");
  const auto revision = document_->revision();
  send("ui_execute", {{"request_id", "1"}, {"steps", {{{"op", "edit"},
      {"target", {{"ref", ambient.at("ref")}}}, {"value", .091}, {"commit", "enter"}}}}});
  send("ui_session", {{"request_id", "cancel-queued"}, {"op", "cancel"}, {"active_request_id", "1"}});
  // Let the reader register both wire messages before the UI services either.
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  ASSERT_TRUE(receive("cancel-queued").at("ok").get<bool>());
  const auto result = receive("1");
  EXPECT_EQ(result["error"]["code"], "cancelled");
  EXPECT_EQ(result["effects"], "none");
  EXPECT_EQ(result["cleanup"], "released");
  EXPECT_EQ(document_->revision(), revision);
}

TEST_F(SemanticSession, EvictedRequestIdsNeverReplayAnEdit) {
  const Json steps = {{{"op", "assert"}, {"condition", {
      {"source", "app"}, {"projection", "document"}, {"field", "valid"},
      {"predicate", "equals"}, {"expected", true}}}}};
  for (int id = 1; id <= 34; ++id) ASSERT_TRUE(execute(id, steps).at("ok").get<bool>());
  const auto revision = document_->revision();
  EXPECT_EQ(execute(1, steps)["error"]["code"], "result_expired");
  EXPECT_EQ(document_->revision(), revision);
}

TEST_F(SemanticSession, RetainedSnapshotAndCursorAreStaleWhenFramesAreUnavailable) {
  const auto observed = call("ui_observe", {{"request_id", "page"}, {"scope", "root"},
      {"depth", 16}, {"page_size", 1}});
  ASSERT_FALSE(observed.at("next_cursor").is_null());
  send("ui_observe", {{"request_id", "continuation"}, {"cursor", observed.at("next_cursor")}});
  const auto continuation = receive("continuation", false);
  ASSERT_TRUE(continuation.at("ok").get<bool>()) << continuation.dump();
  EXPECT_TRUE(continuation["snapshot"]["stale"].get<bool>());
  send("app_inspect", {{"request_id", "retained"}, {"projection", "document"},
      {"snapshot_id", observed["snapshot"]["snapshot_id"]}, {"fields", {"dirty"}}});
  const auto retained = receive("retained", false);
  ASSERT_TRUE(retained.at("ok").get<bool>()) << retained.dump();
  EXPECT_TRUE(retained["snapshot"]["stale"].get<bool>());
  EXPECT_EQ(retained["snapshot"]["snapshot_id"], observed["snapshot"]["snapshot_id"]);
}

TEST_F(SemanticSession, EvictedSnapshotsAndCursorsNeverReturnFreshData) {
  const auto page = call("ui_observe", {{"request_id", "first-page"}, {"scope", "root"},
      {"depth", 16}, {"page_size", 1}});
  ASSERT_FALSE(page.at("next_cursor").is_null());
  const auto cursor = page.at("next_cursor");
  const auto second = call("ui_observe", {{"request_id", "second-page"}, {"cursor", cursor}});
  ASSERT_TRUE(second.at("ok").get<bool>());
  const auto retry = call("ui_observe", {{"request_id", "retry-page"}, {"cursor", cursor}});
  ASSERT_TRUE(retry.at("ok").get<bool>());
  EXPECT_EQ(second.at("items"), retry.at("items"));
  // Cursor pages deliberately pin their immutable snapshots. Exceed both
  // retention bounds before requiring an expiration error.
  for (int i = 0; i != 48; ++i)
    ASSERT_TRUE(call("ui_observe", {{"request_id", "pin-" + std::to_string(i)},
        {"scope", "root"}, {"depth", 16}, {"page_size", 1}}).at("ok").get<bool>());
  const auto evicted = call("ui_observe", {{"request_id", "evicted-page"}, {"cursor", cursor}});
  ASSERT_FALSE(evicted.at("ok").get<bool>());
  EXPECT_EQ(evicted.at("error").at("code"), "snapshot_expired");
  const auto expired = call("app_inspect", {{"request_id", "old-snapshot"},
      {"projection", "document"}, {"snapshot_id", page["snapshot"]["snapshot_id"]}});
  ASSERT_FALSE(expired.at("ok").get<bool>());
  EXPECT_EQ(expired.at("error").at("code"), "snapshot_expired");
  EXPECT_TRUE(observe().at("ok").get<bool>());
}

TEST_F(SemanticSession, TypedPredicatesUseCompleteImmutableValues) {
  const auto ambient = item("ambient-0-to-0-20");
  const double value = document_->document()->environment_light.ambient_intensity;
  Json steps = Json::array();
  const auto app = [&](std::string_view predicate, Json expected) {
    return Json{{"source", "app"}, {"projection", "document"}, {"field", "ambient_intensity"},
                {"predicate", predicate}, {"expected", std::move(expected)}};
  };
  steps.push_back({{"op", "assert"}, {"condition", app("range", {{"min", 0}, {"max", .2}})}});
  auto approximate = app("approx", value); approximate["tolerance"] = {{"absolute", .00001}};
  steps.push_back({{"op", "wait_until"}, {"condition", approximate}});
  steps.push_back({{"op", "assert"}, {"condition", {{"source", "ui"},
      {"target", {{"ref", ambient.at("ref")}}}, {"field", "label"},
      {"predicate", "contains"}, {"expected", "Ambient"}}}});
  const auto objects = call("app_inspect", {{"request_id", "count-objects"},
      {"projection", "objects"}, {"fields", {"record_type"}}});
  steps.push_back({{"op", "assert"}, {"condition", {{"source", "app"},
      {"projection", "objects"}, {"field", "record_type"}, {"predicate", "count"},
      {"expected", objects.at("values").size()}}}});
  const auto result = execute(1, steps);
  EXPECT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(result["effects"], "none");
  const auto unavailable = execute(2, Json::array({{{"op", "assert"}, {"condition", {
      {"source", "app"}, {"projection", "preview"}, {"field", "readable.text"},
      {"predicate", "not_equals"}, {"expected", "anything"}}}}}));
  EXPECT_EQ(unavailable["error"]["code"], "value_unavailable");
}

TEST_F(SemanticSession, RealMenuDisabledAndModalStatesAreEnforced) {
  auto menu = item("edit");
  auto result = execute(1, Json::array({{{"op", "open"}, {"target", {{"ref", menu.at("ref")}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  const auto undo = item("undo");
  result = execute(2, Json::array({{{"op", "activate"}, {"target", {{"ref", undo.at("ref")}}}}}));
  EXPECT_EQ(result["error"]["code"], "disabled");
  EXPECT_EQ(result["effects"], "none");
  menu = item("file");
  result = execute(3, Json::array({{{"op", "open"}, {"target", {{"ref", menu.at("ref")}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  const auto save = item("save-as");
  result = execute(4, Json::array({{{"op", "activate"}, {"target", {{"ref", save.at("ref")}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  ASSERT_FALSE(observe().at("modal_scope").is_null());
  const auto ambient = item("ambient-0-to-0-20");
  result = execute(5, Json::array({{{"op", "edit"}, {"target", {{"ref", ambient.at("ref")}}}, {"value", .08}}}));
  EXPECT_EQ(result["error"]["code"], "blocked_by_modal");
  EXPECT_EQ(result["effects"], "none");
}

TEST_F(SemanticSession, WindowFocusAndSemanticScrollingUseAbsoluteWindowIdentity) {
  const auto observed = observe();
  Json window;
  Json row;
  for (const auto& record : observed.at("items")) {
    if (record.at("kind") == "window" && record.at("label") == "Objects") window = record;
    if (record.at("kind") == "selectable") row = record;
  }
  ASSERT_FALSE(window.is_null());
  ASSERT_FALSE(row.is_null());
  const auto revision = document_->revision();
  const Json target{{"ref", window.at("ref")}};
  const auto result = execute(1, Json::array({
      {{"op", "focus"}, {"target", target}},
      {{"op", "scroll"}, {"target", target}, {"direction", "down"}, {"pages", 1}},
      {{"op", "scroll"}, {"target", target}, {"direction", "up"}, {"pages", 1}},
      {{"op", "scroll"}, {"target", target}, {"to", {{"ref", row.at("ref")}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(document_->revision(), revision);
  const auto after = observe();
  bool stable{};
  for (const auto& record : after.at("items"))
    if (record.at("kind") == "window" && record.at("label") == "Objects")
      stable = record.at("ref") == window.at("ref");
  EXPECT_TRUE(stable);
}

TEST_F(SemanticSession, StrictClippedTargetRequiresExplicitAssistanceAndReportsIt) {
  const auto observed = observe();
  Json window, row;
  for (const auto& record : observed.at("items")) {
    if (record.at("kind") == "window" && record.at("label") == "Objects") window = record;
    if (record.at("kind") == "selectable" && row.is_null()) row = record;
  }
  ASSERT_FALSE(window.is_null()); ASSERT_FALSE(row.is_null());
  ASSERT_TRUE(execute(1, Json::array({{{"op", "scroll"}, {"target", {{"ref", window.at("ref")}}},
      {"direction", "down"}, {"pages", 16}}})).at("ok").get<bool>());
  const auto selection = document_->selection();
  const Json steps = Json::array({{{"op", "select"}, {"target", {{"ref", row.at("ref")}}}}});
  const auto strict = call("ui_execute", {{"request_id", "2"}, {"steps", steps}});
  EXPECT_EQ(strict["error"]["code"], "ui_unavailable") << strict.dump();
  EXPECT_EQ(strict["effects"], "none");
  EXPECT_TRUE(strict["steps"][0]["assistance"].empty());
  EXPECT_EQ(document_->selection(), selection);
  const auto assisted = execute(3, steps);
  ASSERT_TRUE(assisted.at("ok").get<bool>()) << assisted.dump();
  ASSERT_FALSE(assisted["steps"][0]["assistance"].empty());
  EXPECT_EQ(assisted["steps"][0]["assistance"][0]["action"], "scroll");
}

TEST_F(SemanticSession, StrictWindowKeyFocusAndScrollCannotDismissPopup) {
  const auto observed = observe();
  Json summary;
  for (const auto& record : observed.at("items"))
    if (record.at("kind") == "window" && record.at("label") == "Document Summary") summary = record;
  ASSERT_FALSE(summary.is_null());
  const auto edit = item("edit");
  ASSERT_TRUE(execute(1, Json::array({{{"op", "open"}, {"target", {{"ref", edit.at("ref")}}}}})).at("ok").get<bool>());
  int request = 2;
  for (const auto& step : Json::array({
      {{"op", "key"}, {"target", {{"ref", summary.at("ref")}}}, {"chord", "Escape"}},
      {{"op", "focus"}, {"target", {{"ref", summary.at("ref")}}}},
      {{"op", "scroll"}, {"target", {{"ref", summary.at("ref")}}}, {"direction", "down"}, {"pages", 1}}})) {
    const auto result = call("ui_execute", {{"request_id", std::to_string(request++)}, {"steps", Json::array({step})}});
    EXPECT_EQ(result["error"]["code"], "state_conflict") << result.dump();
    EXPECT_EQ(result["effects"], "none");
    EXPECT_TRUE(item("edit")["state"]["open"].get<bool>());
  }
  const auto file = item("file");
  const auto unrelated_close = execute(request++, Json::array({{{"op", "close"}, {"target", {{"ref", file.at("ref")}}}}}));
  EXPECT_FALSE(unrelated_close.at("ok").get<bool>());
  EXPECT_TRUE(item("edit")["state"]["open"].get<bool>());
  const auto closed = execute(request, Json::array({{{"op", "close"}, {"target", {{"ref", edit.at("ref")}}}}}));
  ASSERT_TRUE(closed.at("ok").get<bool>()) << closed.dump();
  EXPECT_FALSE(item("edit")["state"]["open"].get<bool>());
}

TEST_F(SemanticSession, StrictFocusDoesNotUncollapseTheTargetWindow) {
  Json summary;
  const auto observed = observe();
  for (const auto& record : observed.at("items"))
    if (record.at("kind") == "window" && record.at("label") == "Document Summary") summary = record;
  ASSERT_FALSE(summary.is_null());
  // Explicit fixture layout preparation, never an implementation of a wire action.
  ImGui::SetWindowCollapsed("Document Summary", true);
  frame(); frame();
  const auto result = call("ui_execute", {{"request_id", "1"},
      {"steps", {{{"op", "focus"}, {"target", {{"ref", summary.at("ref")}}}}}}});
  EXPECT_EQ(result["error"]["code"], "ui_unavailable") << result.dump();
  EXPECT_EQ(result["effects"], "none");
  EXPECT_TRUE(ImGui::FindWindowByName("Document Summary")->Collapsed);
}

TEST_F(SemanticSession, RealVectorColorComboAndCheckboxUseEngineInput) {
  selectRecord(1, "solid");
  auto component = item("center.x");
  auto result = execute(2, Json::array({
      {{"op", "edit"}, {"target", {{"ref", component.at("ref")}}}, {"text", "-"}},
      {{"op", "assert"}, {"condition", {{"source", "ui"}, {"target", {{"ref", component.at("ref")}}},
          {"field", "input_validation.status"}, {"predicate", "equals"}, {"expected", "invalid"}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(item("center.x")["input"]["text"], "-");
  EXPECT_EQ(item("center.x")["value"]["availability"], "known");
  EXPECT_EQ(item("center.x")["input_validation"]["status"], "invalid");
  result = execute(3, Json::array({{{"op", "edit"}, {"target", {{"ref", component.at("ref")}}},
      {"value", 3.125}, {"commit", "tab"}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_FLOAT_EQ(std::get<PrototypeSolid>(*document_->object(document_->selection())).center.x, 3.125F);
  component = item("tint.r");
  result = execute(4, Json::array({{{"op", "edit"}, {"target", {{"ref", component.at("ref")}}},
      {"value", 127}, {"commit", "tab"}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(std::get<PrototypeSolid>(*document_->object(document_->selection())).color[0], 127);
  const auto combo = item("kind");
  result = execute(5, Json::array({{{"op", "open"}, {"target", {{"ref", combo.at("ref")}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  const auto option = item("option-1");
  result = execute(6, Json::array({{{"op", "select"}, {"target", {{"ref", option.at("ref")}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(std::get<PrototypeSolid>(*document_->object(document_->selection())).kind, PrototypeSolidKind::Boundary);
  selectRecord(7, "point_light");
  const auto checkbox = item("initially-on");
  result = execute(8, Json::array({{{"op", "set_checked"}, {"target", {{"ref", checkbox.at("ref")}}}, {"value", false}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_FALSE(std::get<PrototypePointLight>(*document_->object(document_->selection())).initially_on);
}

class SemanticHouseholdSession : public SemanticSession {
 protected:
  std::string fixture() const override { return "household-interactions"; }
};

class SemanticNarrativeSession : public SemanticSession {
 protected:
  std::string fixture() const override { return "narrative-t4"; }
};

TEST_F(SemanticNarrativeSession, AllowlistedFixtureStartsAndSelectsAppliedEvent) {
  selectRecord(1, "narrative_event");
  const auto event = std::get<NarrativeEventDefinition>(
      *document_->object(document_->selection()));
  EXPECT_EQ(event.id, "neutral-sequence");
  EXPECT_EQ(event.steps.size(), 5U);
  EXPECT_FALSE(document_->dirty());
  EXPECT_EQ(item("narrative-id")["applied_binding"]["field"], "id");
}

TEST_F(SemanticNarrativeSession, FactIdentifierTabCommitsThroughRealInputAndAppliedProjection) {
  const auto add = item("add-fact");
  auto result = execute(1, Json::array({
      {{"op", "activate"}, {"target", {{"ref", add.at("ref")}}}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  const auto selected = document_->selection();
  ASSERT_TRUE(std::holds_alternative<NarrativeFactDefinition>(*document_->object(selected)));
  const auto original = std::get<NarrativeFactDefinition>(*document_->object(selected));
  const auto field = item("narrative-id");
  const Json target{{"ref", field.at("ref")}};
  const auto appliedId = [&](std::string_view expected) {
    return Json{{"op", "assert"}, {"condition", {
        {"source", "app"}, {"projection", "object"}, {"object_ref", field.at("owner")},
        {"field", "id"}, {"predicate", "equals"}, {"expected", expected}}}};
  };
  const auto revision = document_->revision();
  // This pane has one text input followed by a checkbox. Tab must leave the
  // text input instead of wrapping to itself and silently retaining a draft.
  result = execute(2, Json::array({
      {{"op", "edit"}, {"target", target}, {"text", "semantic-done"}, {"commit", "tab"}},
      appliedId("semantic-done")}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(document_->revision(), revision + 1);
  EXPECT_FALSE(item("narrative-id")["state"]["active"].get<bool>());
  EXPECT_EQ(std::get<NarrativeFactDefinition>(*document_->object(selected)).initial_value,
            original.initial_value);

  // A deferred edit must remain a draft across observation and only commit
  // when the separate advertised Tab operation runs.
  result = execute(3, Json::array({
      {{"op", "edit"}, {"target", target}, {"text", "semantic-later"}, {"commit", "none"}},
      appliedId("semantic-done")}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(item("narrative-id")["input"]["text"], "semantic-later");
  EXPECT_EQ(document_->revision(), revision + 1);
  result = execute(4, Json::array({
      {{"op", "commit"}, {"target", target}, {"method", "tab"}},
      appliedId("semantic-later")}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(document_->revision(), revision + 2);
  EXPECT_EQ(item("narrative-id")["ref"], field.at("ref"));
  ASSERT_TRUE(document_->undo());
  EXPECT_EQ(std::get<NarrativeFactDefinition>(*document_->object(selected)).id, "semantic-done");
  ASSERT_TRUE(document_->undo());
  EXPECT_EQ(std::get<NarrativeFactDefinition>(*document_->object(selected)), original);
}

TEST_F(SemanticHouseholdSession, UnicodeMultilineCapacityAndRejectedIdentifierUseRealControls) {
  selectRecord(1, "readable_document");
  const auto selected = document_->selection();
  const auto original = std::get<HouseholdDocumentDefinition>(*document_->object(selected));
  auto text = item("document-title");
  auto result = execute(2, Json::array({{{"op", "edit"}, {"target", {{"ref", text.at("ref")}}},
      {"text", "Записка"}, {"commit", "tab"}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(std::get<HouseholdDocumentDefinition>(*document_->object(selected)).title, "Записка");
  text = item("pages[0]/page-text");
  const auto pages = std::get<HouseholdDocumentDefinition>(*document_->object(selected)).pages;
  result = execute(3, Json::array({{{"op", "edit"}, {"target", {{"ref", text.at("ref")}}}, {"text", "Первая строка\nВторая строка"}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(item("pages[0]/page-text")["input"]["text"], "Первая строка\nВторая строка");
  EXPECT_EQ(std::get<HouseholdDocumentDefinition>(*document_->object(selected)).pages, pages);
  result = execute(4, Json::array({{{"op", "commit"}, {"target", {{"ref", text.at("ref")}}}, {"method", "tab"}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(std::get<HouseholdDocumentDefinition>(*document_->object(selected)).pages.front(), "Первая строка\nВторая строка");
  text = item("document-id");
  result = execute(5, Json::array({{{"op", "edit"}, {"target", {{"ref", text.at("ref")}}}, {"text", "invalid identifier!"}, {"commit", "tab"}}}));
  ASSERT_TRUE(result.at("ok").get<bool>()) << result.dump();
  EXPECT_EQ(std::get<HouseholdDocumentDefinition>(*document_->object(selected)).id, original.id);
  const auto revision = document_->revision();
  result = execute(6, Json::array({{{"op", "edit"}, {"target", {{"ref", text.at("ref")}}},
      {"text", std::string(text.at("capacity_bytes").get<std::size_t>() + 1, 'x')}}}));
  EXPECT_EQ(result["error"]["code"], "limit_exceeded");
  EXPECT_EQ(result["effects"], "none");
  EXPECT_EQ(document_->revision(), revision);
}
}  // namespace
}  // namespace editor_automation
