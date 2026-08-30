#include <gtest/gtest.h>

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

TEST(FrameContract, CarriesOnlyBackendNeutralPrototypeSolidMasks) {
  static_assert(std::is_standard_layout_v<PrototypeScenePresentation>);
  const PrototypeScenePresentation presentation{0x12U, 0x24U};
  EXPECT_EQ(presentation.highlighted_solid_mask, 0x12U);
  EXPECT_EQ(presentation.dimmed_solid_mask, 0x24U);
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
  const PrototypeScenePresentation presentation{0x4U, 0x8U};
  const LoopDecision decision =
      decideLoopAction(false, {800, 600}, true, camera, presentation);
  EXPECT_EQ(decision.action, LoopAction::Render);
  EXPECT_EQ(decision.frame.framebuffer.width, 800U);
  EXPECT_EQ(decision.frame.framebuffer.height, 600U);
  EXPECT_TRUE(decision.frame.framebuffer_resized);
  EXPECT_FLOAT_EQ(decision.frame.camera.view_projection[12], 7.0F);
  EXPECT_EQ(decision.frame.scene_presentation.highlighted_solid_mask, 0x4U);
  EXPECT_EQ(decision.frame.scene_presentation.dimmed_solid_mask, 0x8U);
}
