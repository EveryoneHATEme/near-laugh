#ifndef CORE_WORLD_HOUSEHOLD_HPP
#define CORE_WORLD_HOUSEHOLD_HPP

#include "core/world/door.hpp"

// The panel lies on its local X/Z plane; position is its centre.
inline constexpr WorldExtent household_document_half_extent{0.14F, 0.003F,
                                                            0.20F};
[[nodiscard]] DoorLeafPose householdBoxPose(
    const HouseholdBoxDefinition& box) noexcept;
[[nodiscard]] DoorLeafPose householdDocumentPose(
    const HouseholdDocumentDefinition& document) noexcept;
[[nodiscard]] OpaqueBoxFrame householdBoxPresentation(
    WorldPosition center, std::array<float, 4> orientation) noexcept;
[[nodiscard]] OpaqueBoxFrame householdDocumentPresentation(
    const HouseholdDocumentDefinition& document) noexcept;
[[nodiscard]] OpaqueBoxFrame householdRadioPresentation(
    const PrototypeStaticProp& prop, bool on);
[[nodiscard]] OpaqueBoxFrame householdRadioBounds(
    const PrototypeStaticProp& prop);
[[nodiscard]] bool householdPointInside(const OpaqueBoxFrame& box,
                                        WorldPosition point) noexcept;
[[nodiscard]] std::optional<float> householdRayDistance(
    const OpaqueBoxFrame& box, WorldPosition origin,
    WorldPosition direction) noexcept;
// Initial authoring preview only; safe unresolved links have no invented pose.
[[nodiscard]] std::vector<OpaqueBoxFrame> householdInitialPresentation(
    const LevelDocument& document);
// Authored data and initial clearance only. Requires no devices or live
// physics.
[[nodiscard]] std::vector<LevelDiagnostic> validateHouseholdDefinitions(
    const LevelDocument& document,
    const std::filesystem::path& source_path = {});

#endif
