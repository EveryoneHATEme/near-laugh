#ifndef CORE_WORLD_CHARACTERS_HPP
#define CORE_WORLD_CHARACTERS_HPP

#include "core/animation/character_catalog.hpp"
#include "core/world/level_document.hpp"

[[nodiscard]] const CharacterCatalogEntry* findCharacterModel(
    std::string_view id) noexcept;
[[nodiscard]] const CharacterMarkDefinition* findCharacterMark(
    const LevelCharacters& characters, std::string_view id) noexcept;
[[nodiscard]] const CharacterRouteDefinition* findCharacterRoute(
    const LevelCharacters& characters, std::string_view id) noexcept;
// Metadata only; selected asset contents are checked separately at preflight.
[[nodiscard]] std::vector<LevelDiagnostic> validateCharacterDefinitions(
    const LevelCharacters& characters, const LevelAudio& audio,
    const std::filesystem::path& source_path = {});

#endif
