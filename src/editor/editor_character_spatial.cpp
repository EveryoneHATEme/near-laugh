#include "editor/editor_character_spatial.hpp"

#include <cmath>
#include <numbers>

#include "editor/editor_picking.hpp"

bool editorFiniteCharacterMark(const CharacterMarkDefinition& mark) {
  return std::isfinite(mark.feet_position.x) &&
         std::isfinite(mark.feet_position.y) &&
         std::isfinite(mark.feet_position.z) && std::isfinite(mark.yaw_degrees);
}

EditorCharacterBounds editorCharacterVisualBounds(
    const CharacterCatalogEntry& model, const CharacterMarkDefinition& mark) {
  const double yaw = mark.yaw_degrees * std::numbers::pi / 180.0;
  const double x = (model.preview_min[0] + model.preview_max[0]) * 0.5 -
                   model.feet_origin[0];
  const double z = (model.preview_min[2] + model.preview_max[2]) * 0.5 -
                   model.feet_origin[2];
  return {{mark.feet_position.x +
               static_cast<float>(std::cos(yaw) * x + std::sin(yaw) * z),
           mark.feet_position.y +
               (model.preview_min[1] + model.preview_max[1]) * 0.5F -
               model.feet_origin[1],
           mark.feet_position.z +
               static_cast<float>(-std::sin(yaw) * x + std::cos(yaw) * z)},
          {(model.preview_max[0] - model.preview_min[0]) * 0.5F,
           (model.preview_max[1] - model.preview_min[1]) * 0.5F,
           (model.preview_max[2] - model.preview_min[2]) * 0.5F},
          mark.yaw_degrees};
}

WorldPosition editorCharacterMarkHandle(const CharacterMarkDefinition& mark) {
  auto point = mark.feet_position;
  point.y += editor_marker_radius;
  return point;
}

WorldPosition editorCharacterFacingTip(const CharacterMarkDefinition& mark) {
  auto point = editorCharacterMarkHandle(mark);
  const double yaw = mark.yaw_degrees * std::numbers::pi / 180.0;
  point.x += 1.45F * static_cast<float>(std::sin(yaw));
  point.z += 1.45F * static_cast<float>(std::cos(yaw));
  return point;
}

WorldPosition editorCharacterRouteHandle(const CharacterMarkDefinition& mark) {
  auto point = mark.feet_position;
  point.y += 0.65F;
  return point;
}

WorldPosition editorCharacterDiagnosticHandle(
    const CharacterMarkDefinition& mark) {
  auto point = mark.feet_position;
  point.y += 1.1F;
  return point;
}
