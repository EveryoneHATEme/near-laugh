#include "core/simulation/fixed_step.hpp"

#include <algorithm>
#include <cmath>

FixedStepBatch FixedStepAccumulator::advance(double elapsed_seconds) noexcept {
  const double contribution =
      std::isfinite(elapsed_seconds) && elapsed_seconds > 0.0
          ? std::min(elapsed_seconds, maximum_contribution_seconds)
          : 0.0;
  remainder_ += contribution;
  int complete_steps =
      static_cast<int>(std::floor(remainder_ / step_seconds + 1.0e-9));
  complete_steps = std::min(complete_steps, maximum_steps_per_sample);
  remainder_ -= static_cast<double>(complete_steps) * step_seconds;
  remainder_ = std::clamp(remainder_, 0.0, step_seconds);
  return {complete_steps,
          static_cast<float>(remainder_ / step_seconds)};
}

FixedStepBatch FixedStepAccumulator::sample(Clock::time_point now) noexcept {
  if (!previous_sample_) {
    previous_sample_ = now;
    return advance(0.0);
  }
  const double elapsed =
      std::chrono::duration<double>(now - *previous_sample_).count();
  previous_sample_ = now;
  return advance(elapsed);
}

void FixedStepAccumulator::reset() noexcept {
  remainder_ = 0.0;
  previous_sample_.reset();
}
