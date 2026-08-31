#include "core/runtime_resources.hpp"

#include <stdexcept>
#include <string>

namespace {
std::filesystem::path normalizedAbsolute(const std::filesystem::path& path) {
  return std::filesystem::absolute(path).lexically_normal();
}

void requireFile(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("Required runtime asset is missing: " +
                             path.string());
  }
}
}  // namespace

RuntimeResources resolveRuntimeResources(
    const std::filesystem::path& resource_root) {
  const std::filesystem::path root = normalizedAbsolute(resource_root);
  if (!std::filesystem::is_directory(root)) {
    throw std::runtime_error("Runtime resource root is missing: " +
                             root.string());
  }

  RuntimeResources resources{
      root,
      root / "shaders" / "prototype_scene_vertex.spv",
      root / "shaders" / "prototype_scene_fragment.spv",
      {root / "textures" / "prototype_floor.png",
       root / "textures" / "prototype_boundary.png",
       root / "textures" / "prototype_obstacle.png",
       root / "textures" / "prototype_shooting_target.png"}};
  requireFile(resources.scene_vertex_shader);
  requireFile(resources.scene_fragment_shader);
  for (const std::filesystem::path& texture : resources.scene_textures) {
    requireFile(texture);
  }
  return resources;
}
