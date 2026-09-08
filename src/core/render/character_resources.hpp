#ifndef CORE_RENDER_CHARACTER_RESOURCES_HPP
#define CORE_RENDER_CHARACTER_RESOURCES_HPP

#include <array>

#include "core/render/character_presentation.hpp"
#include "core/render/point_shadow.hpp"
#include "core/render/sampled_texture.hpp"

class GraphicsPipeline;

// Swapchain-independent character data. Immutable indices are shared by asset;
// each slot's mapped vertices are written only after its completion fence.
// Color and shadows use the same indices and evaluated slot geometry.
class CharacterResources {
 public:
  CharacterResources(VkDevice device, VkPhysicalDevice physical, VkQueue queue,
                     std::uint32_t queue_family,
                     std::span<const CharacterRenderInstance> instances);
  ~CharacterResources();
  CharacterResources(const CharacterResources&) = delete;
  CharacterResources& operator=(const CharacterResources&) = delete;

  void validate(std::span<const CharacterPoseFrame> poses) const;
  // Complete CPU validation/deformation before acquiring a presentation image.
  void deform(std::span<const CharacterPoseFrame> poses);
  void upload(std::size_t slot);
  void draw(VkCommandBuffer commands, std::size_t slot,
            const GraphicsPipeline& pipeline) const;

 private:
  struct Slot {
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    void* mapped{};
    bool uploaded{};
  };
  void cleanup() noexcept;
  VkDevice device_{};
  CharacterPresentation presentation_;
  std::vector<CharacterDeformedVertex> scratch_;
  std::vector<PositionColorVertex> vertices_;
  std::vector<std::unique_ptr<SampledTexture>> materials_;
  VkBuffer index_buffer_{};
  VkDeviceMemory index_memory_{};
  void* mapped_indices_{};
  std::array<Slot, lighting_frame_slot_count> slots_{};
  bool owner_recorded_{};
};

#endif
