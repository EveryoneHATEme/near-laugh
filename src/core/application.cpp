#include "application.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <stdexcept>

Application::Application() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("ERROR: Failed to initialize SDL: %s", SDL_GetError());
  } else {
    SDL_Log("SDL initialized successfully");
  }

  SDL_Window* _window = SDL_CreateWindow("window", 1024, 768, 0);
  if (_window == nullptr) {
    throw std::runtime_error("APPLICATION: CreateWindow failed");
  }
  window.reset(_window);

  renderer = std::make_unique<Renderer>(window.get());
  graphics_pipeline = std::make_unique<GraphicsPipeline>(
      renderer->getDevice(), renderer->getSwapchainFormat(),
      std::filesystem::path("resources/shaders/triangle_vertex.spv"),
      std::filesystem::path("resources/shaders/triangle_fragment.spv"));
}

Application::~Application() {}

void Application::GameLoop() {
  bool run = true;

  while (run) {
    auto& frame_context = renderer->beginFrame();
    SDL_GPURenderPass* render_pass =
        renderer->beginRenderPass(frame_context, 0, 0, 0, 1);
    graphics_pipeline->draw(render_pass);
    renderer->endRenderPass(render_pass);
    renderer->endFrame(frame_context);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        run = false;
        break;
      }
    }
  }
}
