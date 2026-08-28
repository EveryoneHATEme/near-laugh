#ifndef CORE_PLATFORM_WINDOW_HPP
#define CORE_PLATFORM_WINDOW_HPP

#include <cstdint>
#include <memory>
#include <string_view>

#include "core/frame.hpp"
#include "core/platform/input.hpp"

class Platform;
class GlfwVulkanBridge;

class Window {
 public:
  // Incomplete implementation type keeps GLFW declarations out of this header.
  struct Impl;

  Window(Platform& platform, std::uint32_t width, std::uint32_t height,
         std::string_view title);
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

  [[nodiscard]] const PhysicalInputSnapshot& input() const noexcept;
  void setCursorCaptured(bool captured);
  [[nodiscard]] bool cursorCaptured() const noexcept;

 private:
  friend class GlfwVulkanBridge;
  [[nodiscard]] void* surfaceBridgeHandle() const noexcept;

  Platform& platform_;
  std::unique_ptr<Impl> impl_;
};

#endif
