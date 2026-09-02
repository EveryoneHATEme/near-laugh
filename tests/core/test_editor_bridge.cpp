#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "editor/editor_glfw_bridge.hpp"

namespace {
std::vector<std::string>* events;
bool context_succeeds;
bool glfw_succeeds;

bool fakeCreateContext() {
  events->push_back("context.create");
  return context_succeeds;
}
bool fakeInitializeGlfw(void*) {
  events->push_back("glfw.create");
  return glfw_succeeds;
}
void fakeShutdownGlfw() noexcept { events->push_back("glfw.destroy"); }
void fakeDestroyContext() noexcept { events->push_back("context.destroy"); }

EditorBridgeOperations fakeOperations() {
  return {fakeCreateContext, fakeInitializeGlfw, fakeShutdownGlfw,
          fakeDestroyContext};
}
}  // namespace

TEST(EditorBridge, SuccessfulStartupShutsDownInReverseOrder) {
  std::vector<std::string> log;
  events = &log;
  context_succeeds = true;
  glfw_succeeds = true;
  {
    EditorBridgeLifetime lifetime(nullptr, fakeOperations());
    EXPECT_EQ(log, (std::vector<std::string>{"context.create", "glfw.create"}));
  }
  EXPECT_EQ(log, (std::vector<std::string>{"context.create", "glfw.create",
                                           "glfw.destroy", "context.destroy"}));
}

TEST(EditorBridge, PartialInitializationCleansOnlyCreatedOwners) {
  std::vector<std::string> log;
  events = &log;
  context_succeeds = true;
  glfw_succeeds = false;
  EXPECT_THROW(EditorBridgeLifetime(nullptr, fakeOperations()),
               std::runtime_error);
  EXPECT_EQ(log, (std::vector<std::string>{"context.create", "glfw.create",
                                           "context.destroy"}));

  log.clear();
  context_succeeds = false;
  EXPECT_THROW(EditorBridgeLifetime(nullptr, fakeOperations()),
               std::runtime_error);
  EXPECT_EQ(log, (std::vector<std::string>{"context.create"}));
}

TEST(EditorBridge, ConcreteOwnersAreNonCopyable) {
  static_assert(!std::is_copy_constructible_v<EditorBridgeLifetime>);
  static_assert(!std::is_copy_constructible_v<EditorGlfwBridge>);
  SUCCEED();
}
