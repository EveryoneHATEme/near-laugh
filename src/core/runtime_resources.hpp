#ifndef CORE_RUNTIME_RESOURCES_HPP
#define CORE_RUNTIME_RESOURCES_HPP

#include <array>
#include <filesystem>
#include <optional>

struct RuntimeResources {
  std::filesystem::path root{};
  std::filesystem::path scene_vertex_shader{};
  std::filesystem::path scene_fragment_shader{};
  std::array<std::filesystem::path, 3> scene_textures{};
  std::filesystem::path prototype_chair_model{};
  std::filesystem::path prototype_level{};
};

[[nodiscard]] RuntimeResources resolveRuntimeResources(
    const std::filesystem::path& resource_root,
    const std::optional<std::filesystem::path>& level_path = std::nullopt);

#endif
