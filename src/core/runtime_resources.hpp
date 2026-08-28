#ifndef CORE_RUNTIME_RESOURCES_HPP
#define CORE_RUNTIME_RESOURCES_HPP

#include <filesystem>

struct RuntimeResources {
  std::filesystem::path root{};
  std::filesystem::path triangle_vertex_shader{};
  std::filesystem::path triangle_fragment_shader{};
};

[[nodiscard]] RuntimeResources resolveRuntimeResources(
    const std::filesystem::path& resource_root);

#endif
