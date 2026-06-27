#include "application.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace {

std::filesystem::path shaderPath(const char* shader_file_name) {
#ifdef ENGINE_RESOURCE_DIR
  return std::filesystem::path(ENGINE_RESOURCE_DIR) / "shaders" /
         shader_file_name;
#else
  return std::filesystem::path("resources") / "shaders" / shader_file_name;
#endif
}

}  // namespace

Application::Application() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error("APPLICATION: SDL_Init failed");
  }

  SDL_Window* _window = SDL_CreateWindow("near-laugh FPS Prototype", 1280, 720,
                                         SDL_WINDOW_RESIZABLE);
  if (_window == nullptr) {
    throw std::runtime_error("APPLICATION: CreateWindow failed");
  }
  window.reset(_window);

  if (!SDL_SetWindowRelativeMouseMode(window.get(), true)) {
    SDL_Log("WARNING: Failed to enable relative mouse mode: %s", SDL_GetError());
  }

  renderer = std::make_unique<Renderer>(window.get());
  graphics_pipeline = std::make_unique<GraphicsPipeline>(
      renderer->getDevice(), renderer->getSwapchainFormat(),
      renderer->getDepthFormat(), shaderPath("triangle_vertex.spv"),
      shaderPath("triangle_fragment.spv"));
  world = std::make_unique<GameWorld>();
}

Application::~Application() {
  graphics_pipeline.reset();
  renderer.reset();
  if (window != nullptr) {
    SDL_SetWindowRelativeMouseMode(window.get(), false);
  }
  window.reset();
  SDL_Quit();
}

void Application::GameLoop() {
  bool run = true;
  auto last_frame_time = std::chrono::steady_clock::now();

  while (run) {
    input.beginFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      input.handleEvent(event);
    }

    if (input.quitRequested() || input.wasKeyPressed(SDL_SCANCODE_ESCAPE)) {
      run = false;
      break;
    }

    const auto current_time = std::chrono::steady_clock::now();
    const std::chrono::duration<float> elapsed = current_time - last_frame_time;
    last_frame_time = current_time;
    const float dt = std::clamp(elapsed.count(), 0.0f, 0.05f);

    world->update(dt, input);
    const RenderPacket packet = world->render(renderer->getAspectRatio());
    graphics_pipeline->upload(packet);

    FrameContext& frame_context = renderer->beginFrame();
    if (frame_context.swapchainTexture != nullptr) {
      SDL_GPURenderPass* render_pass =
          renderer->beginRenderPass(frame_context, 0.05f, 0.06f, 0.075f, 1.0f);
      graphics_pipeline->draw(render_pass);
      renderer->endRenderPass(render_pass);
    }
    renderer->endFrame(frame_context);
  }
}
