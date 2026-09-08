#ifndef CORE_DEVELOPMENT_FRAME_TIMINGS_HPP
#define CORE_DEVELOPMENT_FRAME_TIMINGS_HPP

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Opt-in, main-thread P10 measurement storage. No allocation or timing calls
// occur in ordinary runs. Retain raw samples; write only after GPU teardown.
struct FrameTimingSample {
  double elapsed_seconds{};
  double interval_ms{};
  double cpu_active_ms{};
  double character_deformation_ms{};
  double character_upload_ms{};
  double fence_ms{};
  double acquire_ms{};
  double present_ms{};
  std::optional<double> gpu_frame_ms;
  std::optional<double> gpu_shadow_ms;
  bool submitted{};
  std::uint32_t width{}, height{};
  std::string action;
};

class FrameTimings {
 public:
  using Clock = std::chrono::steady_clock;
  static constexpr std::size_t maximum_samples = 100000;
  FrameTimings();
  void beginFrame(Clock::time_point now);
  void endFrame(Clock::time_point now);
  [[nodiscard]] FrameTimingSample& current();
  [[nodiscard]] FrameTimingSample& sample(std::size_t index);
  [[nodiscard]] std::size_t currentIndex() const;
  [[nodiscard]] double elapsedSeconds(Clock::time_point now) const;
  void writeCsv(const std::filesystem::path& path) const;

 private:
  std::vector<FrameTimingSample> samples_;
  std::optional<Clock::time_point> start_, previous_end_;
  Clock::time_point frame_start_{};
  bool frame_active_{};
};

[[nodiscard]] std::optional<double> timestampMilliseconds(
    std::uint64_t begin, std::uint64_t end, unsigned valid_bits,
    double nanoseconds_per_tick, bool begin_available, bool end_available);

#endif
