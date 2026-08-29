#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <type_traits>

#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/physics/physics_world.hpp"
#include "core/player/player_controller.hpp"
#include "core/render/depth_attachment.hpp"
#include "core/render/graphics_pipeline.hpp"
#include "core/render/renderer.hpp"
#include "core/render/vulkan_context.hpp"
#include "core/testing/test_controls.hpp"
#include "near_laugh/application.hpp"

TEST(Ownership, RuntimeAndVulkanOwnersAreNonCopyable) {
  static_assert(!std::is_copy_constructible_v<Platform>);
  static_assert(!std::is_copy_constructible_v<Window>);
  static_assert(!std::is_copy_constructible_v<VulkanContext>);
  static_assert(!std::is_copy_constructible_v<DepthAttachment>);
  static_assert(!std::is_move_constructible_v<DepthAttachment>);
  static_assert(!std::is_copy_constructible_v<GraphicsPipeline>);
  static_assert(!std::is_copy_constructible_v<Renderer>);
  static_assert(!std::is_copy_constructible_v<PhysicsWorld>);
  static_assert(!std::is_copy_constructible_v<PlayerController>);
  static_assert(!std::is_copy_constructible_v<near_laugh::Application>);
  static_assert(std::is_constructible_v<Window, Platform&, std::uint32_t,
                                        std::uint32_t, std::string_view>);
  static_assert(!std::is_constructible_v<Window, std::uint32_t, std::uint32_t,
                                         std::string_view>);
  SUCCEED();
}

TEST(PlatformFailure, ForcedInitializationFailureIsDeterministic) {
#if defined(_WIN32)
  ASSERT_EQ(_putenv_s("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE", "1"), 0);
#else
  ASSERT_EQ(setenv("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE", "1", 1), 0);
#endif
  EXPECT_TRUE(forcedPlatformInitializationFailure());
#if defined(_WIN32)
  ASSERT_EQ(_putenv_s("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE", ""), 0);
#else
  ASSERT_EQ(unsetenv("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE"), 0);
#endif
  EXPECT_FALSE(forcedPlatformInitializationFailure());
}
