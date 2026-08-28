#ifndef CORE_RENDER_PROTOTYPE_SCENE_HPP
#define CORE_RENDER_PROTOTYPE_SCENE_HPP

#include <cstdint>
#include <type_traits>
#include <vector>

struct PositionColorVertex {
  float position[3];
  std::uint8_t color[4];
};

static_assert(std::is_standard_layout_v<PositionColorVertex>);
static_assert(sizeof(PositionColorVertex) == sizeof(float) * 3 + 4);

[[nodiscard]] const std::vector<PositionColorVertex>& prototypeSceneVertices();

#endif
