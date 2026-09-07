#include <gtest/gtest.h>

#include <limits>

#include "core/development/frame_capture.hpp"
#include "core/development/frame_timings.hpp"

TEST(FrameCapture, ByteCountRejectsZeroAndOverflowBeforeAllocation) {
  EXPECT_EQ(frameCaptureByteCount(1920, 1080), 8294400U);
  EXPECT_EQ(frameCaptureByteCount(4096, 4096), 67108864U);
  EXPECT_THROW((void)frameCaptureByteCount(0, 1), std::invalid_argument);
  EXPECT_THROW((void)frameCaptureByteCount(1, 0), std::invalid_argument);
  EXPECT_THROW((void)frameCaptureByteCount(4097, 4096), std::invalid_argument);
  EXPECT_THROW((void)frameCaptureByteCount(UINT32_MAX, UINT32_MAX),
               std::invalid_argument);
}

TEST(FrameTimings, TimestampPeriodAndValidBitsHandleCounterWrap) {
  EXPECT_DOUBLE_EQ(*timestampMilliseconds(250, 4, 8, 1000, true, true), .010);
  EXPECT_DOUBLE_EQ(*timestampMilliseconds(0x12fa, 0xab04, 8, 1000, true, true),
                   .010);
  EXPECT_DOUBLE_EQ(
      *timestampMilliseconds(std::numeric_limits<std::uint64_t>::max() - 4, 5,
                             64, 1000, true, true),
      .010);
  EXPECT_DOUBLE_EQ(*timestampMilliseconds(10, 20, 64, 2.5, true, true),
                   .000025);
}
TEST(FrameTimings, UnavailableTimestampIsNeverReportedAsZeroGpuWork) {
  EXPECT_FALSE(timestampMilliseconds(0, 0, 0, 1, true, true));
  EXPECT_FALSE(timestampMilliseconds(0, 0, 65, 1, true, true));
  EXPECT_FALSE(timestampMilliseconds(0, 0, 64, 0, true, true));
  EXPECT_FALSE(timestampMilliseconds(0, 0, 64, -1, true, true));
  EXPECT_FALSE(timestampMilliseconds(
      0, 0, 64, std::numeric_limits<double>::quiet_NaN(), true, true));
  EXPECT_FALSE(timestampMilliseconds(0, 0, 64, 1, false, true));
  EXPECT_FALSE(timestampMilliseconds(0, 0, 64, 1, true, false));
}
TEST(FrameTimings, CpuActiveExcludesWaitsAndGpuReadbackUpdatesOriginalSample) {
  FrameTimings timings;
  const auto start = FrameTimings::Clock::time_point{};
  timings.beginFrame(start);
  timings.current().fence_ms = 2;
  timings.current().acquire_ms = 3;
  timings.current().present_ms = 4;
  timings.endFrame(start + std::chrono::milliseconds(16));
  timings.beginFrame(start + std::chrono::milliseconds(17));
  timings.sample(0).gpu_frame_ms = 5;
  timings.endFrame(start + std::chrono::milliseconds(32));
  EXPECT_DOUBLE_EQ(timings.sample(0).cpu_active_ms, 7);
  EXPECT_DOUBLE_EQ(timings.sample(0).interval_ms, 16);
  EXPECT_DOUBLE_EQ(*timings.sample(0).gpu_frame_ms, 5);
  EXPECT_FALSE(timings.sample(1).gpu_frame_ms);
  EXPECT_DOUBLE_EQ(timings.sample(1).interval_ms, 16);
  EXPECT_DOUBLE_EQ(timings.sample(1).cpu_active_ms, 15);
  EXPECT_THROW((void)timings.current(), std::logic_error);
}
TEST(FrameTimings, StorageRefusesUnboundedCapture) {
  FrameTimings timings;
  const auto now = FrameTimings::Clock::time_point{};
  for (std::size_t i = 0; i < FrameTimings::maximum_samples; ++i) {
    timings.beginFrame(now);
    timings.endFrame(now);
  }
  EXPECT_THROW(timings.beginFrame(now), std::runtime_error);
}
