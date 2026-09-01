#ifndef CORE_RENDER_RESOURCES_SHADER_PROVIDER_H
#define CORE_RENDER_RESOURCES_SHADER_PROVIDER_H

#include <cstdint>
#include <filesystem>
#include <vector>

[[nodiscard]] std::vector<std::uint8_t> readBinaryFile(
    const std::filesystem::path& path);
[[nodiscard]] std::vector<std::uint32_t> readSpirvFile(
    const std::filesystem::path& path);

#endif
