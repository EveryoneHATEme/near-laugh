#ifndef CORE_DEVELOPMENT_FRAME_CAPTURE_HPP
#define CORE_DEVELOPMENT_FRAME_CAPTURE_HPP
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

// Opt-in fixed-view validation. Capture frames wait for their GPU copy and
// must not be included in performance measurements.
struct FrameCapture {
  bool requested{};
  std::uint32_t width{}, height{};
  std::vector<std::uint8_t> rgba;
  void writePpm(const std::filesystem::path& path) const;
};
[[nodiscard]] std::size_t frameCaptureByteCount(std::uint32_t width,
                                                std::uint32_t height);
#endif
