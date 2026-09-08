#ifndef DEVELOPMENT_CHARACTER_FIXTURE_HPP
#define DEVELOPMENT_CHARACTER_FIXTURE_HPP

#include <cmath>
#include <numbers>

#include "core/frame.hpp"
#include "core/world/level_document.hpp"

// Fixed P07 inspection views; no gameplay camera or level-file policy.
namespace character_fixture {
inline WorldPosition subtract(WorldPosition a, WorldPosition b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline WorldPosition normalized(WorldPosition v) {
  const float length = std::hypot(v.x, v.y, v.z);
  return {v.x / length, v.y / length, v.z / length};
}
inline WorldPosition cross(WorldPosition a, WorldPosition b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
}
inline float dot(WorldPosition a, WorldPosition b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline CameraFrame camera(WorldPosition eye, WorldPosition target, float aspect) {
  const auto f = normalized(subtract(target, eye));
  const auto r = normalized(cross(f, {0, 1, 0}));
  const auto d = cross(f, r);
  const float sy = 1 / std::tan(std::numbers::pi_v<float> / 6), sx = sy / aspect;
  constexpr float near = .05F, far = 100, a = far / (far - near), b = -near * a;
  return {{sx * r.x, sy * d.x, a * f.x, f.x, sx * r.y, sy * d.y, a * f.y, f.y,
           sx * r.z, sy * d.z, a * f.z, f.z, -sx * dot(r, eye),
           -sy * dot(d, eye), b - a * dot(f, eye), -dot(f, eye)}};
}
inline SpotLightFrame flashlight(WorldPosition eye, WorldPosition target) {
  const auto direction = normalized(subtract(target, eye));
  return {{eye.x, eye.y, eye.z, 12},
          {direction.x, direction.y, direction.z, .965926F},
          {1, 1, 1, 1}, {.906308F, 1, 0, 0}};
}
inline LevelDocument controlScene() {
  LevelDocument doc;
  doc.entries = {{"inspection", {{0, 0, 5}, -90}}};
  doc.default_entry = "inspection";
  doc.solids = {{{0, -.1F, 0}, {6, .1F, 6}, {255, 255, 255, 255},
                 PrototypeSolidKind::Floor, "prototype-floor"}};
  doc.environment_light = {
      {{{-2, 3, 2}, {1, 1, 1}, 1.5F, 10, "key", true, true}}, .08F};
  return doc;
}
}  // namespace character_fixture
#endif
