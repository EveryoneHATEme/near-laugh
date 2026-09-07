#ifndef CORE_WORLD_AUDIO_HPP
#define CORE_WORLD_AUDIO_HPP

#include <span>

#include "core/world/level_document.hpp"

struct AudioCatalogEntry {
  std::string_view id;
  std::string_view label;
};

[[nodiscard]] std::span<const AudioCatalogEntry> audioCatalog() noexcept;
[[nodiscard]] bool audioCatalogContains(std::string_view id) noexcept;
[[nodiscard]] const AudioCueDefinition* findAudioCue(
    const LevelAudio& audio, std::string_view id) noexcept;
[[nodiscard]] bool audioRoomBoundsAreSafe(
    const AudioRoomDefinition& room) noexcept;
[[nodiscard]] std::vector<LevelDiagnostic> validateLevelAudio(
    const LevelAudio& audio, std::span<const DoorDefinition> doors,
    const std::filesystem::path& source_path = {});

#endif
