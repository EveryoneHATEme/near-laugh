#include <exception>
#include <iostream>
#include <utility>
#include <vector>

#include "launcher/executable_path.hpp"
#include "launcher/launch_options.hpp"
#include "near_laugh/application.hpp"

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
  try {
    near_laugh::RuntimeConfig config;
    std::vector<std::filesystem::path> arguments;
    for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
    launcher::applyLaunchOptions(config, arguments,
                                 std::filesystem::current_path());
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
