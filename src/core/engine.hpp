#ifndef CORE_ENGINE_HPP
#define CORE_ENGINE_HPP

#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/renderer.hpp"

class Engine {
 public:
  Engine();
  ~Engine() = default;

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

  void run();
  [[nodiscard]] bool tick();

 private:
  Platform platform_;
  Window window_;
  Renderer renderer_;
};

#endif
