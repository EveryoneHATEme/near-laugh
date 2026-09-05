#ifndef CORE_RENDER_PROTOTYPE_SCENE_HPP
#define CORE_RENDER_PROTOTYPE_SCENE_HPP

#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include "core/world/prototype_level.hpp"

struct PositionColorVertex {
  float position[3];
  std::uint8_t color[4];
  float normal[3];
  float texture_coordinates[2];
  std::uint32_t texture_layer;
};

static_assert(std::is_standard_layout_v<PositionColorVertex>);
static_assert(sizeof(PositionColorVertex) == sizeof(float) * 8 + 8);

[[nodiscard]] std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const PrototypeLevel& level);

// The editor may preview renderable fields while gameplay constraints fail.
[[nodiscard]] std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const PrototypeTerrain& terrain, std::span<const PrototypeSolid> solids);

#endif
