#include "core/runtime_resources.hpp"

#include <stdexcept>
#include <string>

namespace {
std::string pathText(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}
std::filesystem::path normalizedAbsolute(const std::filesystem::path& path) {
  return std::filesystem::absolute(path).lexically_normal();
}

void requireFile(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("Required runtime asset is missing: " +
                             pathText(path));
  }
}
}  // namespace

RuntimeResources resolveRuntimeResources(
    const std::filesystem::path& resource_root,
    const std::optional<std::filesystem::path>& level_path) {
  const std::filesystem::path root = normalizedAbsolute(resource_root);
  if (!std::filesystem::is_directory(root)) {
    throw std::runtime_error("Runtime resource root is missing: " +
                             pathText(root));
  }

  RuntimeResources resources{
      root, root / "shaders" / "prototype_scene_vertex.spv",
      root / "shaders" / "prototype_scene_fragment.spv",
      level_path ? normalizedAbsolute(*level_path)
                 : root / "levels" / "prototype.level.json"};
  requireFile(resources.scene_vertex_shader);
  requireFile(resources.scene_fragment_shader);
  requireFile(resources.prototype_level);
  return resources;
}
