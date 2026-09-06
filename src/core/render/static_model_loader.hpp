#ifndef CORE_RENDER_STATIC_MODEL_LOADER_HPP
#define CORE_RENDER_STATIC_MODEL_LOADER_HPP

#include <filesystem>
#include <vector>

#include "core/render/prototype_scene.hpp"
#include "core/render/scene_material.hpp"
#include "core/world/prototype_level.hpp"

struct StaticModelData {
  std::vector<PositionColorVertex> vertices{};
  SceneMaterialData material{};
  WorldPosition minimum{};
  WorldPosition maximum{};
};

[[nodiscard]] StaticModelData loadStaticModel(
    const std::filesystem::path& model_path);
[[nodiscard]] std::vector<PositionColorVertex> placeStaticModelVertices(
    const StaticModelData& model, const PrototypeStaticProp& placement);

[[nodiscard]] std::vector<PositionColorVertex> loadStaticModelVertices(
    const std::filesystem::path& model_path,
    const PrototypeStaticProp& placement);

#endif
