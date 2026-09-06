#include "editor/editor_picking.hpp"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>
#include <limits>
#include <numbers>

#include "core/world/door.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"

namespace {
glm::dvec3 vec(WorldPosition p) { return {p.x, p.y, p.z}; }
WorldPosition position(glm::dvec3 p) {
  return {static_cast<float>(p.x), static_cast<float>(p.y),
          static_cast<float>(p.z)};
}
bool finite(glm::dvec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool validRay(const EditorRay& ray) {
  return finite(vec(ray.origin)) && finite(vec(ray.direction)) &&
         glm::dot(vec(ray.direction), vec(ray.direction)) > 0.0;
}
double boxHit(glm::dvec3 origin, glm::dvec3 direction, WorldExtent extent) {
  const glm::dvec3 half{extent.x, extent.y, extent.z};
  double enter = -std::numeric_limits<double>::infinity();
  double leave = std::numeric_limits<double>::infinity();
  for (int axis = 0; axis < 3; ++axis) {
    if (std::abs(direction[axis]) < 1e-12) {
      if (origin[axis] < -half[axis] || origin[axis] > half[axis]) return -1;
      continue;
    }
    double a = (-half[axis] - origin[axis]) / direction[axis];
    double b = (half[axis] - origin[axis]) / direction[axis];
    if (a > b) std::swap(a, b);
    enter = std::max(enter, a);
    leave = std::min(leave, b);
    if (enter > leave) return -1;
  }
  return enter > 0 ? enter : leave;
}
double sphereHit(const EditorRay& ray, WorldPosition center) {
  const glm::dvec3 relative = vec(ray.origin) - vec(center);
  const glm::dvec3 direction = vec(ray.direction);
  const double a = glm::dot(direction, direction);
  const double b = glm::dot(relative, direction);
  const double c = glm::dot(relative, relative) -
                   editor_marker_radius * editor_marker_radius;
  const double discriminant = b * b - a * c;
  if (discriminant < 0) return -1;
  const double first = (-b - std::sqrt(discriminant)) / a;
  return first > 0 ? first : (-b + std::sqrt(discriminant)) / a;
}
double triangleHit(const EditorRay& ray, WorldPosition a, WorldPosition b,
                   WorldPosition c) {
  const glm::dvec3 edge1 = vec(b) - vec(a);
  const glm::dvec3 edge2 = vec(c) - vec(a);
  const glm::dvec3 p = glm::cross(vec(ray.direction), edge2);
  const double determinant = glm::dot(edge1, p);
  if (std::abs(determinant) < 1e-12) return -1;
  const glm::dvec3 relative = vec(ray.origin) - vec(a);
  const double u = glm::dot(relative, p) / determinant;
  if (u < -1e-9 || u > 1 + 1e-9) return -1;
  const glm::dvec3 q = glm::cross(relative, edge1);
  const double v = glm::dot(vec(ray.direction), q) / determinant;
  if (v < -1e-9 || u + v > 1 + 1e-9) return -1;
  return glm::dot(edge2, q) / determinant;
}
}  // namespace

std::optional<EditorRay> editorPointerRay(const CameraFrame& camera, double x,
                                          double y, double width,
                                          double height) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
      !std::isfinite(height) || width <= 0 || height <= 0 || x < 0 || y < 0 ||
      x > width || y > height)
    return std::nullopt;
  const glm::dmat4 matrix{glm::make_mat4(camera.view_projection.data())};
  const double determinant = glm::determinant(matrix);
  if (!std::isfinite(determinant) || std::abs(determinant) < 1e-15)
    return std::nullopt;
  const glm::dmat4 inverse = glm::inverse(matrix);
  const double nx = 2 * x / width - 1;
  const double ny = 2 * y / height - 1;
  const glm::dvec4 near = inverse * glm::dvec4{nx, ny, 0, 1};
  const glm::dvec4 far = inverse * glm::dvec4{nx, ny, 1, 1};
  if (near.w == 0 || far.w == 0) return std::nullopt;
  const glm::dvec3 origin = glm::dvec3(near) / near.w;
  const glm::dvec3 delta = glm::dvec3(far) / far.w - origin;
  if (!finite(origin) || !finite(delta) || glm::length(delta) <= 0)
    return std::nullopt;
  return EditorRay{position(origin), position(glm::normalize(delta))};
}

WorldPosition editorSpawnMarker(const PrototypePlayerSpawn& spawn) {
  auto p = spawn.foot_position;
  p.y += editor_marker_radius;
  return p;
}

EditorObjectId pickEditorObject(const EditorDocument& document,
                                const EditorRay& ray) {
  if (!document.document() || !validRay(ray)) return editor_no_object;
  const LevelDocument& level = *document.document();
  double nearest = std::numeric_limits<double>::infinity();
  EditorObjectId selected = editor_no_object;
  const auto consider = [&](EditorObjectId id, double distance) {
    if (distance > 0 && distance < nearest) {
      nearest = distance;
      selected = id;
    }
  };
  for (std::size_t i = 0; i < level.solids.size(); ++i) {
    const auto& solid = level.solids[i];
    consider(document.solidIds()[i],
             boxHit(vec(ray.origin) - vec(solid.center), vec(ray.direction),
                    solid.half_extent));
  }
  for (std::size_t i = 0; i < level.props.size(); ++i) {
    const auto& prop = level.props[i];
    const double yaw =
        static_cast<double>(prop.yaw_degrees) * std::numbers::pi / 180.0;
    const auto inverseYaw = [&](glm::dvec3 v) {
      return glm::dvec3{std::cos(yaw) * v.x - std::sin(yaw) * v.z, v.y,
                        std::sin(yaw) * v.x + std::cos(yaw) * v.z};
    };
    if (const auto* model = findSceneModel(prop.model)) {
      const auto bounds = sceneModelBounds(*model);
      consider(document.propIds()[i],
               boxHit(inverseYaw(vec(ray.origin) -
                                 vec(propBoxWorldCenter(prop, bounds))),
                      inverseYaw(vec(ray.direction)),
                      propBoxWorldHalfExtent(prop, bounds)));
    } else {
      consider(document.propIds()[i], sphereHit(ray, prop.translation));
      for (const auto& bounds : prop.collision_boxes)
        consider(document.propIds()[i],
                 boxHit(inverseYaw(vec(ray.origin) -
                                   vec(propBoxWorldCenter(prop, bounds))),
                        inverseYaw(vec(ray.direction)),
                        propBoxWorldHalfExtent(prop, bounds)));
    }
  }
  for (std::size_t i = 0; i < level.doors.size(); ++i) {
    const auto& door = level.doors[i];
    if (const auto hit = doorRayDistance(door, doorInitialAngle(door),
                                         ray.origin, ray.direction))
      consider(document.doorIds()[i], *hit / glm::length(vec(ray.direction)));
  }
  for (std::size_t i = 0; i < level.entries.size(); ++i)
    consider(document.entryIds()[i],
             sphereHit(ray, editorSpawnMarker(level.entries[i].pose)));
  if (level.light_switch) {
    if (const auto hit = lightSwitchRayDistance(*level.light_switch, ray.origin,
                                                ray.direction)) {
      // Other editor intersections return the ray parameter, not metres.
      consider(editor_light_switch, *hit / glm::length(vec(ray.direction)));
    }
  }
  for (std::size_t i = 0; i < level.environment_light.point_lights.size();
       ++i) {
    consider(editor_first_light + i,
             sphereHit(ray, level.environment_light.point_lights[i].position));
  }
  return selected;
}

std::optional<EditorTerrainHit> pickEditorTerrain(
    const PrototypeTerrain& terrain, const EditorRay& ray) {
  if (!validRay(ray)) return std::nullopt;
  double nearest = std::numeric_limits<double>::infinity();
  WorldPosition normal{};
  for (std::size_t z = 0; z < prototype_terrain_cell_count; ++z) {
    for (std::size_t x = 0; x < prototype_terrain_cell_count; ++x) {
      const auto a = prototypeTerrainSamplePosition(terrain, x, z);
      const auto b = prototypeTerrainSamplePosition(terrain, x, z + 1);
      const auto c = prototypeTerrainSamplePosition(terrain, x + 1, z + 1);
      const auto d = prototypeTerrainSamplePosition(terrain, x + 1, z);
      for (const auto& triangle : {std::array<WorldPosition, 3>{a, b, c},
                                   std::array<WorldPosition, 3>{a, c, d}}) {
        const double t =
            triangleHit(ray, triangle[0], triangle[1], triangle[2]);
        if (t > 0 && t < nearest) {
          nearest = t;
          normal = position(
              glm::normalize(glm::cross(vec(triangle[1]) - vec(triangle[0]),
                                        vec(triangle[2]) - vec(triangle[0]))));
        }
      }
    }
  }
  if (!std::isfinite(nearest)) return std::nullopt;
  return EditorTerrainHit{
      position(vec(ray.origin) + nearest * vec(ray.direction)), nearest,
      normal};
}

std::optional<EditorSurfaceHit> pickEditorSurface(
    const EditorDocument& document, const EditorRay& ray,
    EditorPlacementMode mode) {
  if (!document.document() || !validRay(ray)) return std::nullopt;
  const auto& level = *document.document();
  std::optional<EditorSurfaceHit> nearest;
  // Solid array order wins equal-distance ties, with terrain considered last.
  if (mode == EditorPlacementMode::SceneSurfaces) {
    for (std::size_t i = 0; i < level.solids.size(); ++i) {
      const auto id = document.solidIds()[i];
      if (id == document.selection()) continue;
      const auto& solid = level.solids[i];
      const auto relative = vec(ray.origin) - vec(solid.center);
      const auto direction = vec(ray.direction);
      const double t = boxHit(relative, direction, solid.half_extent);
      if (!(t > 0) || (nearest && t >= nearest->distance)) continue;
      const auto local = relative + direction * t;
      const glm::dvec3 extent{solid.half_extent.x, solid.half_extent.y,
                              solid.half_extent.z};
      int axis = 0;
      for (int j = 1; j < 3; ++j)
        if (std::abs(std::abs(local[j]) - extent[j]) <
            std::abs(std::abs(local[axis]) - extent[axis]))
          axis = j;
      glm::dvec3 normal{0};
      normal[axis] = local[axis] < 0 ? -1 : 1;
      const auto face = axis == 1 ? (normal.y > 0 ? EditorSurfaceFace::Top
                                                  : EditorSurfaceFace::Bottom)
                        : axis == 0
                            ? (normal.x > 0 ? EditorSurfaceFace::PositiveX
                                            : EditorSurfaceFace::NegativeX)
                            : (normal.z > 0 ? EditorSurfaceFace::PositiveZ
                                            : EditorSurfaceFace::NegativeZ);
      nearest = EditorSurfaceHit{position(vec(ray.origin) + t * direction),
                                 position(normal), t, id, face};
    }
  }
  if (level.terrain) {
    if (const auto hit = pickEditorTerrain(*level.terrain, ray);
        hit && (!nearest || hit->distance < nearest->distance)) {
      nearest = EditorSurfaceHit{hit->position, hit->normal, hit->distance,
                                 editor_no_object, EditorSurfaceFace::Terrain};
    }
  }
  return nearest;
}

std::optional<WorldPosition> updateEditorViewport(
    EditorDocument& document, const std::optional<EditorRay>& ray,
    bool pointer_owned, bool navigation_active, bool pressed, bool placing) {
  if (!document.document() || !ray || pointer_owned || navigation_active)
    return std::nullopt;
  if (!placing) {
    if (pressed) document.select(pickEditorObject(document, *ray));
    return std::nullopt;
  }
  if (!document.object(document.selection()) || !document.document()->terrain)
    return std::nullopt;
  const auto hit = pickEditorTerrain(*document.document()->terrain, *ray);
  if (!hit) return std::nullopt;
  if (pressed) static_cast<void>(document.placeSelected(hit->position));
  return hit->position;
}

std::optional<EditorSurfaceHit> updateEditorPlacementViewport(
    EditorDocument& document, const std::optional<EditorRay>& ray,
    bool pointer_owned, bool navigation_active, bool pressed,
    EditorPlacementMode mode, const EditorPlacementOffsets& offsets) {
  if (!ray || pointer_owned || navigation_active) return std::nullopt;
  const auto selected = document.object(document.selection());
  if (!selected) return std::nullopt;
  const auto hit = pickEditorSurface(document, *ray, mode);
  if (hit && pressed) {
    if (auto placed = editorPlacedObject(*selected, *hit, offsets))
      static_cast<void>(
          document.replaceObject(document.selection(), std::move(*placed)));
  }
  return hit;
}

std::optional<WorldPosition> updateEditorTerrainViewport(
    EditorDocument& document, const std::optional<EditorRay>& ray,
    bool pointer_owned, bool navigation_active, bool pressed, bool down,
    bool pointer_moved) {
  if (!document.document() || !document.document()->terrain)
    return std::nullopt;
  if (pointer_owned || navigation_active) {
    // A press begun over UI cannot become a stroke by dragging out of the
    // panel.
    static_cast<void>(document.finishTerrainStroke());
    return std::nullopt;
  }
  const auto intersection =
      ray ? pickEditorTerrain(*document.document()->terrain, *ray)
          : std::nullopt;
  const std::optional<WorldPosition> hit =
      intersection ? std::optional<WorldPosition>{intersection->position}
                   : std::nullopt;
  if (pressed)
    document.beginTerrainStroke(hit);
  else if (document.terrainStrokeActive() && pointer_moved)
    document.extendTerrainStroke(hit);
  if (!down) static_cast<void>(document.finishTerrainStroke());
  return hit;
}
