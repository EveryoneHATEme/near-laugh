#include <gtest/gtest.h>

#include <limits>

#include "core/render/vulkan_utils.hpp"

TEST(VulkanResult, NamesKnownAndUnknownResults) {
  EXPECT_STREQ(vulkanResultName(VK_ERROR_DEVICE_LOST), "VK_ERROR_DEVICE_LOST");
  EXPECT_STREQ(vulkanResultName(static_cast<VkResult>(-987654)),
               "VK_RESULT_UNKNOWN");
}

TEST(QueueFamilies, PrefersOneFamilySupportingBothOperations) {
  const auto selection =
      chooseQueueFamilies({{true, false}, {false, true}, {true, true}});
  ASSERT_TRUE(selection.has_value());
  EXPECT_EQ(selection->graphics, 2U);
  EXPECT_EQ(selection->present, 2U);
}

TEST(QueueFamilies, SupportsDistinctGraphicsAndPresentationFamilies) {
  const auto selection = chooseQueueFamilies({{true, false}, {false, true}});
  ASSERT_TRUE(selection.has_value());
  EXPECT_EQ(selection->graphics, 0U);
  EXPECT_EQ(selection->present, 1U);
}

TEST(QueueFamilies, RejectsMissingRequirement) {
  EXPECT_FALSE(chooseQueueFamilies({{true, false}}).has_value());
}

TEST(SwapchainSelection, PrefersSrgbAndClampsVariableExtent) {
  const VkSurfaceFormatKHR fallback{VK_FORMAT_R8_UNORM,
                                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  const VkSurfaceFormatKHR srgb{VK_FORMAT_B8G8R8A8_SRGB,
                                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  EXPECT_EQ(chooseSurfaceFormat({fallback, srgb}).format, srgb.format);

  VkSurfaceCapabilitiesKHR capabilities{};
  capabilities.currentExtent = {std::numeric_limits<std::uint32_t>::max(),
                                std::numeric_limits<std::uint32_t>::max()};
  capabilities.minImageExtent = {320, 200};
  capabilities.maxImageExtent = {1920, 1080};
  const VkExtent2D extent = chooseSwapchainExtent(capabilities, {2560, 100});
  EXPECT_EQ(extent.width, 1920U);
  EXPECT_EQ(extent.height, 200U);
}
