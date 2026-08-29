#ifndef CORE_SIMULATION_FIXED_STEP_HPP
#define CORE_SIMULATION_FIXED_STEP_HPP

#include <chrono>
#include <optional>

struct FixedStepBatch {
  int complete_steps{};
  float interpolation_alpha{};
};

class FixedStepAccumulator {
 public:
  using Clock = std::chrono::steady_clock;

  static constexpr double step_seconds = 1.0 / 60.0;
  static constexpr double maximum_contribution_seconds = 0.1;
  static constexpr int maximum_steps_per_sample = 6;

  [[nodiscard]] FixedStepBatch advance(double elapsed_seconds) noexcept;
  [[nodiscard]] FixedStepBatch sample(Clock::time_point now) noexcept;
  void reset() noexcept;

  [[nodiscard]] double remainderSeconds() const noexcept { return remainder_; }

 private:
  double remainder_{};
  std::optional<Clock::time_point> previous_sample_{};
};

#endif
