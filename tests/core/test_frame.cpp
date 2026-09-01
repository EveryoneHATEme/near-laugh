#include <gtest/gtest.h>

#include <limits>
#include <type_traits>

#include "core/frame.hpp"

TEST(FrameContract, CameraFrameIsBackendNeutralColumnMajorScalarData) {
  static_assert(std::is_standard_layout_v<CameraFrame>);
  const CameraFrame camera;
  EXPECT_EQ(camera.view_projection.size(), 16U);
  EXPECT_FLOAT_EQ(camera.view_projection[0], 1.0F);
  EXPECT_FLOAT_EQ(camera.view_projection[5], 1.0F);
  EXPECT_FLOAT_EQ(camera.view_projection[10], 1.0F);
  EXPECT_FLOAT_EQ(camera.view_projection[15], 1.0F);
}

TEST(FrameContract, ExposesEveryBackendNeutralOutcome) {
  EXPECT_NE(FrameOutcome::Rendered, FrameOutcome::Skipped);
  EXPECT_NE(FrameOutcome::Rendered, FrameOutcome::Recovered);
  EXPECT_NE(FrameOutcome::Skipped, FrameOutcome::Recovered);
  EXPECT_TRUE(runtimeContinuesAfter(FrameOutcome::Rendered));
  EXPECT_TRUE(runtimeContinuesAfter(FrameOutcome::Skipped));
  EXPECT_TRUE(runtimeContinuesAfter(FrameOutcome::Recovered));
}

TEST(FrameContract, ZeroExtentCannotReachGpuSubmission) {
  EXPECT_FALSE(frameRequestCanSubmit({{0, 600}, false}));
  EXPECT_FALSE(frameRequestCanSubmit({{800, 0}, true}));
  EXPECT_TRUE(frameRequestCanSubmit({{800, 600}, false}));
}

TEST(FrameContract, ValidatesSourceIndependentSpotLightData) {
  static_assert(std::is_standard_layout_v<SpotLightFrame>);
  EXPECT_TRUE(spotLightFrameIsValid({}));

  SpotLightFrame light{{1.0F, 2.0F, 3.0F, 12.0F},
                       {0.0F, 0.0F, -1.0F, 0.95F},
                       {0.8F, 0.9F, 1.0F, 1.25F},
                       {0.85F, 1.0F, 0.0F, 0.0F}};
  EXPECT_TRUE(spotLightFrameIsValid(light));

  light.position_and_range[0] = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(spotLightFrameIsValid(light));
  light.position_and_range[0] = 1.0F;
  light.direction_and_inner_cosine[2] = 0.0F;
  EXPECT_FALSE(spotLightFrameIsValid(light));
  light.direction_and_inner_cosine[2] = -1.0F;
  light.position_and_range[3] = 0.0F;
  EXPECT_FALSE(spotLightFrameIsValid(light));
  light.position_and_range[3] = 12.0F;
  light.color_and_intensity[0] = -0.1F;
  EXPECT_FALSE(spotLightFrameIsValid(light));
  light.color_and_intensity[0] = 0.8F;
  light.color_and_intensity[3] = 0.0F;
  EXPECT_FALSE(spotLightFrameIsValid(light));
  light.color_and_intensity[3] = 1.25F;
  light.outer_cosine_and_enabled[0] = 0.96F;
  EXPECT_FALSE(spotLightFrameIsValid(light));
  light.outer_cosine_and_enabled[0] = 0.85F;
  light.outer_cosine_and_enabled[1] = 0.0F;
  EXPECT_FALSE(spotLightFrameIsValid(light));
}

TEST(RuntimeLoop, StopsBeforeRenderingAfterClose) {
  EXPECT_EQ(decideLoopAction(true, {800, 600}, false).action, LoopAction::Stop);
}

TEST(RuntimeLoop, WaitsForAZeroFramebuffer) {
  EXPECT_EQ(decideLoopAction(false, {0, 600}, false).action,
            LoopAction::WaitForEvents);
}

TEST(RuntimeLoop, CreatesOneExplicitFrameRequest) {
  CameraFrame camera;
  camera.view_projection[12] = 7.0F;
  const SpotLightFrame spot_light{{4.0F, 5.0F, 6.0F, 10.0F},
                                  {1.0F, 0.0F, 0.0F, 0.9F},
                                  {0.5F, 0.6F, 0.7F, 1.1F},
                                  {0.8F, 1.0F, 0.0F, 0.0F}};
  const LoopDecision decision =
      decideLoopAction(false, {800, 600}, true, camera, spot_light);
  EXPECT_EQ(decision.action, LoopAction::Render);
  EXPECT_EQ(decision.frame.framebuffer.width, 800U);
  EXPECT_EQ(decision.frame.framebuffer.height, 600U);
  EXPECT_TRUE(decision.frame.framebuffer_resized);
  EXPECT_FLOAT_EQ(decision.frame.camera.view_projection[12], 7.0F);
  EXPECT_EQ(decision.frame.spot_light.position_and_range,
            spot_light.position_and_range);
  EXPECT_EQ(decision.frame.spot_light.direction_and_inner_cosine,
            spot_light.direction_and_inner_cosine);
  EXPECT_EQ(decision.frame.spot_light.color_and_intensity,
            spot_light.color_and_intensity);
  EXPECT_EQ(decision.frame.spot_light.outer_cosine_and_enabled,
            spot_light.outer_cosine_and_enabled);
}
