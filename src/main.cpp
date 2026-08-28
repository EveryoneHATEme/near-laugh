#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "near_laugh/application.hpp"

int main(int argc, char** argv) {
  try {
    if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
      throw std::runtime_error("Executable path is unavailable");
    }
    const std::filesystem::path executable =
        std::filesystem::absolute(argv[0]).lexically_normal();
    near_laugh::RuntimeConfig config;
    config.resource_root = executable.parent_path() / "resources";
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
