#include "core/world/scene_assets.hpp"

#include <array>
#include <cmath>
#include <numbers>

namespace {
constexpr std::array<PropCollisionBox, 1> legacy_boxes{
    {{{0, .91F, 0}, {.55F, .91F, .48F}}}};
constexpr std::array<PropCollisionBox, 6> chair_boxes{{
    {{0, .46F, -.04F}, {.31F, .06F, .34F}},
    {{0, .91F, -.36F}, {.31F, .285F, .055F}},
    {{-.25F, .20F, -.25F}, {.045F, .20F, .045F}},
    {{.25F, .20F, -.25F}, {.045F, .20F, .045F}},
    {{-.25F, .20F, .25F}, {.045F, .20F, .045F}},
    {{.25F, .20F, .25F}, {.045F, .20F, .045F}},
}};
constexpr std::array<PropCollisionBox, 5> table_boxes{{
    {{0, .69F, 0}, {.837F, .031F, .548F}},
    {{-.72F, .33F, -.43F}, {.045F, .33F, .045F}},
    {{.72F, .33F, -.43F}, {.045F, .33F, .045F}},
    {{-.72F, .33F, .43F}, {.045F, .33F, .045F}},
    {{.72F, .33F, .43F}, {.045F, .33F, .045F}},
}};
constexpr std::array<SceneModel, 5> models{{
    {"prototype-chair",
     "Prototype chair",
     {-.55F, 0, -.48F},
     {.55F, 1.82F, .48F},
     legacy_boxes},
    {"apartment-chair",
     "Apartment chair",
     {-.310980F, 0, -.427899F},
     {.310984F, 1.196155F, .341962F},
     chair_boxes},
    {"apartment-table",
     "Apartment table",
     {-.836348F, 0, -.547953F},
     {.836348F, .720339F, .547951F},
     table_boxes},
    {"apartment-phone",
     "Telephone",
     {-.192647F, 0, -.178485F},
     {.104292F, .079434F, .211183F},
     {}},
    {"apartment-radio",
     "Radio",
     {-.136818F, -.000001F, -.084074F},
     {.247568F, .231443F, .142138F},
     {}},
}};
constexpr std::array<StructuralMaterial, 5> materials{{
    {"prototype-floor", "Prototype floor"},
    {"prototype-boundary", "Prototype boundary"},
    {"prototype-obstacle", "Prototype obstacle"},
    {"wood-floor", "Wood floor"},
    {"wallpaper", "Wallpaper"},
}};
}  // namespace

std::span<const SceneModel> sceneModels() noexcept { return models; }
const SceneModel* findSceneModel(std::string_view id) noexcept {
  for (const auto& model : models)
    if (model.id == id) return &model;
  return nullptr;
}
std::span<const StructuralMaterial> structuralMaterials() noexcept {
  return materials;
}
const StructuralMaterial* findStructuralMaterial(std::string_view id) noexcept {
  for (const auto& material : materials)
    if (material.id == id) return &material;
  return nullptr;
}
WorldPosition propBoxWorldCenter(const PrototypeStaticProp& prop,
                                 const PropCollisionBox& box) noexcept {
  const double yaw =
      static_cast<double>(prop.yaw_degrees) * std::numbers::pi / 180;
  const double x = static_cast<double>(box.center.x) * prop.uniform_scale;
  const double z = static_cast<double>(box.center.z) * prop.uniform_scale;
  return {static_cast<float>(prop.translation.x + std::cos(yaw) * x +
                             std::sin(yaw) * z),
          static_cast<float>(prop.translation.y +
                             static_cast<double>(box.center.y) *
                                 prop.uniform_scale),
          static_cast<float>(prop.translation.z - std::sin(yaw) * x +
                             std::cos(yaw) * z)};
}
WorldExtent propBoxWorldHalfExtent(const PrototypeStaticProp& prop,
                                   const PropCollisionBox& box) noexcept {
  return {box.half_extent.x * prop.uniform_scale,
          box.half_extent.y * prop.uniform_scale,
          box.half_extent.z * prop.uniform_scale};
}
PropCollisionBox sceneModelBounds(const SceneModel& model) noexcept {
  return {{(model.bounds_min.x + model.bounds_max.x) * .5F,
           (model.bounds_min.y + model.bounds_max.y) * .5F,
           (model.bounds_min.z + model.bounds_max.z) * .5F},
          {(model.bounds_max.x - model.bounds_min.x) * .5F,
           (model.bounds_max.y - model.bounds_min.y) * .5F,
           (model.bounds_max.z - model.bounds_min.z) * .5F}};
}
