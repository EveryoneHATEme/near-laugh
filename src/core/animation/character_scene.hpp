#ifndef CORE_ANIMATION_CHARACTER_SCENE_HPP
#define CORE_ANIMATION_CHARACTER_SCENE_HPP

#include "core/animation/character_animation.hpp"
#include "core/world/level_document.hpp"

// Selected actors in authored order; repeated catalog models share one asset.
// Also used by editor initial presentation and Play preflight, without physics.
[[nodiscard]] std::vector<std::shared_ptr<const CharacterAsset>>
prepareCharacterAssets(const std::filesystem::path& root,
                       const LevelCharacters& characters);

#endif
