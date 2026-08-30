#include <gtest/gtest.h>

#include <cstddef>

#include "core/render/graphics_pipeline.hpp"

TEST(SceneVertex, MatchesVulkanPipelineDescription) {
  constexpr VkVertexInputBindingDescription binding =
      sceneVertexBindingDescription();
  constexpr auto attributes = sceneVertexAttributeDescriptions();

  EXPECT_EQ(binding.binding, 0U);
  EXPECT_EQ(binding.stride, sizeof(PositionColorVertex));
  EXPECT_EQ(binding.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
  EXPECT_EQ(attributes[0].location, 0U);
  EXPECT_EQ(attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
  EXPECT_EQ(attributes[0].offset, offsetof(PositionColorVertex, position));
  EXPECT_EQ(attributes[1].location, 1U);
  EXPECT_EQ(attributes[1].format, VK_FORMAT_R8G8B8A8_UNORM);
  EXPECT_EQ(attributes[1].offset, offsetof(PositionColorVertex, color));
  EXPECT_EQ(attributes[2].location, 2U);
  EXPECT_EQ(attributes[2].format, VK_FORMAT_R32G32B32_SFLOAT);
  EXPECT_EQ(attributes[2].offset, offsetof(PositionColorVertex, normal));
  EXPECT_EQ(sizeof(PositionColorVertex), sizeof(float) * 6 + 4);
}

TEST(ScenePipeline, SceneDataFitsTheSharedPushConstantRange) {
  static_assert(std::is_standard_layout_v<ScenePushConstant>);
  constexpr VkPushConstantRange range = scenePushConstantRange();
  EXPECT_EQ(range.stageFlags,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  EXPECT_EQ(range.offset, 0U);
  EXPECT_EQ(range.size, sizeof(ScenePushConstant));
  EXPECT_EQ(offsetof(ScenePushConstant, camera), 0U);
  EXPECT_EQ(offsetof(ScenePushConstant, direction_and_directional_intensity),
            sizeof(CameraFrame));
  EXPECT_EQ(offsetof(ScenePushConstant, ambient_intensity),
            sizeof(CameraFrame) + sizeof(float) * 4);
  EXPECT_LE(sizeof(ScenePushConstant), vulkan_minimum_push_constant_size);
}

TEST(ScenePipeline, PushConstantCarriesCameraAndImmutableLevelLight) {
  CameraFrame camera;
  camera.view_projection[12] = 3.5F;
  const PrototypeEnvironmentLight light = PrototypeLevel{}.environmentLight();
  const ScenePushConstant push_constant = makeScenePushConstant(camera, light);

  EXPECT_FLOAT_EQ(push_constant.camera.view_projection[12], 3.5F);
  EXPECT_FLOAT_EQ(push_constant.direction_and_directional_intensity[0],
                  light.direction_to_light[0]);
  EXPECT_FLOAT_EQ(push_constant.direction_and_directional_intensity[1],
                  light.direction_to_light[1]);
  EXPECT_FLOAT_EQ(push_constant.direction_and_directional_intensity[2],
                  light.direction_to_light[2]);
  EXPECT_FLOAT_EQ(push_constant.direction_and_directional_intensity[3],
                  light.directional_intensity);
  EXPECT_FLOAT_EQ(push_constant.ambient_intensity[0], light.ambient_intensity);
  EXPECT_FLOAT_EQ(push_constant.ambient_intensity[1], 0.0F);
  EXPECT_FLOAT_EQ(push_constant.ambient_intensity[2], 0.0F);
  EXPECT_FLOAT_EQ(push_constant.ambient_intensity[3], 0.0F);
}
