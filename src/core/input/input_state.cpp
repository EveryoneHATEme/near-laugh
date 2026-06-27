#include "input_state.hpp"

#include <algorithm>

void InputState::beginFrame() {
  std::fill(keys_pressed.begin(), keys_pressed.end(), false);
  std::fill(mouse_buttons_pressed.begin(), mouse_buttons_pressed.end(), false);
  mouse_delta_x = 0.0f;
  mouse_delta_y = 0.0f;
  quit_requested = false;
}

void InputState::handleEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_QUIT:
      quit_requested = true;
      break;
    case SDL_EVENT_KEY_DOWN: {
      const SDL_Scancode scancode = event.key.scancode;
      if (scancode >= 0 && scancode < SDL_SCANCODE_COUNT) {
        if (!keys[scancode] && !event.key.repeat) {
          keys_pressed[scancode] = true;
        }
        keys[scancode] = true;
      }
      break;
    }
    case SDL_EVENT_KEY_UP: {
      const SDL_Scancode scancode = event.key.scancode;
      if (scancode >= 0 && scancode < SDL_SCANCODE_COUNT) {
        keys[scancode] = false;
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION:
      mouse_delta_x += event.motion.xrel;
      mouse_delta_y += event.motion.yrel;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      const uint8_t button = event.button.button;
      if (button < mouse_buttons.size()) {
        if (!mouse_buttons[button]) {
          mouse_buttons_pressed[button] = true;
        }
        mouse_buttons[button] = true;
      }
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      const uint8_t button = event.button.button;
      if (button < mouse_buttons.size()) {
        mouse_buttons[button] = false;
      }
      break;
    }
    default:
      break;
  }
}

bool InputState::isKeyDown(SDL_Scancode scancode) const {
  return scancode >= 0 && scancode < SDL_SCANCODE_COUNT && keys[scancode];
}

bool InputState::wasKeyPressed(SDL_Scancode scancode) const {
  return scancode >= 0 && scancode < SDL_SCANCODE_COUNT &&
         keys_pressed[scancode];
}

bool InputState::isMouseButtonDown(uint8_t button) const {
  return button < mouse_buttons.size() && mouse_buttons[button];
}

bool InputState::wasMouseButtonPressed(uint8_t button) const {
  return button < mouse_buttons_pressed.size() && mouse_buttons_pressed[button];
}
