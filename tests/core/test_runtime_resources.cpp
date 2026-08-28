#include <gtest/gtest.h>

#include <filesystem>

#include "core/runtime_resources.hpp"

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
  } catch (...) {
    std::filesystem::current_path(original_working_directory);
    throw;
  }
  std::filesystem::current_path(original_working_directory);
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
