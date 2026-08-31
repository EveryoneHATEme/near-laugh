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
  const auto vertex =
      readSpirvFile(root / "shaders/prototype_scene_vertex.spv");
  const auto fragment =
      readSpirvFile(root / "shaders/prototype_scene_fragment.spv");
  ASSERT_FALSE(vertex.empty());
  ASSERT_FALSE(fragment.empty());
  EXPECT_EQ(vertex.front(), 0x07230203U);
  EXPECT_EQ(fragment.front(), 0x07230203U);
}

TEST(ShaderSources, MatchSolidMaskVertexAndPushLayouts) {
  const std::filesystem::path shaders =
      std::filesystem::absolute("resources/shaders").lexically_normal();
  auto readText = [](const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string{std::istreambuf_iterator<char>{input},
                       std::istreambuf_iterator<char>{}};
  };
  const std::string vertex = readText(shaders / "prototype_scene_vertex.glsl");
  const std::string fragment =
      readText(shaders / "prototype_scene_fragment.glsl");

  EXPECT_NE(vertex.find("layout(location = 3) in uint inSolidMask"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 4) in vec2 inTextureCoordinates"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 5) in uint inTextureLayer"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 3) out vec2 fragTextureCoordinates"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 4) flat out uint fragTextureLayer"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 5) out vec3 fragWorldPosition"),
            std::string::npos);
  EXPECT_NE(vertex.find("fragWorldPosition = inPosition"), std::string::npos);
  EXPECT_NE(vertex.find("flat out uint fragSolidMask"), std::string::npos);
  EXPECT_NE(vertex.find("uvec4 presentationMasks"), std::string::npos);
  EXPECT_NE(fragment.find("flat in uint fragSolidMask"), std::string::npos);
  EXPECT_NE(
      fragment.find("layout(location = 3) in vec2 fragTextureCoordinates"),
      std::string::npos);
  EXPECT_NE(fragment.find("layout(location = 4) flat in uint fragTextureLayer"),
            std::string::npos);
  EXPECT_NE(fragment.find("layout(location = 5) in vec3 fragWorldPosition"),
            std::string::npos);
  EXPECT_NE(
      fragment.find("layout(set = 0, binding = 0) uniform sampler2DArray"),
      std::string::npos);
  EXPECT_NE(fragment.find("layout(std140, set = 1, binding = 0) uniform"),
            std::string::npos);
  EXPECT_NE(fragment.find("PointLight pointLights[2]"), std::string::npos);
  EXPECT_NE(fragment.find("for (int lightIndex = 0; lightIndex < 2;"),
            std::string::npos);
  EXPECT_NE(fragment.find("max(distanceToLight, 0.0001)"), std::string::npos);
  EXPECT_NE(fragment.find("distanceToLight / light.positionAndRadius.w"),
            std::string::npos);
  EXPECT_NE(fragment.find("falloff *= falloff"), std::string::npos);
  EXPECT_NE(fragment.find("clamp(accumulatedLighting, vec3(0.0), vec3(1.0))"),
            std::string::npos);
  const std::size_t sample = fragment.find("texture(");
  const std::size_t tint = fragment.find("sampledColor * fragColor.rgb");
  EXPECT_NE(fragment.find("uvec4 presentationMasks"), std::string::npos);
  const std::size_t highlight = fragment.find("presentationMasks.x");
  const std::size_t dim = fragment.find("presentationMasks.y");
  const std::size_t lighting =
      fragment.find("presentedColor * clamp(accumulatedLighting");
  ASSERT_NE(sample, std::string::npos);
  ASSERT_NE(tint, std::string::npos);
  ASSERT_NE(highlight, std::string::npos);
  ASSERT_NE(dim, std::string::npos);
  ASSERT_NE(lighting, std::string::npos);
  EXPECT_LT(sample, tint);
  EXPECT_LT(tint, highlight);
  EXPECT_LT(highlight, dim);
  EXPECT_LT(dim, lighting);
  EXPECT_NE(fragment.find(",\n        1.0);"), std::string::npos);
}
