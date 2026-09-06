#ifndef CORE_WORLD_SCENE_ASSETS_HPP
#define CORE_WORLD_SCENE_ASSETS_HPP

#include <span>
#include <string_view>

#include "core/world/level_document.hpp"

struct SceneModel {
  std::string_view id;
  std::string_view label;
  WorldPosition bounds_min;
  WorldPosition bounds_max;
  std::span<const PropCollisionBox> default_boxes;
};

struct StructuralMaterial {
  std::string_view id;
  std::string_view label;
};

[[nodiscard]] std::span<const SceneModel> sceneModels() noexcept;
[[nodiscard]] const SceneModel* findSceneModel(std::string_view id) noexcept;
[[nodiscard]] std::span<const StructuralMaterial>
structuralMaterials() noexcept;
[[nodiscard]] const StructuralMaterial* findStructuralMaterial(
    std::string_view id) noexcept;
[[nodiscard]] WorldPosition propBoxWorldCenter(
    const PrototypeStaticProp& prop, const PropCollisionBox& box) noexcept;
[[nodiscard]] WorldExtent propBoxWorldHalfExtent(
    const PrototypeStaticProp& prop, const PropCollisionBox& box) noexcept;
[[nodiscard]] PropCollisionBox sceneModelBounds(
    const SceneModel& model) noexcept;

#endif
