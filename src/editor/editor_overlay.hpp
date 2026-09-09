#ifndef EDITOR_EDITOR_OVERLAY_HPP
#define EDITOR_EDITOR_OVERLAY_HPP

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "editor/editor_document.hpp"

struct EditorOverlayLine {
  // Normalized viewport coordinates after clipping to the Vulkan view volume.
  std::array<float, 2> first{};
  std::array<float, 2> second{};
  WorldColor color{};
};

struct EditorOverlayLabel {
  std::array<float, 2> position{};
  WorldColor color{};
  std::string text;
  EditorObjectId object{};
};

[[nodiscard]] std::optional<EditorOverlayLine> projectEditorLine(
    const CameraFrame& camera, WorldPosition first, WorldPosition second,
    WorldColor color);
[[nodiscard]] std::vector<EditorOverlayLine> buildEditorOverlay(
    const EditorDocument& document, const CameraFrame& camera,
    std::optional<WorldPosition> placement_hit = std::nullopt,
    const EditorTerrainBrush* brush = nullptr);
[[nodiscard]] std::vector<EditorOverlayLabel> buildEditorCharacterOverlayLabels(
    const EditorDocument& document, const CameraFrame& camera);

#endif
