#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

#include "core/resources/image_decoder.hpp"

TEST(TextureAssets, FixedPrototypeTexturesAreDistinctOpaqueRgbaTiles) {
  const std::filesystem::path textures =
      std::filesystem::absolute("resources/textures").lexically_normal();
  const std::array<std::filesystem::path, 4> paths = {
      textures / "prototype_floor.png",
      textures / "prototype_boundary.png",
      textures / "prototype_obstacle.png",
      textures / "prototype_shooting_target.png"};

  std::array<std::vector<std::uint8_t>, 4> decoded_pixels;
  for (std::size_t index = 0; index < paths.size(); ++index) {
    const DecodedRgbaImage image = decodePngRgba(paths[index]);
    EXPECT_EQ(image.width, 256U) << paths[index];
    EXPECT_EQ(image.height, 256U) << paths[index];
    EXPECT_EQ(image.pixels.size(), 256U * 256U * 4U) << paths[index];
    for (std::size_t alpha = 3; alpha < image.pixels.size(); alpha += 4) {
      ASSERT_EQ(image.pixels[alpha], 255U) << paths[index] << " at " << alpha;
    }
    decoded_pixels[index] = image.pixels;
  }

  for (std::size_t first = 0; first < decoded_pixels.size(); ++first) {
    for (std::size_t second = first + 1; second < decoded_pixels.size();
         ++second) {
      EXPECT_NE(decoded_pixels[first], decoded_pixels[second]);
    }
  }
}
