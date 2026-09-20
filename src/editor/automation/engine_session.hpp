#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

struct ImGuiContext;
struct ImGuiTestContext;
struct ImGuiTestEngine;

namespace editor_automation {

enum class EngineSessionState { NotStarted, Running, Stopped, Expired, Faulted };

struct EngineSessionSnapshot {
  EngineSessionState state{EngineSessionState::NotStarted};
  bool dispatcher_active{};
  std::uint64_t dispatcher_turns{};
  // Last dispatcher frame, or the final frame sampled by stop().
  int frame{};
};

// Calls and snapshots belong to the frame thread. The Test Engine coroutine
// accesses this owner only while that thread is suspended inside ImGui.
// Construct before the ImGui-context owner, call start after context creation,
// and keep an EngineStopGuard after all UI/backend owners. This makes Stop run
// before backend teardown and engine destruction run after ImGui destruction.
class EngineSession {
 public:
  EngineSession() = default;
  ~EngineSession();

  EngineSession(const EngineSession&) = delete;
  EngineSession& operator=(const EngineSession&) = delete;
  EngineSession(EngineSession&&) = delete;
  EngineSession& operator=(EngineSession&&) = delete;

  void start();
  void stop() noexcept;
  using Dispatch = void (*)(void*, ImGuiTestContext*);
  void setDispatcher(Dispatch dispatch, void* owner) noexcept {
    dispatch_ = dispatch;
    dispatch_owner_ = owner;
  }
  [[nodiscard]] ImGuiTestEngine* engine() const noexcept { return engine_; }
  [[nodiscard]] bool running() const noexcept;
  [[nodiscard]] EngineSessionSnapshot snapshot() const noexcept;

 private:
  static void dispatch(ImGuiTestContext* context);
  static void teardown(ImGuiTestContext* context);
  void log(const char* message) noexcept;

  ImGuiTestEngine* engine_{};
  ImGuiContext* context_{};
  EngineSessionSnapshot snapshot_{};
  std::chrono::steady_clock::time_point deadline_{};
  std::size_t logged_bytes_{};
  bool started_{};
  bool stopping_{};
  Dispatch dispatch_{};
  void* dispatch_owner_{};
};

class EngineStopGuard {
 public:
  explicit EngineStopGuard(EngineSession& session) noexcept : session_(session) {}
  ~EngineStopGuard() { session_.stop(); }
  EngineStopGuard(const EngineStopGuard&) = delete;
  EngineStopGuard& operator=(const EngineStopGuard&) = delete;
  EngineStopGuard(EngineStopGuard&&) = delete;
  EngineStopGuard& operator=(EngineStopGuard&&) = delete;

 private:
  EngineSession& session_;
};

}  // namespace editor_automation
