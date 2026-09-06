#ifndef LAUNCHER_LAUNCH_OPTIONS_HPP
#define LAUNCHER_LAUNCH_OPTIONS_HPP

#include <span>

#include "near_laugh/runtime_config.hpp"

namespace launcher {
// Arguments exclude argv[0]. Paths stay in their host-native representation.
void applyLaunchOptions(near_laugh::RuntimeConfig& config,
                        std::span<const std::filesystem::path> arguments,
                        const std::filesystem::path& working_directory);
}  // namespace launcher
#endif
