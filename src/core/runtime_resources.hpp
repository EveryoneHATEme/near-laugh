#ifndef CORE_RUNTIME_RESOURCES_HPP
#define CORE_RUNTIME_RESOURCES_HPP

#include <array>
#include <filesystem>

struct RuntimeResources {
  std::filesystem::path root{};
  std::filesystem::path scene_vertex_shader{};
  std::filesystem::path scene_fragment_shader{};
  std::array<std::filesystem::path, 4> scene_textures{};
  std::filesystem::path prototype_chair_model{};
};

[[nodiscard]] RuntimeResources resolveRuntimeResources(
    const std::filesystem::path& resource_root);

#endif
