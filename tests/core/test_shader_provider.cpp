#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

#include "core/render/graphics_pipeline.hpp"
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

TEST(ShaderProvider, PackagedStagesUseOnlyEnabledDeviceCapabilities) {
  for (const auto* file :
       {"prototype_scene_vertex.spv", "prototype_scene_fragment.spv"}) {
    const auto words =
        readSpirvFile(std::filesystem::path("resources/shaders") / file);
    ASSERT_GE(words.size(), 5U);
    bool shader = false;
    for (std::size_t i = 5; i < words.size();) {
      const auto count = words[i] >> 16;
      const auto opcode = words[i] & 0xffff;
      ASSERT_GT(count, 0U);
      ASSERT_LE(count, words.size() - i);
      if (opcode == 17) {  // OpCapability
        ASSERT_EQ(count, 2U);
        // The runtime enables basic Shader capability, without optional
        // demote-to-helper-invocation or other shader device features.
        EXPECT_EQ(words[i + 1], 1U)
            << file << " requires an unenabled capability";
        shader |= words[i + 1] == 1;
      }
      i += count;
    }
    EXPECT_TRUE(shader) << file;
  }
}

TEST(ShaderProvider, PackagedStagesMatchHostPushConstantOffsets) {
  for (const auto* file :
       {"prototype_scene_vertex.spv", "prototype_scene_fragment.spv"}) {
    const auto words =
        readSpirvFile(std::filesystem::path("resources/shaders") / file);
    std::map<std::uint32_t, std::uint32_t> pointers;
    std::map<std::uint32_t, std::map<std::uint32_t, std::uint32_t>> offsets;
    std::uint32_t push_pointer = 0;
    for (std::size_t i = 5; i < words.size();) {
      const auto count = words[i] >> 16;
      const auto opcode = words[i] & 0xffff;
      ASSERT_GT(count, 0U);
      ASSERT_LE(i + count, words.size());
      // OpTypePointer, OpVariable with PushConstant storage, and
      // OpMemberDecorate with Offset. The ABI does not depend on names.
      if (opcode == 32 && count == 4) pointers[words[i + 1]] = words[i + 3];
      if (opcode == 59 && count >= 4 && words[i + 3] == 9)
        push_pointer = words[i + 1];
      if (opcode == 72 && count == 5 && words[i + 3] == 35)
        offsets[words[i + 1]][words[i + 2]] = words[i + 4];
      i += count;
    }
    ASSERT_NE(push_pointer, 0U) << file;
    const auto& members = offsets[pointers.at(push_pointer)];
    const std::array<std::size_t, 5> host{
        offsetof(ScenePushConstant, camera),
        offsetof(ScenePushConstant, spot_position_and_range),
        offsetof(ScenePushConstant, spot_direction_and_inner_cosine),
        offsetof(ScenePushConstant, spot_color_and_intensity),
        offsetof(ScenePushConstant, light_controls)};
    ASSERT_EQ(members.size(), host.size());
    for (std::uint32_t i = 0; i < host.size(); ++i)
      EXPECT_EQ(members.at(i), host[i]) << file;
  }
}
