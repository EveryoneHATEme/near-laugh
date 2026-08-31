#include <gtest/gtest.h>

#include <cstddef>

#include "core/render/graphics_pipeline.hpp"
#include "core/render/lighting_resources.hpp"

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
  EXPECT_EQ(attributes[3].location, 3U);
  EXPECT_EQ(attributes[3].format, VK_FORMAT_R32_UINT);
  EXPECT_EQ(attributes[3].offset, offsetof(PositionColorVertex, solid_mask));
  EXPECT_EQ(attributes[4].location, 4U);
  EXPECT_EQ(attributes[4].format, VK_FORMAT_R32G32_SFLOAT);
  EXPECT_EQ(attributes[4].offset,
            offsetof(PositionColorVertex, texture_coordinates));
  EXPECT_EQ(attributes[5].location, 5U);
  EXPECT_EQ(attributes[5].format, VK_FORMAT_R32_UINT);
  EXPECT_EQ(attributes[5].offset, offsetof(PositionColorVertex, texture_layer));
  EXPECT_EQ(sizeof(PositionColorVertex), sizeof(float) * 8 + 12);
}

TEST(ScenePipeline, SceneDataFitsTheSharedPushConstantRange) {
  static_assert(std::is_standard_layout_v<ScenePushConstant>);
  constexpr VkPushConstantRange range = scenePushConstantRange();
  EXPECT_EQ(range.stageFlags,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  EXPECT_EQ(range.offset, 0U);
  EXPECT_EQ(range.size, sizeof(ScenePushConstant));
  EXPECT_EQ(offsetof(ScenePushConstant, camera), 0U);
  EXPECT_EQ(offsetof(ScenePushConstant, presentation_masks),
            sizeof(CameraFrame));
  EXPECT_EQ(sizeof(ScenePushConstant), 80U);
  EXPECT_LE(sizeof(ScenePushConstant), vulkan_minimum_push_constant_size);
}

TEST(ScenePipeline, PushConstantCarriesOnlyCameraAndPresentation) {
  CameraFrame camera;
  camera.view_projection[12] = 3.5F;
  const PrototypeScenePresentation presentation{0x12U, 0x24U};
  const ScenePushConstant push_constant =
      makeScenePushConstant(camera, presentation);

  EXPECT_FLOAT_EQ(push_constant.camera.view_projection[12], 3.5F);
  EXPECT_EQ(push_constant.presentation_masks[0], 0x12U);
  EXPECT_EQ(push_constant.presentation_masks[1], 0x24U);
  EXPECT_EQ(push_constant.presentation_masks[2], 0U);
  EXPECT_EQ(push_constant.presentation_masks[3], 0U);
}

TEST(SceneLighting, MatchesStd140UploadAndDescriptorContract) {
  static_assert(prototype_point_light_count == 2U);
  EXPECT_EQ(alignof(PrototypePointLightUpload), 16U);
  EXPECT_EQ(sizeof(PrototypePointLightUpload), 32U);
  EXPECT_EQ(offsetof(PrototypePointLightUpload, position_and_radius), 0U);
  EXPECT_EQ(offsetof(PrototypePointLightUpload, color_and_intensity), 16U);
  EXPECT_EQ(alignof(PrototypeLightingUpload), 16U);
  EXPECT_EQ(offsetof(PrototypeLightingUpload, point_lights), 0U);
  EXPECT_EQ(offsetof(PrototypeLightingUpload, ambient_intensity), 64U);
  EXPECT_EQ(sizeof(PrototypeLightingUpload), 80U);

  constexpr VkDescriptorSetLayoutBinding binding =
      prototypeLightingDescriptorBinding();
  EXPECT_EQ(binding.binding, 0U);
  EXPECT_EQ(binding.descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  EXPECT_EQ(binding.descriptorCount, 1U);
  EXPECT_EQ(binding.stageFlags, VK_SHADER_STAGE_FRAGMENT_BIT);
  EXPECT_EQ(binding.pImmutableSamplers, nullptr);

  const PrototypeEnvironmentLight environment =
      PrototypeLevel{}.environmentLight();
  const PrototypeLightingUpload upload =
      makePrototypeLightingUpload(environment);
  for (std::size_t index = 0; index < prototype_point_light_count; ++index) {
    EXPECT_FLOAT_EQ(upload.point_lights[index].position_and_radius[0],
                    environment.point_lights[index].position.x);
    EXPECT_FLOAT_EQ(upload.point_lights[index].position_and_radius[1],
                    environment.point_lights[index].position.y);
    EXPECT_FLOAT_EQ(upload.point_lights[index].position_and_radius[2],
                    environment.point_lights[index].position.z);
    EXPECT_FLOAT_EQ(upload.point_lights[index].position_and_radius[3],
                    environment.point_lights[index].radius);
    EXPECT_EQ(
        upload.point_lights[index].color_and_intensity,
        (std::array<float, 4>{environment.point_lights[index].color[0],
                              environment.point_lights[index].color[1],
                              environment.point_lights[index].color[2],
                              environment.point_lights[index].intensity}));
  }
  EXPECT_FLOAT_EQ(upload.ambient_intensity[0], environment.ambient_intensity);
  EXPECT_FLOAT_EQ(upload.ambient_intensity[1], 0.0F);
  EXPECT_FLOAT_EQ(upload.ambient_intensity[2], 0.0F);
  EXPECT_FLOAT_EQ(upload.ambient_intensity[3], 0.0F);
}

TEST(SceneLighting, RadiusBoundedFalloffIsExactAndBounded) {
  constexpr float radius = 4.0F;
  EXPECT_FLOAT_EQ(prototypePointLightFalloff(0.0F, radius), 1.0F);
  EXPECT_GT(prototypePointLightFalloff(radius * 0.5F, radius), 0.0F);
  EXPECT_LT(prototypePointLightFalloff(radius * 0.5F, radius), 1.0F);
  EXPECT_FLOAT_EQ(prototypePointLightFalloff(radius, radius), 0.0F);
  EXPECT_FLOAT_EQ(prototypePointLightFalloff(radius + 1.0F, radius), 0.0F);
}

TEST(ScenePipeline, ReservesTwoImmutableDescriptorSets) {
  static_assert(scene_texture_descriptor_set == 0U);
  static_assert(scene_lighting_descriptor_set == 1U);
  static_assert(scene_descriptor_set_count == 2U);
  const auto layouts =
      sceneDescriptorSetLayouts(VK_NULL_HANDLE, VK_NULL_HANDLE);
  const auto sets = sceneDescriptorSets(VK_NULL_HANDLE, VK_NULL_HANDLE);
  EXPECT_EQ(layouts.size(), 2U);
  EXPECT_EQ(sets.size(), 2U);
}
