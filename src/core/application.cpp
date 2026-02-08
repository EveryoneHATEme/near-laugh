#include "application.hpp"

#include <SDL3/SDL.h>

#include <iostream>

Application::Application() { std::cout << SDL_abs(-123) << std::endl; }

Application::~Application() {}
