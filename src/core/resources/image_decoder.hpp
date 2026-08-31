#ifndef CORE_RESOURCES_IMAGE_DECODER_HPP
#define CORE_RESOURCES_IMAGE_DECODER_HPP

#include <cstdint>
#include <filesystem>
#include <vector>

struct DecodedRgbaImage {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::uint8_t> pixels{};
};

[[nodiscard]] DecodedRgbaImage decodePngRgba(
    const std::filesystem::path& path);

#endif
