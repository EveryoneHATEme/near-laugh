// Real bridge/application boundary test. Calls the owned window's callbacks
// directly; never sends OS keyboard/mouse input or changes the host clipboard.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>

#include "editor/editor_application.hpp"
#include "launcher/executable_path.hpp"

struct EditorAutomationInputProbe {
  static auto camera(const EditorApplication& app) {
    const auto p = app.camera_.position();
    return std::array{p.x, p.y, p.z, app.camera_.yawDegrees(), app.camera_.pitchDegrees()};
  }
  static auto revision(const EditorApplication& app) { return app.document_.revision(); }
  static const auto& input(const EditorApplication& app) { return app.window_.input(); }
  static bool captured(const EditorApplication& app) { return app.window_.cursorCaptured(); }
  static void close(EditorApplication& app) { app.window_.requestClose(); }
};

namespace {
using editor_automation::Json;
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
std::string utf8(const std::filesystem::path& path) {
  const auto bytes = path.u8string(); return {bytes.begin(), bytes.end()};
}
struct Temporary {
  Temporary() : path(std::filesystem::temp_directory_path() /
      ("near-laugh-native-input-" + std::to_string(GetCurrentProcessId()) + "-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
    std::filesystem::create_directory(path);
  }
  ~Temporary() { std::error_code error; std::filesystem::remove_all(path, error); }
  std::filesystem::path path;
};
struct Pipes {
  Pipes() {
    HANDLE input{}, output{};
    require(CreatePipe(&input, &write, nullptr, 0) != 0, "Create input pipe");
    require(CreatePipe(&read, &output, nullptr, 0) != 0, "Create output pipe");
    const auto old_input = GetStdHandle(STD_INPUT_HANDLE), old_output = GetStdHandle(STD_OUTPUT_HANDLE);
    SetStdHandle(STD_INPUT_HANDLE, input); SetStdHandle(STD_OUTPUT_HANDLE, output);
    try { channel = std::make_unique<editor_automation::Channel>(); }
    catch (...) {
      SetStdHandle(STD_INPUT_HANDLE, old_input); SetStdHandle(STD_OUTPUT_HANDLE, old_output);
      CloseHandle(input); CloseHandle(output); throw;
    }
    SetStdHandle(STD_INPUT_HANDLE, old_input); SetStdHandle(STD_OUTPUT_HANDLE, old_output);
    CloseHandle(input); CloseHandle(output);
  }
  ~Pipes() { channel.reset(); CloseHandle(write); CloseHandle(read); }
  void send(const char* tool, Json request) {
    request["protocol_version"] = 1; request["session_id"] = "native-input";
    const auto data = editor_automation::encodeMessage({{"kind", "call"}, {"tool", tool}, {"arguments", request}});
    DWORD written{};
    require(WriteFile(write, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
            written == data.size(), "Write request");
  }
  void drain() {
    DWORD count{};
    while (PeekNamedPipe(read, nullptr, 0, nullptr, &count, nullptr) && count) {
      std::string bytes(std::min<DWORD>(count, 4096), '\0'); DWORD received{};
      require(ReadFile(read, bytes.data(), static_cast<DWORD>(bytes.size()), &received, nullptr) != 0, "Read result");
      bytes.resize(received);
      for (const auto& message : decoder.feed(bytes)) {
        const auto& result = message.at("result");
        editor_automation::validateResult(message.at("tool").get<std::string>(), result);
        results[result.at("request_id").get<std::string>()] = result;
      }
    }
  }
  Json wait(EditorApplication& app, const char* id) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!results.contains(id) && std::chrono::steady_clock::now() < until) {
      require(app.tick(), "Application stopped before expected result"); drain();
    }
    require(results.contains(id), "Result deadline expired");
    return results.at(id);
  }
  HANDLE write{}, read{};
  std::unique_ptr<editor_automation::Channel> channel;
  editor_automation::JsonLineDecoder decoder;
  std::map<std::string, Json> results;
};

void physicalCallbacks(EditorApplication& app, Pipes& pipes) {
  auto* window = static_cast<GLFWwindow*>(ImGui::GetMainViewport()->PlatformHandle);
  require(window != nullptr, "Bridge has no owned GLFW window");
  const auto key = glfwSetKeyCallback(window, nullptr); glfwSetKeyCallback(window, key);
  const auto mouse = glfwSetMouseButtonCallback(window, nullptr); glfwSetMouseButtonCallback(window, mouse);
  const auto cursor = glfwSetCursorPosCallback(window, nullptr); glfwSetCursorPosCallback(window, cursor);
  const auto character = glfwSetCharCallback(window, nullptr); glfwSetCharCallback(window, character);
  const auto scroll = glfwSetScrollCallback(window, nullptr); glfwSetScrollCallback(window, scroll);
  require(key && mouse && cursor && !character && !scroll, "Unexpected ImGui physical input callbacks installed");
  const auto camera = EditorAutomationInputProbe::camera(app);
  const auto revision = EditorAutomationInputProbe::revision(app);
  const auto clipboard = GetClipboardSequenceNumber();
  auto& platform = ImGui::GetPlatformIO();
  platform.Platform_SetClipboardTextFn(ImGui::GetCurrentContext(), "session-only clipboard");
  require(std::string(platform.Platform_GetClipboardTextFn(ImGui::GetCurrentContext())) == "session-only clipboard",
          "Session clipboard did not round trip");
  key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
  key(window, GLFW_KEY_LEFT_CONTROL, 0, GLFW_PRESS, GLFW_MOD_CONTROL);
  mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  cursor(window, 100, 100); cursor(window, 400, 300);
  require(EditorAutomationInputProbe::input(app).isKeyDown(PhysicalKey::W), "Physical callback fixture was not delivered");
  for (int i = 0; i != 8; ++i) {
    require(app.tick(), "Physical callback closed application"); pipes.drain();
    require(!ImGui::IsKeyDown(ImGuiKey_W) && !ImGui::GetIO().KeyCtrl &&
            !ImGui::GetIO().MouseDown[0] && !ImGui::GetIO().MouseDown[1], "Physical input reached ImGui");
    require(EditorAutomationInputProbe::camera(app) == camera, "Physical input changed camera");
    require(EditorAutomationInputProbe::revision(app) == revision, "Physical input changed document");
    require(!EditorAutomationInputProbe::captured(app), "Physical input captured OS cursor");
  }
  key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
  key(window, GLFW_KEY_LEFT_CONTROL, 0, GLFW_RELEASE, 0);
  mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, 0);
  mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
  require(GetClipboardSequenceNumber() == clipboard, "Host clipboard changed during session clipboard test");
  require((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) != 0 &&
          !ImGui::GetIO().ConfigNavMoveSetMousePos && !ImGui::GetIO().WantSetMousePos,
          "OS cursor isolation is disabled");
}
}  // namespace

int main() {
  try {
    Temporary temporary;
    const auto resources = launcher::executableResourceRoot();
    const auto input = temporary.path / "input.level.json";
    std::filesystem::copy_file(resources / "levels/apartment-stairs.level.json", input);
    Pipes pipes;
    Json slots = Json::array();
    for (int i = 0; i != 9; ++i) {
      const auto name = i == 0 ? std::string("input") : "output-" + std::to_string(i);
      slots.push_back({{"slot", name}, {"path", utf8(temporary.path / (name + ".level.json"))}, {"writable", true}});
    }
    editor_automation::SessionController session(*pipes.channel, {
        {"kind", "start"}, {"arguments", {{"protocol_version", 1}, {"request_id", "start"},
            {"op", "start"}, {"fixture", "apartment-stairs"}, {"environment", "authorized-native-probe"}}},
        {"session_id", "native-input"}, {"root", utf8(temporary.path)}, {"resource_root", utf8(resources)},
        {"input_path", utf8(input)}, {"file_slots", slots}});
    ValidationDiagnostics diagnostics;
    {
      EditorApplication app(resources, input, diagnostics, nullptr, &session);
      require(pipes.wait(app, "start").at("ok").get<bool>(), "Session start failed");
      physicalCallbacks(app, pipes);
      pipes.send("ui_execute", {{"request_id", "1"}, {"steps", {{{"op", "wait_until"},
          {"timeout_ms", 30000}, {"condition", {{"source", "app"}, {"projection", "document"},
              {"field", "dirty"}, {"predicate", "equals"}, {"expected", true}}}}}}});
      for (int i = 0; i != 8; ++i) { require(app.tick(), "Batch startup frame stopped"); pipes.drain(); }
      pipes.send("ui_session", {{"request_id", "status"}, {"op", "status"}});
      require(pipes.wait(app, "status").at("state") == "executing", "Batch did not start");
      physicalCallbacks(app, pipes);
      pipes.send("ui_session", {{"request_id", "cancel"}, {"op", "cancel"}, {"active_request_id", "1"}});
      const auto result = pipes.wait(app, "1");
      require(!result.at("ok").get<bool>() && result.at("cleanup") == "released", "Cancel failed to release input");
      EditorAutomationInputProbe::close(app);
      require(!app.tick(), "Owned window close event was not handled");
    }
    require(ImGui::GetCurrentContext() == nullptr, "ImGui context survived application");
    require(diagnostics.errorCount() == 0, "Vulkan validation error during final teardown");
    std::cout << "PASS: physical callback isolation during idle/running batches; session clipboard; owned close; final Vulkan errors=0\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
