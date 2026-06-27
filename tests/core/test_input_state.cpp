#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include "src/core/input/input_state.hpp"

TEST(InputStateTest, TracksPressedAndHeldKeys) {
  InputState input;
  input.beginFrame();

  SDL_Event key_down{};
  key_down.type = SDL_EVENT_KEY_DOWN;
  key_down.key.scancode = SDL_SCANCODE_W;
  input.handleEvent(key_down);

  EXPECT_TRUE(input.isKeyDown(SDL_SCANCODE_W));
  EXPECT_TRUE(input.wasKeyPressed(SDL_SCANCODE_W));

  input.beginFrame();
  EXPECT_TRUE(input.isKeyDown(SDL_SCANCODE_W));
  EXPECT_FALSE(input.wasKeyPressed(SDL_SCANCODE_W));

  SDL_Event key_up{};
  key_up.type = SDL_EVENT_KEY_UP;
  key_up.key.scancode = SDL_SCANCODE_W;
  input.handleEvent(key_up);

  EXPECT_FALSE(input.isKeyDown(SDL_SCANCODE_W));
}

TEST(InputStateTest, TracksMouseMotionAndButtonPress) {
  InputState input;
  input.beginFrame();

  SDL_Event motion{};
  motion.type = SDL_EVENT_MOUSE_MOTION;
  motion.motion.xrel = 3.0f;
  motion.motion.yrel = -2.0f;
  input.handleEvent(motion);

  SDL_Event button{};
  button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  button.button.button = SDL_BUTTON_LEFT;
  input.handleEvent(button);

  EXPECT_FLOAT_EQ(input.mouseDeltaX(), 3.0f);
  EXPECT_FLOAT_EQ(input.mouseDeltaY(), -2.0f);
  EXPECT_TRUE(input.isMouseButtonDown(SDL_BUTTON_LEFT));
  EXPECT_TRUE(input.wasMouseButtonPressed(SDL_BUTTON_LEFT));

  input.beginFrame();
  EXPECT_FLOAT_EQ(input.mouseDeltaX(), 0.0f);
  EXPECT_FLOAT_EQ(input.mouseDeltaY(), 0.0f);
  EXPECT_TRUE(input.isMouseButtonDown(SDL_BUTTON_LEFT));
  EXPECT_FALSE(input.wasMouseButtonPressed(SDL_BUTTON_LEFT));
}
