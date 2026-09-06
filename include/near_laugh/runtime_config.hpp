#ifndef NEAR_LAUGH_RUNTIME_CONFIG_HPP
#define NEAR_LAUGH_RUNTIME_CONFIG_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace near_laugh {

struct RuntimeConfig {
  std::uint32_t window_width{1024};
  std::uint32_t window_height{768};
  std::string window_title{"near-laugh"};
  std::filesystem::path resource_root{};
  std::optional<std::filesystem::path> level_path{};
  std::optional<std::string> entry_id{};
};

}  // namespace near_laugh

#endif
