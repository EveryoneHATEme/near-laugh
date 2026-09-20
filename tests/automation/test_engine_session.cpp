#include <gtest/gtest.h>
#include <imgui.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "editor/automation/engine_session.hpp"

namespace editor_automation {
namespace {

struct UiContext {
  UiContext() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {800, 600};
    io.DeltaTime = 1.0F / 60.0F;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  }
  ~UiContext() { ImGui::DestroyContext(); }
};

void frame(float delta = 1.0F / 60.0F) {
  ImGui::GetIO().DeltaTime = delta;
  ImGui::NewFrame();
  ImGui::Begin("Real application workspace");
  ImGui::TextUnformatted("The application owns and draws this window");
  ImGui::End();
  ImGui::Render();
}

TEST(EngineSession, InertOwnerNeedsAContextAndCanStopBeforeStarting) {
  EngineSession session;
  EXPECT_EQ(session.engine(), nullptr);
  EXPECT_FALSE(session.running());
  EXPECT_EQ(session.snapshot().state, EngineSessionState::NotStarted);
  session.stop();
  EXPECT_THROW(session.start(), std::logic_error);
}

TEST(EngineSession, IdleDispatcherPersistsBeyondTheOrdinaryTestWatchdog) {
  EngineSession session;
  UiContext context;
  EngineStopGuard stop(session);
  session.start();
  for (int index = 0; index != 8; ++index) frame();
  const auto before = session.snapshot();
  ASSERT_TRUE(before.dispatcher_active);
  ASSERT_TRUE(session.running());

  // Advance upstream test time past its ordinary 60-second watchdog without
  // waiting in real time. No request-boundary test restart can hide expiry.
  for (int index = 0; index != 75; ++index) frame(1.0F);
  const auto after = session.snapshot();
  EXPECT_EQ(after.state, EngineSessionState::Running);
  EXPECT_TRUE(after.dispatcher_active);
  EXPECT_GT(after.dispatcher_turns, before.dispatcher_turns);
  EXPECT_GT(after.frame, before.frame);
  EXPECT_TRUE(session.running());
}

TEST(EngineSession, StopResumesIdleDispatcherWithoutAnotherFrameAndIsIdempotent) {
  EngineSession session;
  UiContext context;
  EngineStopGuard stop(session);
  session.start();
  for (int index = 0; index != 8; ++index) frame();
  ASSERT_TRUE(session.snapshot().dispatcher_active);
  const int last_frame = session.snapshot().frame;
  session.stop();
  EXPECT_FALSE(session.running());
  EXPECT_FALSE(session.snapshot().dispatcher_active);
  EXPECT_EQ(session.snapshot().state, EngineSessionState::Stopped);
  EXPECT_EQ(session.snapshot().frame, last_frame);
  session.stop();
  EXPECT_EQ(session.snapshot().state, EngineSessionState::Stopped);
  EXPECT_THROW(session.start(), std::logic_error);
}

TEST(EngineSession, PersistentDispatcherStillHasABoundedEngineWatchdog) {
  EngineSession session;
  UiContext context;
  EngineStopGuard stop(session);
  session.start();
  for (int index = 0; index != 8; ++index) frame();
  ASSERT_TRUE(session.snapshot().dispatcher_active);
  frame(3603.0F);
  EXPECT_FALSE(session.running());
  EXPECT_FALSE(session.snapshot().dispatcher_active);
  EXPECT_EQ(session.snapshot().state, EngineSessionState::Faulted);
  session.stop();
  EXPECT_EQ(session.snapshot().state, EngineSessionState::Faulted);
}

TEST(EngineSession, StopBeforeFirstFrameDoesNotRunQueuedWork) {
  EngineSession session;
  UiContext context;
  EngineStopGuard stop(session);
  session.start();
  session.stop();
  EXPECT_EQ(session.snapshot().dispatcher_turns, 0U);
  EXPECT_EQ(session.snapshot().state, EngineSessionState::Stopped);
}

struct ObservedContext {
  ObservedContext(EngineSession& session, std::vector<std::string>& events)
      : session(session), events(events) {}
  ~ObservedContext() {
    EXPECT_FALSE(session.running());
    EXPECT_FALSE(session.snapshot().dispatcher_active);
    events.push_back("context.destroy");
  }
  EngineSession& session;
  std::vector<std::string>& events;
  UiContext context;
};

struct ObservedBackend {
  ~ObservedBackend() {
    EXPECT_FALSE(session.running());
    EXPECT_FALSE(session.snapshot().dispatcher_active);
    EXPECT_NE(ImGui::GetCurrentContext(), nullptr);
    events.push_back("backend.destroy");
  }
  EngineSession& session;
  std::vector<std::string>& events;
};

struct ApplicationComposition {
  ApplicationComposition(std::vector<std::string>& events, bool fail)
      : context(session, events), backend{session, events}, stop(session) {
    session.start();
    for (int index = 0; index != 8; ++index) frame();
    if (fail) throw std::runtime_error("Injected constructor-body failure");
  }
  EngineSession session;
  ObservedContext context;
  ObservedBackend backend;
  EngineStopGuard stop;
};

TEST(EngineSession, CompositionStopsBeforeBackendAndDestroysContextBeforeEngine) {
  std::vector<std::string> events;
  { ApplicationComposition application(events, false); }
  EXPECT_EQ(events, (std::vector<std::string>{"backend.destroy",
                                            "context.destroy"}));
  EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
}

TEST(EngineSession, ConstructorBodyFailureUsesTheSameGuardAndUnwindOrder) {
  std::vector<std::string> events;
  EXPECT_THROW(ApplicationComposition(events, true), std::runtime_error);
  EXPECT_EQ(events, (std::vector<std::string>{"backend.destroy",
                                            "context.destroy"}));
  EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
}

}  // namespace
}  // namespace editor_automation
