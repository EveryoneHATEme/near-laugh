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

TEST(ShaderSources, MatchSpotLightVertexAndPushLayouts) {
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

  EXPECT_NE(vertex.find("layout(location = 3) in vec2 inTextureCoordinates"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 4) in uint inTextureLayer"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 2) out vec2 fragTextureCoordinates"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 3) flat out uint fragTextureLayer"),
            std::string::npos);
  EXPECT_NE(vertex.find("layout(location = 4) out vec3 fragWorldPosition"),
            std::string::npos);
  EXPECT_NE(vertex.find("fragWorldPosition = inPosition"), std::string::npos);
  EXPECT_EQ(vertex.find("SolidMask"), std::string::npos);
  EXPECT_EQ(vertex.find("presentationMasks"), std::string::npos);

  for (const std::string* source : {&vertex, &fragment}) {
    EXPECT_NE(source->find("vec4 spotPositionAndRange"), std::string::npos);
    EXPECT_NE(source->find("vec4 spotDirectionAndInnerCosine"),
              std::string::npos);
    EXPECT_NE(source->find("vec4 spotColorAndIntensity"), std::string::npos);
    EXPECT_NE(source->find("vec4 spotOuterCosineAndEnabled"),
              std::string::npos);
  }

  EXPECT_NE(
      fragment.find("layout(set = 0, binding = 0) uniform sampler2DArray"),
      std::string::npos);
  EXPECT_NE(fragment.find("layout(std140, set = 1, binding = 0) uniform"),
            std::string::npos);
  EXPECT_NE(fragment.find("PointLight pointLights[2]"), std::string::npos);
  EXPECT_NE(fragment.find("for (int lightIndex = 0; lightIndex < 2;"),
            std::string::npos);
  EXPECT_NE(fragment.find("spotOuterCosineAndEnabled.y > 0.5"),
            std::string::npos);
  EXPECT_NE(fragment.find("smoothstep("), std::string::npos);
  EXPECT_NE(fragment.find("distanceToLight / scene.spotPositionAndRange.w"),
            std::string::npos);
  EXPECT_NE(fragment.find("max(dot(normal, -directionFromLight), 0.0)"),
            std::string::npos);
  EXPECT_NE(fragment.find("clamp(accumulatedLighting, vec3(0.0), vec3(1.0))"),
            std::string::npos);
  EXPECT_EQ(fragment.find("fragSolidMask"), std::string::npos);
  EXPECT_EQ(fragment.find("presentationMasks"), std::string::npos);

  const std::size_t sample = fragment.find("texture(");
  const std::size_t tint = fragment.find("sampledColor * fragColor.rgb");
  const std::size_t point_lighting = fragment.find("for (int lightIndex");
  const std::size_t spot_lighting =
      fragment.find("spotOuterCosineAndEnabled.y > 0.5");
  const std::size_t output =
      fragment.find("presentedColor * clamp(accumulatedLighting");
  ASSERT_NE(sample, std::string::npos);
  ASSERT_NE(tint, std::string::npos);
  ASSERT_NE(point_lighting, std::string::npos);
  ASSERT_NE(spot_lighting, std::string::npos);
  ASSERT_NE(output, std::string::npos);
  EXPECT_LT(sample, tint);
  EXPECT_LT(tint, point_lighting);
  EXPECT_LT(point_lighting, spot_lighting);
  EXPECT_LT(spot_lighting, output);
}
