#include <exception>
#include <iostream>

#include "core/application.hpp"

int main() {
  try {
    Application application;
    application.gameLoop();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "near-laugh startup/runtime failure: " << error.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "near-laugh startup/runtime failure: unknown exception\n";
    return 1;
  }
}
