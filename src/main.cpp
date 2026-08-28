#include <exception>
#include <iostream>
#include <utility>

#include "launcher/executable_path.hpp"
#include "near_laugh/application.hpp"

int main() {
  try {
    near_laugh::RuntimeConfig config;
    config.resource_root = launcher::executableResourceRoot();
    near_laugh::Application application(std::move(config));
    application.run();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "near-laugh startup/runtime failure: " << error.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "near-laugh startup/runtime failure: unknown exception\n";
    return 1;
  }
}
