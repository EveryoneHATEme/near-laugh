#ifndef CORE_APPLICATION_HPP
#define CORE_APPLICATION_HPP

#include <SDL3/SDL.h>

#include <memory>

#include "game/game_world.hpp"
#include "input/input_state.hpp"
#include "render/graphics_pipeline.hpp"
#include "render/renderer.hpp"

struct WindowDeleter {
  void operator()(SDL_Window* window) const noexcept {
    if (window != nullptr) {
      SDL_DestroyWindow(window);
    }
  }
};
using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;

class Application {
 private:
  WindowPtr window;
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<GraphicsPipeline> graphics_pipeline;
  std::unique_ptr<GameWorld> world;
  InputState input;

 public:
  Application();
  ~Application();

  void GameLoop();
};

#endif
