#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "core/resources/shader_provider.hpp"

TEST(ShaderProvider, MissingAssetReportsItsPath) {
  const std::filesystem::path path = "definitely-missing-shader.spv";
  try {
    static_cast<void>(readSpirvFile(path));
    FAIL() << "Expected missing shader failure";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find(path.string()), std::string::npos);
  }
}

TEST(ShaderProvider, RejectsMalformedSpirvSize) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "near_laugh_bad_shader.spv";
  {
    std::ofstream output(path, std::ios::binary);
    output.write("bad", 3);
  }
  EXPECT_THROW(static_cast<void>(readSpirvFile(path)), std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ShaderProvider, LoadsProjectShaders) {
  EXPECT_FALSE(readSpirvFile("resources/shaders/triangle_vertex.spv").empty());
  EXPECT_FALSE(
      readSpirvFile("resources/shaders/triangle_fragment.spv").empty());
}
