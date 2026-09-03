#ifndef NEAR_LAUGH_RUNTIME_CONFIG_HPP
#define NEAR_LAUGH_RUNTIME_CONFIG_HPP

#include <cstdint>
#include <filesystem>
#include <string>

namespace near_laugh {

struct RuntimeConfig {
  std::uint32_t window_width{1024};
  std::uint32_t window_height{768};
  std::string window_title{"near-laugh"};
  std::filesystem::path resource_root{};
};

}  // namespace near_laugh

#endif
