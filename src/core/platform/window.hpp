#ifndef CORE_PLATFORM_WINDOW_HPP
#define CORE_PLATFORM_WINDOW_HPP

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "core/platform/input.hpp"

struct FramebufferExtent {
  std::uint32_t width{};
  std::uint32_t height{};

  [[nodiscard]] bool isZero() const noexcept {
    return width == 0 || height == 0;
  }
};

class Window {
 public:
  // Incomplete implementation type keeps GLFW declarations out of this header.
  struct Impl;

  Window(std::uint32_t width, std::uint32_t height, std::string_view title);
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  void pollEvents();
  void waitEvents() const;
  [[nodiscard]] bool shouldClose() const;
  [[nodiscard]] FramebufferExtent framebufferExtent() const;
  [[nodiscard]] bool consumeFramebufferResize() noexcept;
  void setSize(std::uint32_t width, std::uint32_t height);
  void minimize();
  void restore();

  [[nodiscard]] const InputSnapshot& input() const noexcept;
  void setCursorCaptured(bool captured);
  [[nodiscard]] bool cursorCaptured() const noexcept;

  [[nodiscard]] std::vector<const char*> requiredVulkanExtensions() const;
  [[nodiscard]] VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

 private:
  std::unique_ptr<Impl> impl_;
};

#endif
