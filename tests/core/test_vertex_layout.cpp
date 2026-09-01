#include <gtest/gtest.h>

#include <cstddef>

#include "core/render/graphics_pipeline.hpp"
#include "core/render/immutable_mesh_buffer.hpp"
#include "core/render/lighting_resources.hpp"
#include "prototype_level_fixture.hpp"

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
  EXPECT_EQ(attributes[3].format, VK_FORMAT_R32G32_SFLOAT);
  EXPECT_EQ(attributes[3].offset,
            offsetof(PositionColorVertex, texture_coordinates));
  EXPECT_EQ(attributes[4].location, 4U);
  EXPECT_EQ(attributes[4].format, VK_FORMAT_R32_UINT);
  EXPECT_EQ(attributes[4].offset, offsetof(PositionColorVertex, texture_layer));
  EXPECT_EQ(sizeof(PositionColorVertex), sizeof(float) * 8 + 8);
}

TEST(ImmutableMeshBuffer, AcceptsOnlyCompleteDrawableTriangleStreams) {
  EXPECT_FALSE(immutableMeshVertexCountIsValid(0));
  EXPECT_FALSE(immutableMeshVertexCountIsValid(1));
  EXPECT_FALSE(immutableMeshVertexCountIsValid(2));
  EXPECT_TRUE(immutableMeshVertexCountIsValid(3));
  EXPECT_TRUE(immutableMeshVertexCountIsValid(6));
  EXPECT_FALSE(immutableMeshVertexCountIsValid(
      static_cast<std::size_t>(UINT32_MAX) + 1U));
}

TEST(ScenePipeline, SceneDataFitsTheSharedPushConstantRange) {
  static_assert(std::is_standard_layout_v<ScenePushConstant>);
  constexpr VkPushConstantRange range = scenePushConstantRange();
  EXPECT_EQ(range.stageFlags,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  EXPECT_EQ(range.offset, 0U);
  EXPECT_EQ(range.size, sizeof(ScenePushConstant));
  EXPECT_EQ(offsetof(ScenePushConstant, camera), 0U);
  EXPECT_EQ(offsetof(ScenePushConstant, spot_light), sizeof(CameraFrame));
  EXPECT_EQ(sizeof(ScenePushConstant), 128U);
  EXPECT_EQ(sizeof(ScenePushConstant), vulkan_minimum_push_constant_size);
}

TEST(ScenePipeline, PushConstantCarriesCameraAndGenericSpotLight) {
  CameraFrame camera;
  camera.view_projection[12] = 3.5F;
  const SpotLightFrame spot_light{{1.0F, 2.0F, 3.0F, 8.0F},
                                  {0.0F, 0.0F, -1.0F, 0.95F},
                                  {0.7F, 0.8F, 0.9F, 1.2F},
                                  {0.85F, 1.0F, 0.0F, 0.0F}};
  const ScenePushConstant push_constant =
      makeScenePushConstant(camera, spot_light);

  EXPECT_FLOAT_EQ(push_constant.camera.view_projection[12], 3.5F);
  EXPECT_EQ(push_constant.spot_light.position_and_range,
            spot_light.position_and_range);
  EXPECT_EQ(push_constant.spot_light.direction_and_inner_cosine,
            spot_light.direction_and_inner_cosine);
  EXPECT_EQ(push_constant.spot_light.color_and_intensity,
            spot_light.color_and_intensity);
  EXPECT_EQ(push_constant.spot_light.outer_cosine_and_enabled,
            spot_light.outer_cosine_and_enabled);
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
      loadPackagedPrototypeLevel().environmentLight();
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

TEST(SceneLighting, SpotLightFalloffIsConeRangeOrientationAndStateBounded) {
  constexpr float range = 10.0F;
  constexpr float inner = 0.95F;
  constexpr float outer = 0.80F;
  const float distance_falloff = spotLightDistanceFalloff(5.0F, range);
  EXPECT_GT(distance_falloff, 0.0F);
  EXPECT_LT(distance_falloff, 1.0F);
  EXPECT_FLOAT_EQ(spotLightAngularFalloff(inner, inner, outer), 1.0F);
  EXPECT_FLOAT_EQ(spotLightAngularFalloff(outer, inner, outer), 0.0F);
  const float transition = spotLightAngularFalloff(0.875F, inner, outer);
  EXPECT_GT(transition, 0.0F);
  EXPECT_LT(transition, 1.0F);

  EXPECT_FLOAT_EQ(
      spotLightDiffuseFactor(1.0F, 5.0F, range, inner, inner, outer, true),
      distance_falloff);
  EXPECT_FLOAT_EQ(
      spotLightDiffuseFactor(-1.0F, 5.0F, range, inner, inner, outer, true),
      0.0F);
  EXPECT_FLOAT_EQ(
      spotLightDiffuseFactor(1.0F, range, range, inner, inner, outer, true),
      0.0F);
  EXPECT_FLOAT_EQ(
      spotLightDiffuseFactor(1.0F, 5.0F, range, outer, inner, outer, true),
      0.0F);
  EXPECT_FLOAT_EQ(
      spotLightDiffuseFactor(1.0F, 5.0F, range, inner, inner, outer, false),
      0.0F);
  EXPECT_LE(spotLightDiffuseFactor(2.0F, 0.0F, range, 1.0F, inner, outer, true),
            1.0F);
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
