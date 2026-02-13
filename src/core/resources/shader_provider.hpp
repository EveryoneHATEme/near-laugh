#ifndef CORE_RENDER_RESOURCES_SHADER_PROVIDER_H
#define CORE_RENDER_RESOURCES_SHADER_PROVIDER_H

#include <filesystem>
#include <fstream>
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
    const size_t file_size = std::filesystem::file_size(path);

    std::ifstream file(path, std::ios::binary);
    if (!file) {
      throw std::runtime_error("ShaderProvider: failed to open the file");
    }
    std::vector<uint8_t> buffer(file_size);

    if (!file.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
      throw std::runtime_error("ShaderProvider: failed to read the file");
    }

    return buffer;
  }
};

#endif
