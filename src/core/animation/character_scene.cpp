#include "core/animation/character_scene.hpp"

#include <stdexcept>

#include "core/world/characters.hpp"

std::vector<std::shared_ptr<const CharacterAsset>> prepareCharacterAssets(
    const std::filesystem::path& root, const LevelCharacters& characters) {
  std::vector<std::shared_ptr<const CharacterAsset>> assets;
  for (const auto& actor : characters.actors) {
    try {
      const auto* model = findCharacterModel(actor.model);
      if (!model || !characterCatalogIsValid(*model))
        throw std::runtime_error("invalid model catalog profile");
      std::shared_ptr<const CharacterAsset> asset;
      for (std::size_t i = 0; i < assets.size(); ++i)
        if (characters.actors[i].model == actor.model) {
          asset = assets[i];
          break;
        }
      if (!asset) asset = loadCharacterAsset(root / model->resource);
      for (const auto id : {"idle", "walk", "interact"})
        (void)characterClip(*asset, id);
      assets.push_back(std::move(asset));
    } catch (const std::exception& error) {
      throw std::runtime_error("actor '" + actor.id + "', model '" +
                               actor.model + "': " + error.what());
    }
  }
  return assets;
}
