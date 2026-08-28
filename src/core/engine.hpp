#ifndef CORE_ENGINE_HPP
#define CORE_ENGINE_HPP

#include <utility>

#include "core/camera/free_fly_camera.hpp"
#include "core/input/fps_input.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/renderer.hpp"
#include "core/runtime_resources.hpp"
#include "near_laugh/runtime_config.hpp"

class ValidationDiagnostics;

class Engine {
 public:
  Engine(const near_laugh::RuntimeConfig& config, RuntimeResources resources,
         ValidationDiagnostics& diagnostics);
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
  FpsInputMapper input_mapper_{};
  FpsActionSnapshot input_{};
  FreeFlyCamera camera_{};
  FrameClock frame_clock_{};
  Renderer renderer_;
};

#endif
