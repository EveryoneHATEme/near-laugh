#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <span>
#include <string>

#include "core/runtime_resources.hpp"

namespace {
const std::array<std::filesystem::path, 8> runtimeAssetPaths = {
    "shaders/prototype_scene_vertex.spv",
    "shaders/prototype_scene_fragment.spv",
    "textures/prototype_floor.png",
    "textures/prototype_boundary.png",
    "textures/prototype_obstacle.png",
    "textures/prototype_shooting_target.png",
    "models/prototype_chair.glb",
    "levels/prototype.level.json"};

std::filesystem::path makeCompleteRuntimeRoot() {
  const std::filesystem::path root = (std::filesystem::temp_directory_path() /
                                      "near_laugh_runtime_resources_test")
                                         .lexically_normal();
  std::filesystem::remove_all(root);
  for (const std::filesystem::path& relative : runtimeAssetPaths) {
    const std::filesystem::path destination = root / relative;
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(
        std::filesystem::absolute("resources") / relative, destination);
  }
  return root;
}

void expectMissingPathReported(const std::filesystem::path& root,
                               const std::filesystem::path& relative) {
  const std::filesystem::path missing = (root / relative).lexically_normal();
  std::filesystem::remove(missing);
  try {
    static_cast<void>(resolveRuntimeResources(root));
    FAIL() << "Expected missing runtime asset failure for " << missing;
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find(missing.string()),
              std::string::npos);
  }
}
}  // namespace

TEST(RuntimeResources, ResolvesShadersIndependentlyOfWorkingDirectory) {
  const std::filesystem::path resource_root =
      std::filesystem::absolute("resources").lexically_normal();
  const std::filesystem::path original_working_directory =
      std::filesystem::current_path();
  std::filesystem::current_path(std::filesystem::temp_directory_path());
  try {
    const RuntimeResources resources = resolveRuntimeResources(resource_root);
    EXPECT_TRUE(
        std::filesystem::is_regular_file(resources.scene_vertex_shader));
    EXPECT_TRUE(
        std::filesystem::is_regular_file(resources.scene_fragment_shader));
    for (const std::filesystem::path& texture : resources.scene_textures) {
      EXPECT_TRUE(std::filesystem::is_regular_file(texture));
      EXPECT_EQ(texture.parent_path(), resource_root / "textures");
    }
    EXPECT_TRUE(
        std::filesystem::is_regular_file(resources.prototype_chair_model));
    EXPECT_EQ(resources.prototype_chair_model,
              resource_root / "models" / "prototype_chair.glb");
    EXPECT_TRUE(std::filesystem::is_regular_file(resources.prototype_level));
    EXPECT_EQ(resources.prototype_level,
              resource_root / "levels" / "prototype.level.json");
  } catch (...) {
    std::filesystem::current_path(original_working_directory);
    throw;
  }
  std::filesystem::current_path(original_working_directory);
}

TEST(RuntimeResources, EveryMissingTextureReportsItsResolvedAbsolutePath) {
  for (const std::filesystem::path& relative :
       std::span{runtimeAssetPaths}.subspan(2, 4)) {
    const std::filesystem::path root = makeCompleteRuntimeRoot();
    expectMissingPathReported(root, relative);
    std::filesystem::remove_all(root);
  }
}

TEST(RuntimeResources, MissingModelReportsItsResolvedAbsolutePath) {
  const std::filesystem::path root = makeCompleteRuntimeRoot();
  expectMissingPathReported(root, "models/prototype_chair.glb");
  std::filesystem::remove_all(root);
}

TEST(RuntimeResources, MissingLevelReportsItsResolvedAbsolutePath) {
  const std::filesystem::path root = makeCompleteRuntimeRoot();
  expectMissingPathReported(root, "levels/prototype.level.json");
  std::filesystem::remove_all(root);
}

TEST(RuntimeResources, MissingShaderReportsResolvedAbsolutePath) {
  const std::filesystem::path root = (std::filesystem::temp_directory_path() /
                                      "near_laugh_missing_runtime_resources")
                                         .lexically_normal();
  std::filesystem::create_directories(root / "shaders");
  const std::filesystem::path missing =
      (root / "shaders" / "prototype_scene_vertex.spv").lexically_normal();
  try {
    static_cast<void>(resolveRuntimeResources(root));
    FAIL() << "Expected missing runtime shader failure";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find(missing.string()),
              std::string::npos);
  }
  std::filesystem::remove_all(root);
}
