#include "core/resources/image_decoder.hpp"

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "core/resources/shader_provider.hpp"

DecodedRgbaImage decodePngRgba(const std::filesystem::path& path) {
  const std::vector<std::uint8_t> encoded = readBinaryFile(path);
  if (encoded.empty() ||
      encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("PNG asset has an invalid encoded size: " +
                             path.string());
  }

  int width = 0;
  int height = 0;
  int source_channels = 0;
  using StbiPixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
  StbiPixels decoded(stbi_load_from_memory(
                         encoded.data(), static_cast<int>(encoded.size()),
                         &width, &height, &source_channels, STBI_rgb_alpha),
                     &stbi_image_free);
  if (!decoded) {
    const char* reason = stbi_failure_reason();
    throw std::runtime_error("Failed to decode PNG asset " + path.string() +
                             ": " +
                             (reason != nullptr ? reason : "unknown error"));
  }
  if (width <= 0 || height <= 0) {
    throw std::runtime_error("Decoded PNG dimensions are invalid: " +
                             path.string());
  }

  constexpr std::size_t channels = 4;
  const std::size_t decoded_width = static_cast<std::size_t>(width);
  const std::size_t decoded_height = static_cast<std::size_t>(height);
  if (decoded_width > std::numeric_limits<std::size_t>::max() / channels /
                          decoded_height) {
    throw std::runtime_error("Decoded PNG byte count overflows: " +
                             path.string());
  }
  const std::size_t byte_count = decoded_width * decoded_height * channels;
  return {static_cast<std::uint32_t>(width),
          static_cast<std::uint32_t>(height),
          std::vector<std::uint8_t>(decoded.get(),
                                    decoded.get() + byte_count)};
}
