#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/renderer.hpp"

int main() {
  try {
    Platform platform;
    Window window(640, 480, "near-laugh Vulkan smoke");
    window.setCursorCaptured(true);
    if (!window.cursorCaptured()) {
      throw std::runtime_error("Cursor capture did not enable");
    }
    window.setCursorCaptured(false);
    if (window.cursorCaptured()) {
      throw std::runtime_error("Cursor capture did not disable");
    }
    Renderer renderer(window);
    std::cout << "Smoke validation: "
              << (renderer.validationEnabled() ? "enabled" : "unavailable")
              << '\n';
    for (int frame = 0; frame < 120 && !window.shouldClose(); ++frame) {
      window.pollEvents();
      if (frame == 20) {
        window.setSize(800, 600);
      }
      if (frame == 40) {
        renderer.requestSwapchainRecreation();
      }
      if (frame == 60) {
        window.minimize();
        for (int poll = 0; poll < 20 && !window.framebufferExtent().isZero();
             ++poll) {
          window.pollEvents();
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!window.framebufferExtent().isZero()) {
          std::cout << "Smoke note: desktop did not report a zero-sized "
                       "framebuffer while minimized\n";
        }
        window.restore();
        for (int poll = 0; poll < 20 && window.framebufferExtent().isZero();
             ++poll) {
          window.pollEvents();
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
      static_cast<void>(renderer.renderFrame());
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Vulkan smoke failed: " << error.what() << '\n';
    return 1;
  }
}
