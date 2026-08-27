#include <gtest/gtest.h>

#include <cstddef>

#include "core/render/graphics_pipeline.hpp"

TEST(TriangleVertex, MatchesVulkanPipelineDescription) {
  constexpr VkVertexInputBindingDescription binding =
      triangleVertexBindingDescription();
  constexpr auto attributes = triangleVertexAttributeDescriptions();

  EXPECT_EQ(binding.binding, 0U);
  EXPECT_EQ(binding.stride, sizeof(PositionColorVertex));
  EXPECT_EQ(binding.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
  EXPECT_EQ(attributes[0].location, 0U);
  EXPECT_EQ(attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
  EXPECT_EQ(attributes[0].offset, offsetof(PositionColorVertex, position));
  EXPECT_EQ(attributes[1].location, 1U);
  EXPECT_EQ(attributes[1].format, VK_FORMAT_R8G8B8A8_UNORM);
  EXPECT_EQ(attributes[1].offset, offsetof(PositionColorVertex, color));
}
