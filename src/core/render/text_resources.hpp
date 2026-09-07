#ifndef CORE_RENDER_TEXT_RESOURCES_HPP
#define CORE_RENDER_TEXT_RESOURCES_HPP

#include <vulkan/vulkan.h>

#include <memory>

#include "core/render/sampled_texture.hpp"
#include "core/text/caption_font.hpp"

// Immutable atlas plus one mapped glyph buffer per existing fenced frame slot.
class TextResources final {
 public:
  TextResources(VkDevice device, VkPhysicalDevice physical_device,
                VkQueue queue, unsigned queue_family, VkFormat color_format,
                VkFormat depth_format, const std::filesystem::path& root,
                const CaptionFont& font);
  ~TextResources();
  TextResources(const TextResources&) = delete;
  TextResources& operator=(const TextResources&) = delete;
  void recreatePipeline(VkFormat color, VkFormat depth);
  void update(unsigned slot, std::span<const TextVertex> vertices);
  void draw(VkCommandBuffer commands, unsigned slot) const;

 private:
  void cleanup() noexcept;
  VkDevice device_;
  std::filesystem::path root_;
  std::unique_ptr<SampledTexture> atlas_;
  VkPipelineLayout layout_{};
  VkPipeline pipeline_{};
  struct Buffer {
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    void* mapped{};
    unsigned count{};
  };
  std::array<Buffer, 2> frames_{};
};

#endif
