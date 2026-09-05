#ifndef EDITOR_EDITOR_TERRAIN_HPP
#define EDITOR_EDITOR_TERRAIN_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/world/level_document.hpp"

enum class EditorBrushMode { Raise, Lower, Smooth };

struct EditorTerrainBrush {
  bool operator==(const EditorTerrainBrush&) const = default;
  EditorBrushMode mode{EditorBrushMode::Raise};
  float radius{2.0F};
  float strength{0.05F};
  float smooth_strength{0.5F};
  float falloff{1.0F};
};

[[nodiscard]] std::string editorBrushFieldError(
    const EditorTerrainBrush& brush);
[[nodiscard]] bool commitEditorBrush(EditorTerrainBrush& current,
                                     const EditorTerrainBrush& candidate,
                                     std::string& error);
// At the radius the weight is zero, including when falloff is zero.
[[nodiscard]] double editorBrushWeight(double distance, double radius,
                                       double falloff);

struct EditorTerrainSampleEdit {
  bool operator==(const EditorTerrainSampleEdit&) const = default;
  std::size_t index{};
  float before{};
  float after{};
};

// Pure kernel: computes sorted, changed samples from the pre-stamp terrain.
[[nodiscard]] std::vector<EditorTerrainSampleEdit> editorTerrainStamp(
    const PrototypeTerrain& terrain, const EditorTerrainBrush& brush,
    WorldPosition center);

// Resample observed X/Z segments, carrying unused distance across updates.
// A miss breaks the path so returning to terrain does not bridge unseen space.
class EditorTerrainPath {
 public:
  [[nodiscard]] std::vector<WorldPosition> advance(
      std::optional<WorldPosition> hit);

 private:
  std::optional<WorldPosition> previous_{};
  double remainder_{};
};

struct EditorTerrainStroke {
  EditorTerrainBrush brush{};
  EditorTerrainPath path{};
  std::map<std::size_t, float> before{};

  [[nodiscard]] bool advance(PrototypeTerrain& terrain,
                             std::optional<WorldPosition> hit);
  [[nodiscard]] std::vector<EditorTerrainSampleEdit> changes(
      const PrototypeTerrain& terrain) const;
};

#endif
