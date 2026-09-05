#include "editor/editor_picking.hpp"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>
#include <limits>
#include <numbers>

#include "core/world/prototype_level.hpp"

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
  const auto& prop = level.static_prop;
  const double yaw =
      static_cast<double>(prop.yaw_degrees) * std::numbers::pi / 180.0;
  const auto inverseYaw = [&](glm::dvec3 v) {
    return glm::dvec3{std::cos(yaw) * v.x - std::sin(yaw) * v.z, v.y,
                      std::sin(yaw) * v.x + std::cos(yaw) * v.z};
  };
  consider(editor_prop,
           boxHit(inverseYaw(vec(ray.origin) -
                             vec(prototypeStaticPropProxyWorldCenter(prop))),
                  inverseYaw(vec(ray.direction)),
                  prototypeStaticPropProxyWorldHalfExtent(prop)));
  consider(editor_spawn, sphereHit(ray, editorSpawnMarker(level.player_spawn)));
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
  for (std::size_t z = 0; z < prototype_terrain_cell_count; ++z) {
    for (std::size_t x = 0; x < prototype_terrain_cell_count; ++x) {
      const auto a = prototypeTerrainSamplePosition(terrain, x, z);
      const auto b = prototypeTerrainSamplePosition(terrain, x, z + 1);
      const auto c = prototypeTerrainSamplePosition(terrain, x + 1, z + 1);
      const auto d = prototypeTerrainSamplePosition(terrain, x + 1, z);
      for (double t : {triangleHit(ray, a, b, c), triangleHit(ray, a, c, d)}) {
        if (t > 0 && t < nearest) nearest = t;
      }
    }
  }
  if (!std::isfinite(nearest)) return std::nullopt;
  return EditorTerrainHit{
      position(vec(ray.origin) + nearest * vec(ray.direction)), nearest};
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
  if (!document.object(document.selection())) return std::nullopt;
  const auto hit = pickEditorTerrain(document.document()->terrain, *ray);
  if (!hit) return std::nullopt;
  if (pressed) static_cast<void>(document.placeSelected(hit->position));
  return hit->position;
}
