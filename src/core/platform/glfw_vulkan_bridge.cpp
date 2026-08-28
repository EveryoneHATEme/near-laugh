#include "core/platform/glfw_vulkan_bridge.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <stdexcept>
#include <string>

#include "core/platform/window.hpp"

std::vector<const char*> GlfwVulkanBridge::requiredInstanceExtensions() {
  std::uint32_t count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&count);
  if (extensions == nullptr || count == 0) {
    throw std::runtime_error(
        "GLFW did not provide the Vulkan instance extensions required for "
        "surface creation");
  }
  return {extensions, extensions + count};
}

VkSurfaceKHR GlfwVulkanBridge::createSurface(VkInstance instance,
                                             const Window& window) {
  auto* handle = static_cast<GLFWwindow*>(window.surfaceBridgeHandle());
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  const VkResult result =
      glfwCreateWindowSurface(instance, handle, nullptr, &surface);
  if (result != VK_SUCCESS) {
    throw std::runtime_error(
        "GLFW Vulkan surface creation failed with VkResult " +
        std::to_string(result));
  }
  return surface;
}
