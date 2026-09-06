#include "launcher/launch_options.hpp"

#include <stdexcept>
#include <string_view>

namespace launcher {
void applyLaunchOptions(near_laugh::RuntimeConfig& config,
                        std::span<const std::filesystem::path> arguments,
                        const std::filesystem::path& working_directory) {
  auto candidate = config;
  bool level_seen = false, entry_seen = false;
  const auto usage = [] {
    throw std::invalid_argument(
        "Usage: game [--level <path>] [--entry <id>]; options need nonempty "
        "values and may appear once");
  };
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    const auto& option = arguments[i];
    if (option != "--level" && option != "--entry") usage();
    if (++i == arguments.size() || arguments[i].empty() ||
        arguments[i].native().find(std::filesystem::path::value_type{}) !=
            std::filesystem::path::string_type::npos)
      usage();
    const auto& value = arguments[i];
    if (option == "--level") {
      if (level_seen || value == "--level" || value == "--entry") usage();
      level_seen = true;
      candidate.level_path =
          std::filesystem::absolute(working_directory / value)
              .lexically_normal();
    } else {
      if (entry_seen) usage();
      entry_seen = true;
      const auto bytes = value.u8string();
      const std::string id(bytes.begin(), bytes.end());
      if (id.empty() || id.size() > 64 || id[0] < 'a' || id[0] > 'z') usage();
      for (char c : id)
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-')
          usage();
      candidate.entry_id = id;
    }
  }
  config = std::move(candidate);
}
}  // namespace launcher
