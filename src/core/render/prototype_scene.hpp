#ifndef CORE_RENDER_PROTOTYPE_SCENE_HPP
#define CORE_RENDER_PROTOTYPE_SCENE_HPP

#include <cstdint>
#include <type_traits>
#include <vector>

#include "core/world/prototype_level.hpp"

struct PositionColorVertex {
  float position[3];
  std::uint8_t color[4];
  float normal[3];
};

static_assert(std::is_standard_layout_v<PositionColorVertex>);
static_assert(sizeof(PositionColorVertex) == sizeof(float) * 6 + 4);

[[nodiscard]] std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const PrototypeLevel& level);

#endif
