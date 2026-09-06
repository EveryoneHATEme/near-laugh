#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

#include "core/render/scene_material.hpp"
#include "core/resources/image_decoder.hpp"
#include "core/resources/shader_provider.hpp"

TEST(TextureAssets, FixedPrototypeTexturesAreDistinctOpaqueRgbaTiles) {
  const std::filesystem::path textures =
      std::filesystem::absolute("resources/textures").lexically_normal();
  const std::array<std::filesystem::path, 3> paths = {
      textures / "prototype_floor.png", textures / "prototype_boundary.png",
      textures / "prototype_obstacle.png"};

  std::array<std::vector<std::uint8_t>, 3> decoded_pixels;
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

TEST(TextureAssets, SelectedStructuralTexturesKeepTheirOwnDimensionsAndPixels) {
  const auto floor =
      decodePngRgba("resources/textures/apartment_wood_floor.png");
  const auto wall = decodePngRgba("resources/textures/apartment_wallpaper.png");
  EXPECT_EQ(floor.width, 128U);
  EXPECT_EQ(floor.height, 128U);
  EXPECT_EQ(wall.width, 128U);
  EXPECT_EQ(wall.height, 128U);
  EXPECT_NE(floor.pixels, wall.pixels);
}

TEST(TextureAssets, EmbeddedDecodeMatchesFileAndRejectsBadOrOversizedImages) {
  const auto path =
      std::filesystem::path{"resources/textures/apartment_wood_floor.png"};
  const auto bytes = readBinaryFile(path);
  const auto from_file = decodePngRgba(path);
  const auto embedded = decodePngRgba(bytes, "selected embedded floor");
  EXPECT_EQ(embedded.pixels, from_file.pixels);
  EXPECT_THROW(static_cast<void>(decodePngRgba(bytes, "bounded", 64)),
               std::runtime_error);
  const std::vector<std::uint8_t> malformed{1, 2, 3, 4};
  EXPECT_THROW(static_cast<void>(decodePngRgba(malformed, "malformed")),
               std::runtime_error);
  EXPECT_THROW(static_cast<void>(decodePngRgba({}, "empty")),
               std::runtime_error);
  auto oversized = bytes;
  // The PNG IHDR width is big endian; reject before decoding or allocating
  // pixels.
  oversized[16] = 0x7F;
  oversized[17] = 0xFF;
  oversized[18] = 0xFF;
  oversized[19] = 0xFF;
  EXPECT_THROW(static_cast<void>(decodePngRgba(oversized, "oversized")),
               std::runtime_error);
}

TEST(SceneMaterial, MaskUsesSampleTimesFactorAndOpaqueIgnoresAlpha) {
  SceneMaterialData material;
  material.alpha_cutoff = 0.5F;
  EXPECT_TRUE(materialCoversSample(material, 0.0F));
  material.base_color_factor[3] = 0;
  EXPECT_TRUE(materialCoversSample(material, 0.0F));
  material.alpha_mask = true;
  EXPECT_FALSE(materialCoversSample(material, 1.0F));
  material.base_color_factor[3] = 0.5F;
  EXPECT_TRUE(materialCoversSample(material, 1.0F));
  EXPECT_FALSE(materialCoversSample(material, 0.999F));
  material.base_color_factor[3] = 1;
  EXPECT_TRUE(materialCoversSample(material, 0.5F));
  EXPECT_FALSE(materialCoversSample(material, 0.499F));
}
