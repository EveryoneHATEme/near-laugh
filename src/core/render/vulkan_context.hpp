#ifndef CORE_RENDER_VULKAN_CONTEXT_HPP
#define CORE_RENDER_VULKAN_CONTEXT_HPP

#include <vulkan/vulkan.h>

#include "core/render/vulkan_utils.hpp"

class Window;
class ValidationDiagnostics;

class VulkanContext {
 public:
  VulkanContext(const Window& window, ValidationDiagnostics& diagnostics);
  ~VulkanContext();

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;
  VulkanContext(VulkanContext&&) = delete;
  VulkanContext& operator=(VulkanContext&&) = delete;

  [[nodiscard]] VkInstance instance() const noexcept { return instance_; }
  [[nodiscard]] VkSurfaceKHR surface() const noexcept { return surface_; }
  [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept {
    return physical_device_;
  }
  [[nodiscard]] VkDevice device() const noexcept { return device_; }
  [[nodiscard]] VkQueue graphicsQueue() const noexcept {
    return graphics_queue_;
  }
  [[nodiscard]] VkQueue presentQueue() const noexcept { return present_queue_; }
  [[nodiscard]] QueueFamilySelection queueFamilies() const noexcept {
    return queue_families_;
  }
  [[nodiscard]] bool validationEnabled() const noexcept {
    return validation_enabled_;
  }

 private:
  void createInstance();
  void createDebugMessenger();
  void selectPhysicalDevice();
  void createDevice();
  void cleanup() noexcept;

  VkInstance instance_{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT debug_messenger_{VK_NULL_HANDLE};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  VkQueue present_queue_{VK_NULL_HANDLE};
  QueueFamilySelection queue_families_{};
  const Window& window_;
  ValidationDiagnostics& diagnostics_;
  bool validation_enabled_{};
};

#endif
