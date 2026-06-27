#ifndef CORE_RENDER_RESOURCES_SHADER_PROVIDER_H
#define CORE_RENDER_RESOURCES_SHADER_PROVIDER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

class ShaderProvider {
  // singleton for now

 private:
  ShaderProvider() = default;

 public:
  static ShaderProvider& get() {
    static ShaderProvider instance;
    return instance;
  }

  std::vector<uint8_t> readShader(const std::filesystem::path& path) const {
    std::error_code error;
    std::filesystem::path absolute_path =
        std::filesystem::absolute(path, error);
    if (error) {
      absolute_path = path;
      error.clear();
    }

    if (!std::filesystem::exists(path, error)) {
      throw std::runtime_error("ShaderProvider: file does not exist: " +
                               absolute_path.string());
    }

    const uintmax_t file_size = std::filesystem::file_size(path, error);
    if (error) {
      throw std::runtime_error("ShaderProvider: failed to query file size: " +
                               absolute_path.string());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
      throw std::runtime_error("ShaderProvider: failed to open the file: " +
                               absolute_path.string());
    }
    std::vector<uint8_t> buffer(static_cast<std::size_t>(file_size));

    if (!file.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
      throw std::runtime_error("ShaderProvider: failed to read the file: " +
                               absolute_path.string());
    }

    return buffer;
  }
};

#endif
