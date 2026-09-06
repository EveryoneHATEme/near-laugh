#include "core/render/scene_assets.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "core/render/static_model_loader.hpp"
#include "core/world/scene_assets.hpp"

namespace {
SceneMaterialData structuralMaterial(const std::filesystem::path& root,
                                     std::string_view id) {
  std::string filename;
  bool nearest = false;
  if (id == "prototype-floor")
    filename = "prototype_floor.png";
  else if (id == "prototype-boundary")
    filename = "prototype_boundary.png";
  else if (id == "prototype-obstacle")
    filename = "prototype_obstacle.png";
  else if (id == "wood-floor") {
    filename = "apartment_wood_floor.png";
    nearest = true;
  } else if (id == "wallpaper") {
    filename = "apartment_wallpaper.png";
    nearest = true;
  } else
    throw std::runtime_error("Unknown structural material: " + std::string{id});
  SceneMaterialData result;
  try {
    result.image = decodePngRgba(root / "textures" / filename);
  } catch (const std::exception& e) {
    throw std::runtime_error("Material " + std::string{id} + ": " + e.what());
  }
  result.nearest = nearest;
  return result;
}

void appendChecked(std::vector<PositionColorVertex>& target,
                   std::span<const PositionColorVertex> vertices) {
  const auto maximum = std::min<std::size_t>(
      UINT32_MAX,
      std::numeric_limits<std::size_t>::max() / sizeof(PositionColorVertex));
  if (target.size() > maximum || vertices.size() > maximum - target.size())
    throw std::runtime_error(
        "Expanded scene vertices exceed allocation/draw limits");
  target.insert(target.end(), vertices.begin(), vertices.end());
}
}  // namespace

std::filesystem::path sceneModelPath(const std::filesystem::path& root,
                                     std::string_view id) {
  if (id == "prototype-chair") return root / "models" / "prototype_chair.glb";
  if (id == "apartment-chair") return root / "models" / "apartment_chair.glb";
  if (id == "apartment-table") return root / "models" / "apartment_table.glb";
  if (id == "apartment-phone") return root / "models" / "apartment_phone.glb";
  if (id == "apartment-radio") return root / "models" / "apartment_radio.glb";
  throw std::runtime_error("Unknown model identity: " + std::string{id});
}

PreparedSceneAssets prepareSceneAssets(const std::filesystem::path& root,
                                       const LevelDocument& document) {
  if (document.props.size() > level_maximum_prop_count ||
      document.solids.size() > level_maximum_solid_count ||
      document.doors.size() > level_maximum_door_count)
    throw std::runtime_error(
        "Scene object count exceeds the supported profile");
  PreparedSceneAssets result;
  const auto add_structural = [&](std::string_view id) {
    for (std::size_t i = 0; i < result.materials.size(); ++i)
      if (result.materials[i].id == id) return i;
    const auto index = result.materials.size();
    result.materials.push_back({std::string{id}, structuralMaterial(root, id)});
    return index;
  };
  const auto generated = buildPrototypeSceneVertices(
      document.terrain, document.solids, document.light_switch);
  const auto catalog = structuralMaterials();
  for (std::size_t role = 0; role < catalog.size(); ++role) {
    SceneBatchData batch;
    for (const auto& vertex : generated)
      if (vertex.texture_layer == role) batch.vertices.push_back(vertex);
    if (!batch.vertices.empty()) {
      batch.material = add_structural(catalog[role].id);
      result.world.push_back(std::move(batch));
    }
  }
  if (!document.doors.empty())
    result.obstacle_material = add_structural("prototype-obstacle");
  std::unordered_map<std::string, StaticModelData> models;
  std::unordered_map<std::string, std::size_t> batches;
  for (const auto& prop : document.props) {
    try {
      auto [it, inserted] = models.try_emplace(prop.model);
      if (inserted) {
        it->second = loadStaticModel(sceneModelPath(root, prop.model));
        std::size_t material;
        if (prop.model == "prototype-chair") {
          material = add_structural("prototype-obstacle");
        } else {
          material = result.materials.size();
          result.materials.push_back(
              {"model:" + prop.model, it->second.material});
        }
        batches.emplace(prop.model, result.props.size());
        result.props.push_back({material, {}});
      }
      const auto vertices = placeStaticModelVertices(it->second, prop);
      appendChecked(result.props[batches.at(prop.model)].vertices, vertices);
    } catch (const std::exception& e) {
      throw std::runtime_error("Prop " + prop.id + " model " + prop.model +
                               ": " + e.what());
    }
  }
  // An empty editor scene still needs a compatible descriptor layout.
  // The white fallback owns no file asset.
  if (result.materials.empty())
    result.materials.push_back({"constant-white", {}});
  std::size_t vertex_count = 0;
  for (const auto* batches : {&result.world, &result.props})
    for (const auto& batch : *batches) {
      if (batch.vertices.size() > (std::numeric_limits<std::size_t>::max() /
                                   sizeof(PositionColorVertex)) -
                                      vertex_count)
        throw std::runtime_error("Aggregate scene vertex bytes overflow");
      vertex_count += batch.vertices.size();
    }
  std::size_t image_bytes = 0;
  for (const auto& material : result.materials) {
    if (material.data.image.pixels.size() >
        std::numeric_limits<std::size_t>::max() - image_bytes)
      throw std::runtime_error("Aggregate scene image bytes overflow");
    image_bytes += material.data.image.pixels.size();
  }
  return result;
}

PreparedSceneAssets prepareSceneAssets(const std::filesystem::path& root,
                                       const PrototypeLevel& level) {
  LevelDocument document;
  document.terrain = level.terrain();
  document.solids = level.solids();
  document.props = level.props();
  document.light_switch = level.lightSwitch();
  document.doors = level.doors();
  return prepareSceneAssets(root, document);
}

void validateSceneAssets(const LevelDocument& document,
                         const std::filesystem::path& resource_root) {
  static_cast<void>(prepareSceneAssets(resource_root, document));
}
