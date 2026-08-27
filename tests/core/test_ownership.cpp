#include <gtest/gtest.h>

#include <cstdlib>
#include <type_traits>

#include "core/engine.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/graphics_pipeline.hpp"
#include "core/render/renderer.hpp"
#include "core/render/vulkan_context.hpp"

TEST(Ownership, RuntimeAndVulkanOwnersAreNonCopyable) {
  static_assert(!std::is_copy_constructible_v<Platform>);
  static_assert(!std::is_copy_constructible_v<Window>);
  static_assert(!std::is_copy_constructible_v<VulkanContext>);
  static_assert(!std::is_copy_constructible_v<GraphicsPipeline>);
  static_assert(!std::is_copy_constructible_v<Renderer>);
  static_assert(!std::is_copy_constructible_v<Engine>);
  SUCCEED();
}

TEST(PlatformFailure, ForcedInitializationFailureIsDeterministic) {
#if defined(_WIN32)
  ASSERT_EQ(_putenv_s("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE", "1"), 0);
#else
  ASSERT_EQ(setenv("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE", "1", 1), 0);
#endif
  EXPECT_TRUE(Platform::forcedInitializationFailureRequested());
#if defined(_WIN32)
  ASSERT_EQ(_putenv_s("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE", ""), 0);
#else
  ASSERT_EQ(unsetenv("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE"), 0);
#endif
  EXPECT_FALSE(Platform::forcedInitializationFailureRequested());
}
