#ifndef CORE_WORLD_DOOR_HPP
#define CORE_WORLD_DOOR_HPP

#include <array>
#include <optional>
#include <string>

#include "core/frame.hpp"
#include "core/world/level_document.hpp"

struct DoorLeafPose {
  WorldPosition center{};
  WorldExtent half_extent{};
  float yaw_degrees{};
};

[[nodiscard]] std::string doorFieldError(const DoorDefinition& door);
[[nodiscard]] bool doorGeometryIsValid(const DoorDefinition& door) noexcept;
[[nodiscard]] float doorInitialAngle(const DoorDefinition& door) noexcept;
[[nodiscard]] DoorLeafPose doorLeafPose(const DoorDefinition& door,
                                        float angle) noexcept;
[[nodiscard]] WorldPosition doorWorldPoint(const DoorDefinition& door,
                                           float angle,
                                           WorldPosition local) noexcept;
[[nodiscard]] WorldPosition doorLocalPoint(const DoorDefinition& door,
                                           float angle,
                                           WorldPosition world) noexcept;
[[nodiscard]] std::array<WorldPosition, 8> doorCorners(
    const DoorDefinition& door, float angle) noexcept;
[[nodiscard]] std::optional<float> doorRayDistance(
    const DoorDefinition& door, float angle, WorldPosition origin,
    WorldPosition direction) noexcept;
[[nodiscard]] bool doorPointInside(const DoorDefinition& door, float angle,
                                   WorldPosition point) noexcept;
[[nodiscard]] bool yawedBoxesOverlap(const DoorLeafPose& a,
                                     const DoorLeafPose& b,
                                     float tolerance = 0.0001F) noexcept;
[[nodiscard]] bool doorOverlapsTerrain(const DoorDefinition& door, float angle,
                                       const PrototypeTerrain& terrain);

// Six fixed generated boxes: leaf, two handles, bolt, and two knock plates.
// Pure presentation used by runtime and initial-state editor preview.
[[nodiscard]] std::array<OpaqueBoxFrame, 6> doorPresentationBoxes(
    const DoorDefinition& door, float angle, bool locked,
    float handle_depression = 0.0F, float knock_pulse = 0.0F,
    int feedback_side = 1) noexcept;

#endif
