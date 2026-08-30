#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "core/resources/shader_provider.hpp"

TEST(ShaderProvider, MissingAssetReportsItsPath) {
  const std::filesystem::path path =
      std::filesystem::absolute("definitely-missing-shader.spv")
          .lexically_normal();
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
  const std::filesystem::path root =
      std::filesystem::absolute("resources").lexically_normal();
  EXPECT_FALSE(
      readSpirvFile(root / "shaders/prototype_scene_vertex.spv").empty());
  EXPECT_FALSE(
      readSpirvFile(root / "shaders/prototype_scene_fragment.spv").empty());
}

TEST(ShaderSources, MatchSolidMaskVertexAndPushLayouts) {
  const std::filesystem::path shaders =
      std::filesystem::absolute("resources/shaders").lexically_normal();
  auto readText = [](const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string{std::istreambuf_iterator<char>{input},
                       std::istreambuf_iterator<char>{}};
  };
  const std::string vertex =
      readText(shaders / "prototype_scene_vertex.glsl");
  const std::string fragment =
      readText(shaders / "prototype_scene_fragment.glsl");

  EXPECT_NE(vertex.find("layout(location = 3) in uint inSolidMask"),
            std::string::npos);
  EXPECT_NE(vertex.find("flat out uint fragSolidMask"), std::string::npos);
  EXPECT_NE(vertex.find("uvec4 presentationMasks"), std::string::npos);
  EXPECT_NE(fragment.find("flat in uint fragSolidMask"), std::string::npos);
  EXPECT_NE(fragment.find("uvec4 presentationMasks"), std::string::npos);
  const std::size_t highlight = fragment.find("presentationMasks.x");
  const std::size_t dim = fragment.find("presentationMasks.y");
  ASSERT_NE(highlight, std::string::npos);
  ASSERT_NE(dim, std::string::npos);
  EXPECT_LT(highlight, dim);
}
