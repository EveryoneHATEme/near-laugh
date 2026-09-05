#ifndef EDITOR_EDITOR_PICKING_HPP
#define EDITOR_EDITOR_PICKING_HPP

#include <optional>

#include "core/frame.hpp"
#include "editor/editor_document.hpp"

inline constexpr float editor_marker_radius = 0.25F;

struct EditorRay {
  WorldPosition origin{};
  WorldPosition direction{};
};

struct EditorTerrainHit {
  WorldPosition position{};
  double distance{};
};

[[nodiscard]] std::optional<EditorRay> editorPointerRay(
    const CameraFrame& camera, double x, double y, double width, double height);
[[nodiscard]] EditorObjectId pickEditorObject(const EditorDocument& document,
                                              const EditorRay& ray);
[[nodiscard]] std::optional<EditorTerrainHit> pickEditorTerrain(
    const PrototypeTerrain& terrain, const EditorRay& ray);
[[nodiscard]] WorldPosition editorSpawnMarker(
    const PrototypePlayerSpawn& spawn);

// The UI supplies a press edge; capture/navigation suppress both selection and
// placement.
[[nodiscard]] std::optional<WorldPosition> updateEditorViewport(
    EditorDocument& document, const std::optional<EditorRay>& ray,
    bool pointer_owned, bool navigation_active, bool pressed, bool placing);

[[nodiscard]] std::optional<WorldPosition> updateEditorTerrainViewport(
    EditorDocument& document, const std::optional<EditorRay>& ray,
    bool pointer_owned, bool navigation_active, bool pressed, bool down,
    bool pointer_moved = true);

#endif
