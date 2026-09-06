#ifndef CORE_RENDER_SCENE_ASSETS_HPP
#define CORE_RENDER_SCENE_ASSETS_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/render/prototype_scene.hpp"
#include "core/render/scene_material.hpp"

struct SceneMaterialAsset {
  std::string id;
  SceneMaterialData data;
};
struct SceneBatchData {
  std::size_t material{};
  std::vector<PositionColorVertex> vertices;
};
struct PreparedSceneAssets {
  std::vector<SceneMaterialAsset> materials;
  std::vector<SceneBatchData> world;
  std::vector<SceneBatchData> props;
  std::optional<std::size_t> obstacle_material;
};

[[nodiscard]] std::filesystem::path sceneModelPath(
    const std::filesystem::path& root, std::string_view id);
[[nodiscard]] PreparedSceneAssets prepareSceneAssets(
    const std::filesystem::path& root, const LevelDocument& document);
[[nodiscard]] PreparedSceneAssets prepareSceneAssets(
    const std::filesystem::path& root, const PrototypeLevel& level);
void validateSceneAssets(const LevelDocument& document,
                         const std::filesystem::path& resource_root);

#endif
