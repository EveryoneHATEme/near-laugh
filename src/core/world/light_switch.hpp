#ifndef CORE_WORLD_LIGHT_SWITCH_HPP
#define CORE_WORLD_LIGHT_SWITCH_HPP

#include "core/world/level_document.hpp"

// The centre is the authored position; local +Z faces the approach at yaw 0.
// Both the plate and fixed rocker fit inside these same targeting bounds.
inline constexpr WorldExtent light_switch_half_extent{0.09F, 0.13F, 0.02F};

[[nodiscard]] WorldPosition lightSwitchWorldPoint(
    const PrototypeLightSwitch& light_switch, WorldPosition local) noexcept;
[[nodiscard]] std::array<WorldPosition, 8> lightSwitchCorners(
    const PrototypeLightSwitch& light_switch) noexcept;
// Structural safety only: terrain-footprint validation is document-owned.
[[nodiscard]] bool lightSwitchIsValid(
    const PrototypeLightSwitch& light_switch) noexcept;
// Normalizes direction. Returns metres to the first surface; inside origins
// (including the surface itself), invalid inputs, and misses are rejected.
[[nodiscard]] std::optional<float> lightSwitchRayDistance(
    const PrototypeLightSwitch& light_switch, WorldPosition origin,
    WorldPosition direction) noexcept;
[[nodiscard]] std::array<bool, prototype_point_light_count>
initialPointLightEnabled(
    const std::optional<PrototypeLightSwitch>& light_switch) noexcept;

#endif
