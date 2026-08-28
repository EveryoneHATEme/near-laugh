#include "core/engine.hpp"

#include <utility>

#include "core/render/validation_diagnostics.hpp"

Engine::Engine(const near_laugh::RuntimeConfig& config,
               RuntimeResources resources,
               ValidationDiagnostics& diagnostics)
    : platform_(),
      window_(platform_, config.window_width, config.window_height,
              config.window_title),
      renderer_(window_, window_.framebufferExtent(),
                {std::move(resources.triangle_vertex_shader),
                 std::move(resources.triangle_fragment_shader)},
                diagnostics) {}

void Engine::run() {
  while (tick()) {
  }
}

bool Engine::tick() {
  window_.pollEvents();
  input_ = input_mapper_.map(window_.input());
  const LoopDecision decision =
      decideLoopAction(window_.shouldClose(), window_.framebufferExtent(),
                       window_.consumeFramebufferResize());
  switch (decision.action) {
    case LoopAction::Stop:
      return false;
    case LoopAction::WaitForEvents:
      window_.waitEvents();
      return !window_.shouldClose();
    case LoopAction::Render:
      static_cast<void>(renderer_.renderFrame(decision.frame));
      return !window_.shouldClose();
  }
  return false;
}
