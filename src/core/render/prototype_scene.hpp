#ifndef CORE_RENDER_PROTOTYPE_SCENE_HPP
#define CORE_RENDER_PROTOTYPE_SCENE_HPP

#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/frame.hpp"
#include "core/world/prototype_level.hpp"

struct PositionColorVertex {
  float position[3];
  std::uint8_t color[4];
  float normal[3];
  float texture_coordinates[2];
  // CPU grouping key for structural materials; imported vertices use zero.
  // Material descriptors now determine appearance, not an image-array layer.
  std::uint32_t texture_layer;
};

static_assert(std::is_standard_layout_v<PositionColorVertex>);
static_assert(sizeof(PositionColorVertex) == sizeof(float) * 8 + 8);

[[nodiscard]] std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const PrototypeLevel& level);

// The editor may preview renderable fields while gameplay constraints fail.
[[nodiscard]] std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const std::optional<PrototypeTerrain>& terrain,
    std::span<const PrototypeSolid> solids,
    const std::optional<PrototypeLightSwitch>& light_switch = std::nullopt);

[[nodiscard]] std::uint32_t structuralMaterialIndex(std::string_view id);
[[nodiscard]] std::vector<PositionColorVertex> buildOpaqueBoxVertices(
    std::span<const OpaqueBoxFrame> boxes);

#endif
