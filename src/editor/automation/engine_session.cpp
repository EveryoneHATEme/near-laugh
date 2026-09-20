#include "editor/automation/engine_session.hpp"

#include <imgui.h>
#include <imgui_te_context.h>
#include <imgui_te_engine.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <stdexcept>

namespace editor_automation {
namespace {
constexpr auto maximum_session = std::chrono::minutes(60);
constexpr std::size_t maximum_log_bytes = 64 * 1024;
constexpr std::size_t maximum_log_message = 2048;
}  // namespace

EngineSession::~EngineSession() {
  assert(!started_ && "EngineStopGuard must run before ImGui destruction");
  assert((!context_ || ImGui::GetCurrentContext() != context_) &&
         "Destroy the ImGui context before its Test Engine owner");
  if (engine_) ImGuiTestEngine_DestroyContext(engine_);
}

void EngineSession::start() {
  if (engine_ || snapshot_.state != EngineSessionState::NotStarted)
    throw std::logic_error("An automation engine session cannot be restarted");
  context_ = ImGui::GetCurrentContext();
  if (!context_)
    throw std::logic_error("An automation engine session needs an ImGui context");

  try {
    engine_ = ImGuiTestEngine_CreateContext();
    auto& io = ImGuiTestEngine_GetIO(engine_);
    io.ConfigSavedSettings = false;
    io.ConfigCaptureEnabled = false;
    io.ConfigCaptureOnError = false;
    io.ConfigBreakOnError = false;
    io.ConfigKeepGuiFunc = false;
    io.ConfigRestoreFocusAfterTests = false;
    io.ConfigLogToTTY = false;
    io.ConfigLogToDebugger = false;
    io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Error;
    io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Error;
    io.ConfigLogToFuncUserData = this;
    io.ConfigLogToFunc = [](ImGuiTestEngine*, ImGuiTestContext*,
                            ImGuiTestVerboseLevel, const char* message,
                            void* user_data) {
      static_cast<EngineSession*>(user_data)->log(message);
    };
    // These cover the whole persistent dispatcher. Operation deadlines are
    // independent; the ordinary engine 60-second test watchdog is too short.
    io.ConfigWatchdogWarning = 3600.0F;
    io.ConfigWatchdogKillTest = 3602.0F;
    // Keep upstream's process-exit watchdog disabled: the session host owns
    // bounded process termination and its resulting evidence.
    io.ExportResultsFilename = nullptr;

    ImGuiTest* test = ImGuiTestEngine_RegisterTest(
        engine_, "editor", "semantic automation session");
    test->UserData = this;
    test->TestFunc = dispatch;
    test->TeardownFunc = teardown;
    test->GuiFunc = nullptr;

    ImGuiTestEngine_Start(engine_, context_);
    started_ = true;
    deadline_ = std::chrono::steady_clock::now() + maximum_session;
    snapshot_.state = EngineSessionState::Running;
    ImGuiTestEngine_QueueTest(engine_, test);
  } catch (...) {
    snapshot_.state = EngineSessionState::Faulted;
    stop();
    throw;
  }
}

void EngineSession::stop() noexcept {
  if (!started_) return;
  stopping_ = true;
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGui::SetCurrentContext(context_);
  snapshot_.frame = ImGui::GetFrameCount();
  // Stop alone sets engine Abort, not the running TestFunc's Abort flag.
  // Setting both lets the idle dispatcher return without another UI frame.
  ImGuiTestEngine_AbortCurrentTest(engine_);
  ImGuiTestEngine_Stop(engine_);
  started_ = false;
  snapshot_.dispatcher_active = false;
  if (snapshot_.state == EngineSessionState::Running)
    snapshot_.state = EngineSessionState::Stopped;

  // This is terminal cleanup, not evidence that a release frame completed.
  // Active-request cleanup must release through the engine before this point.
  auto& io = ImGui::GetIO();
  io.ClearEventsQueue();
  io.ClearInputKeys();
  io.ClearInputMouse();
  ImGui::SetCurrentContext(previous);
}

bool EngineSession::running() const noexcept {
  return started_ && snapshot_.state == EngineSessionState::Running;
}

EngineSessionSnapshot EngineSession::snapshot() const noexcept {
  return snapshot_;
}

void EngineSession::dispatch(ImGuiTestContext* context) {
  auto& session = *static_cast<EngineSession*>(context->Test->UserData);
  session.snapshot_.dispatcher_active = true;
  while (!session.stopping_ && !context->IsError()) {
    session.snapshot_.frame = ImGui::GetFrameCount();
    if (std::chrono::steady_clock::now() >= session.deadline_) {
      session.snapshot_.state = EngineSessionState::Expired;
      break;
    }
    ++session.snapshot_.dispatcher_turns;
    if (session.dispatch_) session.dispatch_(session.dispatch_owner_, context);
    context->Yield();
  }
  if (context->IsError() && !session.stopping_)
    session.snapshot_.state = EngineSessionState::Faulted;
}

void EngineSession::teardown(ImGuiTestContext* context) {
  auto& session = *static_cast<EngineSession*>(context->Test->UserData);
  session.snapshot_.dispatcher_active = false;
  // Upstream temporarily resets Output.Status before invoking TeardownFunc.
  if ((context->IsError() || context->ErrorCounter != 0) && !session.stopping_)
    session.snapshot_.state = EngineSessionState::Faulted;
}

void EngineSession::log(const char* message) noexcept {
  if (!message || logged_bytes_ >= maximum_log_bytes) return;
  const std::size_t budget =
      std::min(maximum_log_message, maximum_log_bytes - logged_bytes_);
  std::size_t length{};
  while (length < budget && message[length]) ++length;
  logged_bytes_ += length;
  static_cast<void>(std::fwrite(message, 1, length, stderr));
}

}  // namespace editor_automation
