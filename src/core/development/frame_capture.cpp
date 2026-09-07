#include "core/development/frame_capture.hpp"

#include <fstream>
#include <locale>
#include <stdexcept>

std::size_t frameCaptureByteCount(std::uint32_t width, std::uint32_t height) {
  constexpr std::size_t maximum_pixels = 4096ULL * 4096;
  if (!width || !height || width > maximum_pixels / height)
    throw std::invalid_argument("Frame readback requires 1 to 16777216 pixels");
  return std::size_t(width) * height * 4;
}
void FrameCapture::writePpm(const std::filesystem::path& path) const {
  if (requested || rgba.size() != frameCaptureByteCount(width, height))
    throw std::logic_error("Frame capture is not ready");
  if (std::filesystem::exists(path))
    throw std::runtime_error("Capture output already exists");
  std::ofstream output(path, std::ios::binary);
  output.imbue(std::locale::classic());
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (std::size_t i = 0; i < rgba.size(); i += 4)
    output.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
  if (!output) throw std::runtime_error("Cannot write frame capture");
}
