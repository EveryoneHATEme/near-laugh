#ifndef CORE_INPUT_INPUT_STATE_HPP
#define CORE_INPUT_INPUT_STATE_HPP

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>

class InputState {
 private:
  std::array<bool, SDL_SCANCODE_COUNT> keys{};
  std::array<bool, SDL_SCANCODE_COUNT> keys_pressed{};
  std::array<bool, 8> mouse_buttons{};
  std::array<bool, 8> mouse_buttons_pressed{};
  float mouse_delta_x{};
  float mouse_delta_y{};
  bool quit_requested{};

 public:
  void beginFrame();
  void handleEvent(const SDL_Event& event);

  bool isKeyDown(SDL_Scancode scancode) const;
  bool wasKeyPressed(SDL_Scancode scancode) const;
  bool isMouseButtonDown(uint8_t button) const;
  bool wasMouseButtonPressed(uint8_t button) const;

  float mouseDeltaX() const { return mouse_delta_x; }
  float mouseDeltaY() const { return mouse_delta_y; }
  bool quitRequested() const { return quit_requested; }
};

#endif
