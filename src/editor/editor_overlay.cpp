#include "editor/editor_overlay.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <numbers>

#include "core/world/characters.hpp"
#include "core/world/door.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"
#include "editor/editor_character_spatial.hpp"
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
    std::optional<WorldPosition> placement_hit,
    const EditorTerrainBrush* brush) {
  std::vector<EditorOverlayLine> lines;
  if (!document.document()) return lines;
  constexpr WorldColor selected_color{255, 205, 60, 255};
  constexpr WorldColor light_color{120, 190, 255, 255};
  constexpr WorldColor spawn_color{100, 235, 140, 255};
  const auto line = [&](WorldPosition a, WorldPosition b, WorldColor color) {
    if (auto projected = projectEditorLine(camera, a, b, color))
      lines.push_back(*projected);
  };
  const auto box = [&](WorldPosition center, WorldExtent extent, float yaw,
                       WorldColor color = WorldColor{255, 205, 60, 255}) {
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
        if (!(i & bit)) line(corners[i], corners[i | bit], color);
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
  constexpr WorldColor character_invalid{255, 70, 70, 255};
  const auto character_error = [&](const std::string& record) {
    return std::any_of(document.diagnostics().begin(),
                       document.diagnostics().end(),
                       [&](const auto& diagnostic) {
                         return diagnostic.document_path.starts_with(record);
                       });
  };
  constexpr WorldColor mark_color{130, 240, 165, 255};
  constexpr WorldColor route_color{230, 140, 255, 255};
  constexpr WorldColor proxy_color{70, 220, 255, 255};
  const auto arrow = [&](WorldPosition first, WorldPosition second,
                         WorldColor color, bool tip_at_end) {
    line(first, second, color);
    const glm::vec3 delta{second.x - first.x, second.y - first.y,
                          second.z - first.z};
    const float length = glm::length(delta);
    if (!(length > 0.001F)) return;
    const auto forward = delta / length;
    auto side = glm::cross(forward, glm::vec3{0, 1, 0});
    if (glm::length(side) < 0.001F) side = {1, 0, 0};
    side = glm::normalize(side);
    const auto tip = glm::vec3{first.x, first.y, first.z} +
                     delta * (tip_at_end ? 1.F : 0.65F);
    const float size = std::min(0.2F, length * 0.25F);
    for (const float sign : {-1.F, 1.F}) {
      const auto wing = tip - forward * size + side * size * 0.55F * sign;
      line({tip.x, tip.y, tip.z}, {wing.x, wing.y, wing.z}, color);
    }
  };
  const auto route_handle = [&](WorldPosition point, WorldColor color) {
    constexpr float radius = editor_character_route_radius;
    for (int plane = 0; plane < 2; ++plane) {
      const std::array<WorldPosition, 4> corners = {
          {{point.x, point.y + radius, point.z},
           {point.x + (plane == 0 ? radius : 0), point.y,
            point.z + (plane == 1 ? radius : 0)},
           {point.x, point.y - radius, point.z},
           {point.x - (plane == 0 ? radius : 0), point.y,
            point.z - (plane == 1 ? radius : 0)}}};
      for (std::size_t i = 0; i < corners.size(); ++i)
        line(corners[i], corners[(i + 1) % corners.size()], color);
    }
  };
  for (std::size_t i = 0; i < level.characters.marks.size(); ++i) {
    const auto& mark = level.characters.marks[i];
    if (!editorFiniteCharacterMark(mark)) continue;
    const auto color =
        document.selection() ==
                document.characterIds(EditorCharacterKind::Mark)[i]
            ? selected_color
        : character_error("characters.marks[" + std::to_string(i) + "]")
            ? character_invalid
            : mark_color;
    const auto handle = editorCharacterMarkHandle(mark);
    marker(handle, color);
    line(mark.feet_position, handle, color);
    arrow(handle, editorCharacterFacingTip(mark), color, true);
    route_handle(editorCharacterFacingTip(mark), color);
  }
  for (std::size_t i = 0; i < level.characters.actors.size(); ++i) {
    const auto& actor = level.characters.actors[i];
    const auto* mark = findCharacterMark(level.characters, actor.initial_mark);
    if (!mark || !editorFiniteCharacterMark(*mark)) continue;
    const bool selected = document.selection() ==
                          document.characterIds(EditorCharacterKind::Actor)[i];
    const bool invalid =
        character_error("characters.actors[" + std::to_string(i) + "]");
    const auto* model = findCharacterModel(actor.model);
    if (!model || !characterCatalogIsValid(*model)) {
      marker(editorCharacterDiagnosticHandle(*mark), character_invalid);
      continue;
    }
    const auto bounds = editorCharacterVisualBounds(*model, *mark);
    box(bounds.center, bounds.half_extent, bounds.yaw_degrees,
        selected  ? selected_color
        : invalid ? character_invalid
                  : WorldColor{180, 185, 205, 180});
    if (!selected) continue;
    // The capsule is authored metadata, distinct from conservative visual
    // bounds.
    const float radius = model->capsule_radius_m;
    const float bottom = mark->feet_position.y + radius;
    const float top = mark->feet_position.y + model->capsule_height_m - radius;
    for (int step = 0; step < 24; ++step) {
      const auto ring = [&](int i, float height) {
        const double angle = i * 2 * std::numbers::pi / 24;
        return WorldPosition{mark->feet_position.x +
                                 radius * static_cast<float>(std::cos(angle)),
                             height,
                             mark->feet_position.z +
                                 radius * static_cast<float>(std::sin(angle))};
      };
      for (const float height : {bottom, top})
        line(ring(step, height), ring(step + 1, height), proxy_color);
      if (step % 6 == 0) line(ring(step, bottom), ring(step, top), proxy_color);
      for (int plane = 0; plane < 2; ++plane)
        for (const float sign : {-1.F, 1.F}) {
          const auto cap = [&](int i) {
            const double angle = i * std::numbers::pi / 24;
            const float horizontal =
                radius * static_cast<float>(std::cos(angle));
            return WorldPosition{
                mark->feet_position.x + (plane == 0 ? horizontal : 0),
                (sign < 0 ? bottom : top) +
                    sign * radius * static_cast<float>(std::sin(angle)),
                mark->feet_position.z + (plane == 1 ? horizontal : 0)};
          };
          line(cap(step), cap(step + 1), proxy_color);
        }
    }
  }
  for (std::size_t i = 0; i < level.characters.routes.size(); ++i) {
    const auto& route = level.characters.routes[i];
    const auto color =
        document.selection() ==
                document.characterIds(EditorCharacterKind::Route)[i]
            ? selected_color
        : character_error("characters.routes[" + std::to_string(i) + "]")
            ? character_invalid
            : route_color;
    const CharacterMarkDefinition* previous = nullptr;
    for (const auto& id : route.marks) {
      const auto* mark = findCharacterMark(level.characters, id);
      if (mark && !editorFiniteCharacterMark(*mark)) mark = nullptr;
      if (mark) {
        const auto point = editorCharacterRouteHandle(*mark);
        route_handle(point, color);
        if (previous)
          arrow(editorCharacterRouteHandle(*previous), point, color, false);
      }
      previous = mark;
    }
  }
  constexpr WorldColor audio_color{210, 130, 255, 255};
  for (std::size_t i = 0; i < level.audio.sources.size(); ++i)
    marker(level.audio.sources[i].position,
           document.selection() == document.audioIds(EditorAudioKind::Source)[i]
               ? selected_color
               : audio_color);
  for (std::size_t i = 0; i < level.audio.rooms.size(); ++i) {
    const auto& room = level.audio.rooms[i];
    box(room.center, room.half_extent, 0,
        document.selection() == document.audioIds(EditorAudioKind::Room)[i]
            ? selected_color
            : WorldColor{140, 100, 180, 150});
  }
  if (const auto value = document.object(document.selection())) {
    if (const auto* connection =
            std::get_if<AudioConnectionDefinition>(&*value)) {
      const auto room_center = [&](const std::optional<std::string>& id)
          -> std::optional<WorldPosition> {
        if (!id) return {};
        for (const auto& room : level.audio.rooms)
          if (room.id == *id) return room.center;
        return {};
      };
      auto a = room_center(connection->room_a),
           b = room_center(connection->room_b);
      std::optional<WorldPosition> hinge;
      for (const auto& door : level.doors)
        if (connection->door == door.id) hinge = door.hinge_position;
      if (a && b) line(*a, *b, selected_color);
      if (hinge) {
        if (a) line(*a, *hinge, audio_color);
        if (b) line(*b, *hinge, audio_color);
      } else if (a && !connection->room_b)
        line(*a, {a->x, a->y + 2, a->z}, selected_color);
      else if (b && !connection->room_a)
        line(*b, {b->x, b->y + 2, b->z}, selected_color);
    }
  }
  for (const auto& diagnostic : document.diagnostics()) {
    const auto& location = diagnostic.terrain_location;
    if (!level.terrain || !location || !location->triangle ||
        location->x >= prototype_terrain_cell_count ||
        location->z >= prototype_terrain_cell_count)
      continue;
    const auto x = location->x, z = location->z;
    const auto a = prototypeTerrainSamplePosition(*level.terrain, x, z);
    const auto c = prototypeTerrainSamplePosition(*level.terrain, x + 1, z + 1);
    const auto b =
        *location->triangle == 0
            ? prototypeTerrainSamplePosition(*level.terrain, x, z + 1)
            : prototypeTerrainSamplePosition(*level.terrain, x + 1, z);
    constexpr WorldColor invalid{255, 70, 70, 255};
    line(a, b, invalid);
    line(b, c, invalid);
    line(c, a, invalid);
  }
  if (placement_hit && brush && level.terrain) {
    // Concentric intensity rings show the same falloff used by the kernel.
    for (int ring = 1; ring <= 4; ++ring) {
      const double radius = brush->radius * ring / 4.0;
      const double weight =
          editorBrushWeight(radius, brush->radius, brush->falloff);
      const WorldColor color{
          255, 205, 60,
          static_cast<std::uint8_t>(ring == 4 ? 255 : 40 + 180 * weight)};
      for (int step = 0; step < 96; ++step) {
        const auto point = [&](int i) {
          const double angle = i * 2 * std::numbers::pi / 96;
          WorldPosition p{
              placement_hit->x + static_cast<float>(radius * std::cos(angle)),
              0,
              placement_hit->z + static_cast<float>(radius * std::sin(angle))};
          p.y = prototypeTerrainHeightAt(*level.terrain, p.x, p.z);
          return p;
        };
        // Out-of-terrain arcs get NaN heights and are omitted by projection.
        line(point(step), point(step + 1), color);
      }
    }
  }
  for (std::size_t i = 0; i < level.entries.size(); ++i)
    marker(editorSpawnMarker(level.entries[i].pose),
           document.selection() == document.entryIds()[i] ? selected_color
                                                          : spawn_color);
  for (const auto& prop : level.props)
    if (!findSceneModel(prop.model))
      marker(prop.translation, {255, 80, 80, 255});
  for (std::size_t i = 0; i < level.environment_light.point_lights.size();
       ++i) {
    marker(level.environment_light.point_lights[i].position,
           document.selection() == document.lightIds()[i] ? selected_color
                                                          : light_color);
  }
  if (const auto value = document.object(document.selection())) {
    if (const auto* light = std::get_if<PrototypePointLight>(&*value);
        light && pointLightFieldError(*light).empty()) {
      // Three great circles make the authored influence radius visible.
      for (int axis = 0; axis < 3; ++axis)
        for (int step = 0; step < 64; ++step) {
          const auto point = [&](int n) {
            const float a = static_cast<float>(n * 2 * std::numbers::pi / 64);
            auto p = light->position;
            const float c = light->radius * std::cos(a);
            const float s = light->radius * std::sin(a);
            if (axis == 0) {
              p.y += c;
              p.z += s;
            }
            if (axis == 1) {
              p.x += c;
              p.z += s;
            }
            if (axis == 2) {
              p.x += c;
              p.y += s;
            }
            return p;
          };
          line(point(step), point(step + 1), light_color);
        }
      for (const auto& light_switch : level.light_switches)
        if (light_switch.light_id == light->id)
          line(light->position, light_switch.position, selected_color);
    }
    if (const auto* light_switch = std::get_if<PrototypeLightSwitch>(&*value);
        light_switch && lightSwitchIsValid(*light_switch)) {
      const auto corners = lightSwitchCorners(*light_switch);
      for (const auto& light : level.environment_light.point_lights)
        if (light.id == light_switch->light_id)
          line(light_switch->position, light.position, light_color);
      for (int i = 0; i < 8; ++i)
        for (int bit : {1, 2, 4})
          if (!(i & bit)) line(corners[i], corners[i | bit], selected_color);
    }
    if (const auto* solid = std::get_if<PrototypeSolid>(&*value))
      box(solid->center, solid->half_extent, 0);
    if (const auto* door = std::get_if<DoorDefinition>(&*value);
        door && doorGeometryIsValid(*door)) {
      const auto pose = doorLeafPose(*door, doorInitialAngle(*door));
      box(pose.center, pose.half_extent, pose.yaw_degrees);
      const auto hinge = door->hinge_position;
      line(hinge, {hinge.x, hinge.y + door->height, hinge.z},
           {100, 235, 140, 255});
      const WorldPosition tip{door->width, .03F, 0};
      for (int i = 0; i < 24; ++i)
        line(
            doorWorldPoint(*door, door->open_angle_degrees * i / 24, tip),
            doorWorldPoint(*door, door->open_angle_degrees * (i + 1) / 24, tip),
            {100, 235, 140, 255});
      line(hinge, doorWorldPoint(*door, door->open_angle_degrees, tip),
           {100, 235, 140, 255});
      if (door->lock_side != DoorLockSide::None) {
        const float side =
            door->lock_side == DoorLockSide::PositiveZ ? 1.F : -1.F;
        line(doorWorldPoint(*door, 0, {door->width * .7F, 1, 0}),
             doorWorldPoint(*door, 0, {door->width * .7F, 1, side * .4F}),
             {255, 120, 70, 255});
      }
    }
    if (const auto* prop = std::get_if<PrototypeStaticProp>(&*value)) {
      if (const auto* model = findSceneModel(prop->model)) {
        const auto bounds = sceneModelBounds(*model);
        box(propBoxWorldCenter(*prop, bounds),
            propBoxWorldHalfExtent(*prop, bounds), prop->yaw_degrees);
      } else
        marker(prop->translation, {255, 80, 80, 255});
      for (const auto& bounds : prop->collision_boxes)
        box(propBoxWorldCenter(*prop, bounds),
            propBoxWorldHalfExtent(*prop, bounds), prop->yaw_degrees,
            {70, 220, 255, 255});
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

std::vector<EditorOverlayLabel> buildEditorCharacterOverlayLabels(
    const EditorDocument& document, const CameraFrame& camera) {
  std::vector<EditorOverlayLabel> labels;
  if (!document.document()) return labels;
  constexpr WorldColor selected_color{255, 205, 60, 255};
  constexpr WorldColor invalid_color{255, 70, 70, 255};
  const auto label = [&](EditorObjectId object, WorldPosition point,
                         WorldColor color, std::string text) {
    const auto projected = projectEditorLine(camera, point, point, color);
    if (!projected) return;
    for (auto& existing : labels)
      if (existing.object == object && existing.position == projected->first) {
        existing.text += "\n" + text;
        if (color == invalid_color) existing.color = color;
        return;
      }
    labels.push_back({projected->first, color, std::move(text), object});
  };
  const auto& characters = document.document()->characters;
  for (std::size_t i = 0; i < characters.actors.size(); ++i) {
    const auto& actor = characters.actors[i];
    const auto* mark = findCharacterMark(characters, actor.initial_mark);
    if (!mark || !editorFiniteCharacterMark(*mark)) continue;
    const auto id = document.characterIds(EditorCharacterKind::Actor)[i];
    const auto* model = findCharacterModel(actor.model);
    if (!model || !characterCatalogIsValid(*model))
      label(id, editorCharacterDiagnosticHandle(*mark), invalid_color,
            "Actor " + actor.id + ": missing model '" + actor.model + "'");
    else if (document.selection() == id) {
      auto point = mark->feet_position;
      point.y += model->capsule_height_m + 0.2F;
      label(id, point, selected_color,
            "Actor " + actor.id + "\nYellow: visual bounds; cyan: capsule");
    }
  }
  for (std::size_t i = 0; i < characters.marks.size(); ++i) {
    const auto id = document.characterIds(EditorCharacterKind::Mark)[i];
    const auto& mark = characters.marks[i];
    if (document.selection() == id && editorFiniteCharacterMark(mark))
      label(id, editorCharacterFacingTip(mark), selected_color,
            "Mark " + mark.id + " / facing");
  }
  for (std::size_t i = 0; i < characters.routes.size(); ++i) {
    const auto& route = characters.routes[i];
    const auto id = document.characterIds(EditorCharacterKind::Route)[i];
    std::vector<const CharacterMarkDefinition*> marks;
    for (const auto& mark_id : route.marks) {
      const auto* mark = findCharacterMark(characters, mark_id);
      marks.push_back(mark && editorFiniteCharacterMark(*mark) ? mark
                                                               : nullptr);
    }
    const auto first = std::find_if(marks.begin(), marks.end(),
                                    [](const auto* mark) { return mark; });
    // A wholly unresolved route has list/Properties diagnostics, no world
    // anchor.
    if (first == marks.end()) continue;
    if (std::none_of(
            characters.actors.begin(), characters.actors.end(),
            [&](const auto& actor) { return actor.id == route.actor; }))
      label(id, editorCharacterRouteHandle(**first), invalid_color,
            "Route " + route.id + ": missing actor '" + route.actor + "'");
    for (std::size_t j = 0; j < marks.size(); ++j) {
      if (marks[j]) {
        if (document.selection() == id)
          label(id, editorCharacterRouteHandle(*marks[j]), selected_color,
                "Route " + route.id + " / " + std::to_string(j + 1) + ": " +
                    route.marks[j]);
        continue;
      }
      const CharacterMarkDefinition* anchor = nullptr;
      // Label an unresolved slot beside a real neighboring mark, without a
      // line.
      for (std::size_t distance = 1; distance < marks.size() && !anchor;
           ++distance) {
        if (j >= distance) anchor = marks[j - distance];
        if (!anchor && j + distance < marks.size())
          anchor = marks[j + distance];
      }
      if (anchor)
        label(id, editorCharacterRouteHandle(*anchor), invalid_color,
              "Route " + route.id + " / " + std::to_string(j + 1) +
                  ": missing mark '" + route.marks[j] + "'");
    }
  }
  return labels;
}
