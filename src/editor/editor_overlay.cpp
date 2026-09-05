#include "editor/editor_overlay.hpp"

#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <numbers>

#include "core/world/prototype_level.hpp"
#include "editor/editor_picking.hpp"

std::optional<EditorOverlayLine> projectEditorLine(const CameraFrame& camera,
                                                   WorldPosition first,
                                                   WorldPosition second,
                                                   WorldColor color) {
  const glm::dmat4 matrix{glm::make_mat4(camera.view_projection.data())};
  glm::dvec4 a = matrix * glm::dvec4{first.x, first.y, first.z, 1};
  glm::dvec4 b = matrix * glm::dvec4{second.x, second.y, second.z, 1};
  for (int i = 0; i < 4; ++i) {
    if (!std::isfinite(a[i]) || !std::isfinite(b[i])) return std::nullopt;
  }
  const std::array<glm::dvec4, 6> planes = {{{1, 0, 0, 1},
                                             {-1, 0, 0, 1},
                                             {0, 1, 0, 1},
                                             {0, -1, 0, 1},
                                             {0, 0, 1, 0},
                                             {0, 0, -1, 1}}};
  for (const auto plane : planes) {
    const double da = glm::dot(plane, a);
    const double db = glm::dot(plane, b);
    if (da < 0 && db < 0) return std::nullopt;
    if (da < 0 || db < 0) {
      const auto intersection = a + (b - a) * (da / (da - db));
      if (da < 0)
        a = intersection;
      else
        b = intersection;
    }
  }
  if (a.w <= 0 || b.w <= 0) return std::nullopt;
  return EditorOverlayLine{{static_cast<float>((a.x / a.w + 1) * 0.5),
                            static_cast<float>((a.y / a.w + 1) * 0.5)},
                           {static_cast<float>((b.x / b.w + 1) * 0.5),
                            static_cast<float>((b.y / b.w + 1) * 0.5)},
                           color};
}

std::vector<EditorOverlayLine> buildEditorOverlay(
    const EditorDocument& document, const CameraFrame& camera,
    std::optional<WorldPosition> placement_hit) {
  std::vector<EditorOverlayLine> lines;
  if (!document.document()) return lines;
  constexpr WorldColor selected_color{255, 205, 60, 255};
  constexpr WorldColor light_color{120, 190, 255, 255};
  constexpr WorldColor spawn_color{100, 235, 140, 255};
  const auto line = [&](WorldPosition a, WorldPosition b, WorldColor color) {
    if (auto projected = projectEditorLine(camera, a, b, color))
      lines.push_back(*projected);
  };
  const auto box = [&](WorldPosition center, WorldExtent extent, float yaw) {
    std::array<WorldPosition, 8> corners;
    const double angle = static_cast<double>(yaw) * std::numbers::pi / 180.0;
    for (int i = 0; i < 8; ++i) {
      const double x = (i & 1) ? extent.x : -extent.x;
      const double z = (i & 4) ? extent.z : -extent.z;
      corners[i] = {center.x + static_cast<float>(std::cos(angle) * x +
                                                  std::sin(angle) * z),
                    center.y + ((i & 2) ? extent.y : -extent.y),
                    center.z + static_cast<float>(-std::sin(angle) * x +
                                                  std::cos(angle) * z)};
    }
    for (int i = 0; i < 8; ++i) {
      for (int bit : {1, 2, 4})
        if (!(i & bit)) line(corners[i], corners[i | bit], selected_color);
    }
  };
  const auto marker = [&](WorldPosition center, WorldColor color) {
    for (int plane = 0; plane < 3; ++plane) {
      for (int i = 0; i < 24; ++i) {
        const auto point = [&](int step) {
          const double angle = step * 2 * std::numbers::pi / 24;
          const float a =
              editor_marker_radius * static_cast<float>(std::cos(angle));
          const float b =
              editor_marker_radius * static_cast<float>(std::sin(angle));
          return WorldPosition{center.x + (plane == 2 ? 0 : a),
                               center.y + (plane == 0   ? 0
                                           : plane == 1 ? b
                                                        : a),
                               center.z + (plane == 1 ? 0 : b)};
        };
        line(point(i), point(i + 1), color);
      }
    }
  };
  const auto& level = *document.document();
  marker(editorSpawnMarker(level.player_spawn),
         document.selection() == editor_spawn ? selected_color : spawn_color);
  for (std::size_t i = 0; i < level.environment_light.point_lights.size();
       ++i) {
    marker(level.environment_light.point_lights[i].position,
           document.selection() == editor_first_light + i ? selected_color
                                                          : light_color);
  }
  if (const auto value = document.object(document.selection())) {
    if (const auto* solid = std::get_if<PrototypeSolid>(&*value))
      box(solid->center, solid->half_extent, 0);
    if (const auto* prop = std::get_if<PrototypeStaticProp>(&*value)) {
      box(prototypeStaticPropProxyWorldCenter(*prop),
          prototypeStaticPropProxyWorldHalfExtent(*prop), prop->yaw_degrees);
    }
  }
  if (placement_hit) {
    const auto p = *placement_hit;
    line({p.x - 0.35F, p.y, p.z}, {p.x + 0.35F, p.y, p.z}, selected_color);
    line({p.x, p.y, p.z - 0.35F}, {p.x, p.y, p.z + 0.35F}, selected_color);
    line(p, {p.x, p.y + 0.5F, p.z}, selected_color);
  }
  return lines;
}
