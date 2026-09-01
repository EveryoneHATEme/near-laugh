#include "core/resources/shader_provider.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    throw std::runtime_error("Failed to open binary asset: " + path.string());
  }

  const std::streampos end = file.tellg();
  if (end < 0) {
    throw std::runtime_error("Failed to determine binary asset size: " +
                             path.string());
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
  file.seekg(0, std::ios::beg);
  if (!bytes.empty() &&
      !file.read(reinterpret_cast<char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()))) {
    throw std::runtime_error("Failed to read binary asset: " + path.string());
  }
  return bytes;
}

std::vector<std::uint32_t> readSpirvFile(const std::filesystem::path& path) {
  const std::vector<std::uint8_t> bytes = readBinaryFile(path);
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0) {
    throw std::runtime_error(
        "SPIR-V asset size must be a non-zero multiple of four bytes: " +
        path.string());
  }

  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}
