#ifndef EDITOR_EDITOR_CHARACTER_SPATIAL_HPP
#define EDITOR_EDITOR_CHARACTER_SPATIAL_HPP

#include "core/world/characters.hpp"

inline constexpr float editor_character_route_radius = 0.12F;
inline constexpr float editor_character_line_radius = 0.045F;

struct EditorCharacterBounds {
  WorldPosition center{};
  WorldExtent half_extent{};
  float yaw_degrees{};
};

[[nodiscard]] bool editorFiniteCharacterMark(
    const CharacterMarkDefinition& mark);
[[nodiscard]] EditorCharacterBounds editorCharacterVisualBounds(
    const CharacterCatalogEntry& model, const CharacterMarkDefinition& mark);
[[nodiscard]] WorldPosition editorCharacterMarkHandle(
    const CharacterMarkDefinition& mark);
[[nodiscard]] WorldPosition editorCharacterFacingTip(
    const CharacterMarkDefinition& mark);
[[nodiscard]] WorldPosition editorCharacterRouteHandle(
    const CharacterMarkDefinition& mark);
[[nodiscard]] WorldPosition editorCharacterDiagnosticHandle(
    const CharacterMarkDefinition& mark);

#endif
