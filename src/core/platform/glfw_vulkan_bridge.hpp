#ifndef CORE_PLATFORM_GLFW_VULKAN_BRIDGE_HPP
#define CORE_PLATFORM_GLFW_VULKAN_BRIDGE_HPP

#include <vulkan/vulkan.h>

#include <vector>

class Window;

class GlfwVulkanBridge {
 public:
  [[nodiscard]] static std::vector<const char*> requiredInstanceExtensions();
  [[nodiscard]] static VkSurfaceKHR createSurface(VkInstance instance,
                                                  const Window& window);
};

#endif
