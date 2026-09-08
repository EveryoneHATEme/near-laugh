#include "core/development/frame_timings.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <stdexcept>

FrameTimings::FrameTimings() { samples_.reserve(maximum_samples); }

void FrameTimings::beginFrame(Clock::time_point now) {
  if (frame_active_) throw std::logic_error("Timing frame already active");
  if (samples_.size() == maximum_samples)
    throw std::runtime_error("P10 timing sample limit reached");
  if (!start_) start_ = now;
  frame_start_ = now;
  samples_.emplace_back();
  samples_.back().elapsed_seconds = elapsedSeconds(now);
  frame_active_ = true;
}

void FrameTimings::endFrame(Clock::time_point now) {
  if (!frame_active_) throw std::logic_error("No active timing frame");
  auto& row = samples_.back();
  row.interval_ms = std::chrono::duration<double, std::milli>(
                        now - previous_end_.value_or(frame_start_))
                        .count();
  row.cpu_active_ms =
      std::chrono::duration<double, std::milli>(now - frame_start_).count() -
      row.fence_ms - row.acquire_ms - row.present_ms;
  previous_end_ = now;
  frame_active_ = false;
}

FrameTimingSample& FrameTimings::current() {
  if (!frame_active_) throw std::logic_error("No active timing frame");
  return samples_.back();
}
FrameTimingSample& FrameTimings::sample(std::size_t index) {
  return samples_.at(index);
}
std::size_t FrameTimings::currentIndex() const {
  if (!frame_active_) throw std::logic_error("No active timing frame");
  return samples_.size() - 1;
}
double FrameTimings::elapsedSeconds(Clock::time_point now) const {
  return start_ ? std::chrono::duration<double>(now - *start_).count() : 0;
}

void FrameTimings::writeCsv(const std::filesystem::path& path) const {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output.imbue(std::locale::classic());
  output
      << std::fixed << std::setprecision(6)
      << "frame,elapsed_seconds,interval_ms,cpu_active_ms,fence_ms,acquire_ms,"
         "present_ms,gpu_frame_ms,gpu_shadow_ms,submitted,width,height,"
         "action,character_deformation_ms,character_upload_ms\n";
  for (std::size_t i = 0; i < samples_.size(); ++i) {
    const auto& row = samples_[i];
    output << i << ',' << row.elapsed_seconds << ',' << row.interval_ms << ','
           << row.cpu_active_ms << ',' << row.fence_ms << ',' << row.acquire_ms
           << ',' << row.present_ms << ',';
    if (row.gpu_frame_ms) output << *row.gpu_frame_ms;
    output << ',';
    if (row.gpu_shadow_ms) output << *row.gpu_shadow_ms;
    output << ',' << row.submitted << ',' << row.width << ',' << row.height
           << ',' << std::quoted(row.action) << ','
           << row.character_deformation_ms << ',' << row.character_upload_ms
           << '\n';
  }
  output.close();
}

std::optional<double> timestampMilliseconds(
    std::uint64_t begin, std::uint64_t end, unsigned valid_bits,
    double nanoseconds_per_tick, bool begin_available, bool end_available) {
  if (!begin_available || !end_available || valid_bits == 0 ||
      valid_bits > 64 || !std::isfinite(nanoseconds_per_tick) ||
      nanoseconds_per_tick <= 0)
    return std::nullopt;
  const auto mask = valid_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                     : (std::uint64_t{1} << valid_bits) - 1;
  const double milliseconds = static_cast<double>((end - begin) & mask) *
                              nanoseconds_per_tick / 1000000.0;
  return std::isfinite(milliseconds) ? std::optional{milliseconds}
                                     : std::nullopt;
}
