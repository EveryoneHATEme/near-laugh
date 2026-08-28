#include "core/runtime_resources.hpp"

#include <stdexcept>
#include <string>

namespace {
std::filesystem::path normalizedAbsolute(
    const std::filesystem::path& path) {
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
      root, root / "shaders" / "triangle_vertex.spv",
      root / "shaders" / "triangle_fragment.spv"};
  requireFile(resources.triangle_vertex_shader);
  requireFile(resources.triangle_fragment_shader);
  return resources;
}
