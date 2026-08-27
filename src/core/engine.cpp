#include "core/engine.hpp"

Engine::Engine()
    : platform_(), window_(1024, 768, "near-laugh FPS"), renderer_(window_) {}

void Engine::run() {
  while (tick()) {
  }
}

bool Engine::tick() {
  window_.pollEvents();
  if (window_.shouldClose()) {
    return false;
  }
  static_cast<void>(renderer_.renderFrame());
  return !window_.shouldClose();
}
