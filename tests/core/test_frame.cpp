#include <gtest/gtest.h>

#include "core/frame.hpp"

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

TEST(RuntimeLoop, StopsBeforeRenderingAfterClose) {
  EXPECT_EQ(decideLoopAction(true, {800, 600}, false).action,
            LoopAction::Stop);
}

TEST(RuntimeLoop, WaitsForAZeroFramebuffer) {
  EXPECT_EQ(decideLoopAction(false, {0, 600}, false).action,
            LoopAction::WaitForEvents);
}

TEST(RuntimeLoop, CreatesOneExplicitFrameRequest) {
  const LoopDecision decision = decideLoopAction(false, {800, 600}, true);
  EXPECT_EQ(decision.action, LoopAction::Render);
  EXPECT_EQ(decision.frame.framebuffer.width, 800U);
  EXPECT_EQ(decision.frame.framebuffer.height, 600U);
  EXPECT_TRUE(decision.frame.framebuffer_resized);
}
