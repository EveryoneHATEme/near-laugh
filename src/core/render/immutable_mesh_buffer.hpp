#ifndef CORE_RENDER_IMMUTABLE_MESH_BUFFER_HPP
#define CORE_RENDER_IMMUTABLE_MESH_BUFFER_HPP

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "core/render/prototype_scene.hpp"

[[nodiscard]] constexpr bool immutableMeshVertexCountIsValid(
    std::size_t count) noexcept {
  return count != 0 && count % 3 == 0 &&
         count <= static_cast<std::size_t>(UINT32_MAX);
}

class ImmutableMeshBuffer {
 public:
  ImmutableMeshBuffer(VkDevice device, VkPhysicalDevice physical_device,
                      std::span<const PositionColorVertex> vertices,
                      std::string_view lifecycle_name);
  ~ImmutableMeshBuffer();

  ImmutableMeshBuffer(const ImmutableMeshBuffer&) = delete;
  ImmutableMeshBuffer& operator=(const ImmutableMeshBuffer&) = delete;
  ImmutableMeshBuffer(ImmutableMeshBuffer&&) = delete;
  ImmutableMeshBuffer& operator=(ImmutableMeshBuffer&&) = delete;

  void bindAndDraw(VkCommandBuffer command_buffer) const;
  [[nodiscard]] std::uint32_t vertexCount() const noexcept {
    return vertex_count_;
  }

 private:
  [[nodiscard]] std::string event(std::string_view suffix) const;
  [[nodiscard]] std::string failureStage(std::string_view suffix) const;
  void cleanup() noexcept;

  VkDevice device_{VK_NULL_HANDLE};
  VkBuffer buffer_{VK_NULL_HANDLE};
  VkDeviceMemory memory_{VK_NULL_HANDLE};
  std::uint32_t vertex_count_{};
  std::string lifecycle_name_{};
  bool lifecycle_recorded_{};
  mutable bool draw_recorded_{};
};

#endif
