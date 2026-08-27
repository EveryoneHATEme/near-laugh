#ifndef CORE_RENDER_RENDERER_H
#define CORE_RENDER_RENDERER_H

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "core/render/vulkan_context.hpp"

class GraphicsPipeline;
class Window;

class Renderer {
 public:
  explicit Renderer(Window& window);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;

  [[nodiscard]] bool renderFrame();
  void requestSwapchainRecreation() noexcept;
  [[nodiscard]] bool validationEnabled() const noexcept;

 private:
  static constexpr std::size_t frames_in_flight = 2;

  struct FrameSlot {
    VkCommandPool command_pool{VK_NULL_HANDLE};
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    VkSemaphore image_available{VK_NULL_HANDLE};
    VkSemaphore render_finished{VK_NULL_HANDLE};
    VkFence completion{VK_NULL_HANDLE};
  };

  void createSwapchain();
  void cleanupSwapchain() noexcept;
  void recreateSwapchain();
  void createFrameSlots();
  void cleanupFrameSlots() noexcept;
  void recordFrame(VkCommandBuffer command_buffer, std::uint32_t image_index);
  [[nodiscard]] bool waitForRenderableExtent();

  Window& window_;
  VulkanContext context_;
  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat swapchain_format_{VK_FORMAT_UNDEFINED};
  VkExtent2D swapchain_extent_{};
  std::vector<VkImage> swapchain_images_{};
  std::vector<VkImageView> swapchain_views_{};
  std::vector<VkFence> image_fences_{};
  std::vector<bool> image_initialized_{};
  std::unique_ptr<GraphicsPipeline> pipeline_{};
  std::array<FrameSlot, frames_in_flight> frames_{};
  std::size_t current_frame_{};
  bool recreate_requested_{};
};

#endif
