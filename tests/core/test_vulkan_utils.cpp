#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>

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

TEST(SwapchainSelection, RequiresColorAttachmentImageUsage) {
  EXPECT_NO_THROW(requireColorAttachmentSwapchainUsage(
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
  try {
    requireColorAttachmentSwapchainUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    FAIL() << "Expected missing color-attachment usage to fail";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("color-attachment"),
              std::string::npos);
  }
}

TEST(SwapchainSelection, PrefersOpaqueCompositeAlpha) {
  EXPECT_EQ(chooseCompositeAlpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR |
                                 VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR),
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
}

TEST(SwapchainSelection, ChoosesDeterministicSupportedCompositeAlphaFallback) {
  EXPECT_EQ(chooseCompositeAlpha(VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR |
                                 VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR),
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR);
  EXPECT_EQ(chooseCompositeAlpha(VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR |
                                 VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR),
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR);
  EXPECT_EQ(chooseCompositeAlpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR),
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR);
  EXPECT_THROW(chooseCompositeAlpha(0), std::runtime_error);
}
