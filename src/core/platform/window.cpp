#include "core/platform/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

#include "core/platform/platform.hpp"
#include "core/testing/test_controls.hpp"

struct Window::Impl {
  GLFWwindow* handle{};
  InputAccumulator input{};
  bool framebuffer_resized{};
  bool cursor_captured{};
};

namespace {
Window::Impl& implementation(GLFWwindow* window) {
  return *static_cast<Window::Impl*>(glfwGetWindowUserPointer(window));
}

void framebufferSizeCallback(GLFWwindow* window, int, int) {
  implementation(window).framebuffer_resized = true;
}

void cursorPositionCallback(GLFWwindow* window, double x, double y) {
  implementation(window).input.addCursorPosition(x, y);
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
  if (action != GLFW_PRESS && action != GLFW_RELEASE) {
    return;
  }
  const bool down = action == GLFW_PRESS;
  auto& input = implementation(window).input;
  switch (key) {
    case GLFW_KEY_E:
      input.setKey(PhysicalKey::E, down);
      break;
    case GLFW_KEY_W:
      input.setKey(PhysicalKey::W, down);
      break;
    case GLFW_KEY_S:
      input.setKey(PhysicalKey::S, down);
      break;
    case GLFW_KEY_A:
      input.setKey(PhysicalKey::A, down);
      break;
    case GLFW_KEY_D:
      input.setKey(PhysicalKey::D, down);
      break;
    case GLFW_KEY_SPACE:
      input.setKey(PhysicalKey::Space, down);
      break;
    case GLFW_KEY_LEFT_SHIFT:
      input.setKey(PhysicalKey::LeftShift, down);
      break;
    case GLFW_KEY_LEFT_CONTROL:
      input.setKey(PhysicalKey::LeftControl, down);
      break;
    case GLFW_KEY_ESCAPE:
      input.setKey(PhysicalKey::Escape, down);
      break;
    default:
      break;
  }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
  if (action != GLFW_PRESS && action != GLFW_RELEASE) {
    return;
  }
  const bool down = action == GLFW_PRESS;
  auto& input = implementation(window).input;
  switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
      input.setMouseButton(PhysicalMouseButton::Left, down);
      break;
    case GLFW_MOUSE_BUTTON_RIGHT:
      input.setMouseButton(PhysicalMouseButton::Right, down);
      break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
      input.setMouseButton(PhysicalMouseButton::Middle, down);
      break;
    default:
      break;
  }
}
}  // namespace

Window::Window(Platform& platform, std::uint32_t width, std::uint32_t height,
               std::string_view title)
    : platform_(platform), impl_(std::make_unique<Impl>()) {
  if (width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("Window dimensions exceed GLFW's integer range");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  const std::string owned_title(title);
  impl_->handle =
      glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
                       owned_title.c_str(), nullptr, nullptr);
  if (impl_->handle == nullptr) {
    const char* description = nullptr;
    const int code = glfwGetError(&description);
    throw std::runtime_error(
        "Window creation failed (GLFW error " + std::to_string(code) +
        "): " + (description != nullptr ? description : "unknown error"));
  }

  glfwSetWindowUserPointer(impl_->handle, impl_.get());
  glfwSetFramebufferSizeCallback(impl_->handle, framebufferSizeCallback);
  glfwSetCursorPosCallback(impl_->handle, cursorPositionCallback);
  glfwSetKeyCallback(impl_->handle, keyCallback);
  glfwSetMouseButtonCallback(impl_->handle, mouseButtonCallback);
  recordLifecycleEvent("window.created");
}

Window::~Window() {
  if (impl_ != nullptr && impl_->handle != nullptr) {
    glfwDestroyWindow(impl_->handle);
    recordLifecycleEvent("window.destroyed");
  }
}

void Window::pollEvents() {
  impl_->input.beginEventBatch();
  glfwPollEvents();
}

void Window::waitEvents() {
  impl_->input.beginEventBatch();
  glfwWaitEvents();
}

bool Window::shouldClose() const {
  return glfwWindowShouldClose(impl_->handle) == GLFW_TRUE;
}

FramebufferExtent Window::framebufferExtent() const {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(impl_->handle, &width, &height);
  return {static_cast<std::uint32_t>(std::max(width, 0)),
          static_cast<std::uint32_t>(std::max(height, 0))};
}

bool Window::consumeFramebufferResize() noexcept {
  const bool resized = impl_->framebuffer_resized;
  impl_->framebuffer_resized = false;
  return resized;
}

void Window::setSize(std::uint32_t width, std::uint32_t height) {
  if (width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("Window dimensions exceed GLFW's integer range");
  }
  glfwSetWindowSize(impl_->handle, static_cast<int>(width),
                    static_cast<int>(height));
}

void Window::minimize() { glfwIconifyWindow(impl_->handle); }

void Window::restore() { glfwRestoreWindow(impl_->handle); }

void Window::cancelCloseRequest() noexcept {
  glfwSetWindowShouldClose(impl_->handle, GLFW_FALSE);
}

const PhysicalInputSnapshot& Window::input() const noexcept {
  return impl_->input.snapshot();
}

void Window::setCursorCaptured(bool captured) {
  if (captured == impl_->cursor_captured) {
    return;
  }
  glfwSetInputMode(impl_->handle, GLFW_CURSOR,
                   captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  impl_->cursor_captured = captured;
  impl_->input.resetCursorTracking();
}

bool Window::cursorCaptured() const noexcept { return impl_->cursor_captured; }

void* Window::surfaceBridgeHandle() const noexcept { return impl_->handle; }
