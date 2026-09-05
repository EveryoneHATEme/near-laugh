#include "editor/editor_terrain.hpp"

#include <algorithm>
#include <cmath>

#include "core/world/prototype_level.hpp"

namespace {
constexpr int last_sample = static_cast<int>(prototype_terrain_cell_count);
constexpr double stamp_spacing = prototype_terrain_sample_spacing * 0.5;
bool bounded(float value, float minimum, float maximum) {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}
bool finite(WorldPosition p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
std::size_t index(int x, int z) {
  return static_cast<std::size_t>(z) * prototype_terrain_sample_count +
         static_cast<std::size_t>(x);
}
}  // namespace

std::string editorBrushFieldError(const EditorTerrainBrush& brush) {
  switch (brush.mode) {
    case EditorBrushMode::Raise:
    case EditorBrushMode::Lower:
    case EditorBrushMode::Smooth:
      break;
    default:
      return "Unsupported terrain brush mode.";
  }
  if (!bounded(brush.radius, prototype_terrain_sample_spacing, 8.0F))
    return "Brush radius must be finite and between 0.5 and 8 metres.";
  if (!bounded(brush.strength, 0.01F, 1.0F))
    return "Raise/lower strength must be finite and between 0.01 and 1 metre.";
  if (!bounded(brush.smooth_strength, 0.0F, 1.0F))
    return "Smooth strength must be finite and between 0 and 1.";
  if (!bounded(brush.falloff, 0.0F, 1.0F))
    return "Brush falloff must be finite and between 0 and 1.";
  return {};
}

bool commitEditorBrush(EditorTerrainBrush& current,
                       const EditorTerrainBrush& candidate,
                       std::string& error) {
  error = editorBrushFieldError(candidate);
  if (!error.empty()) return false;
  current = candidate;
  return true;
}

double editorBrushWeight(double distance, double radius, double falloff) {
  if (!std::isfinite(distance) || !std::isfinite(radius) ||
      !std::isfinite(falloff) || radius <= 0 || distance < 0 ||
      distance >= radius || falloff < 0 || falloff > 1)
    return 0;
  const double t = distance / radius;
  return 1 - falloff * (t * t * (3 - 2 * t));
}

std::vector<EditorTerrainSampleEdit> editorTerrainStamp(
    const PrototypeTerrain& terrain, const EditorTerrainBrush& brush,
    WorldPosition center) {
  std::vector<EditorTerrainSampleEdit> edits;
  if (!editorBrushFieldError(brush).empty() || !finite(center) ||
      !finite(terrain.origin) ||
      terrain.sample_spacing != prototype_terrain_sample_spacing)
    return edits;

  const double cx = (static_cast<double>(center.x) - terrain.origin.x) /
                    terrain.sample_spacing;
  const double cz = (static_cast<double>(center.z) - terrain.origin.z) /
                    terrain.sample_spacing;
  const double radius = brush.radius / terrain.sample_spacing;
  if (cx + radius < 0 || cz + radius < 0 || cx - radius > last_sample ||
      cz - radius > last_sample)
    return edits;
  const int x0 = static_cast<int>(
      std::ceil(std::clamp(cx - radius, 0.0, double(last_sample))));
  const int z0 = static_cast<int>(
      std::ceil(std::clamp(cz - radius, 0.0, double(last_sample))));
  const int x1 = static_cast<int>(
      std::floor(std::clamp(cx + radius, 0.0, double(last_sample))));
  const int z1 = static_cast<int>(
      std::floor(std::clamp(cz + radius, 0.0, double(last_sample))));

  const int sx0 = std::max(0, x0 - 1), sz0 = std::max(0, z0 - 1);
  const int sx1 = std::min(last_sample, x1 + 1),
            sz1 = std::min(last_sample, z1 + 1);
  const int width = sx1 - sx0 + 1;
  std::vector<float> snapshot;
  if (brush.mode == EditorBrushMode::Smooth) {
    snapshot.reserve(static_cast<std::size_t>(width * (sz1 - sz0 + 1)));
    for (int z = sz0; z <= sz1; ++z)
      for (int x = sx0; x <= sx1; ++x)
        snapshot.push_back(terrain.heights[index(x, z)]);
  }
  for (int z = z0; z <= z1; ++z) {
    for (int x = x0; x <= x1; ++x) {
      const double distance =
          std::hypot(x - cx, z - cz) * terrain.sample_spacing;
      const double weight =
          editorBrushWeight(distance, brush.radius, brush.falloff);
      const float before = terrain.heights[index(x, z)];
      if (weight == 0 || !std::isfinite(before)) continue;
      double after = before;
      if (brush.mode == EditorBrushMode::Smooth) {
        if (brush.smooth_strength == 0) continue;
        // Separable [1, 2, 1] weights, total 16. Border coordinates repeat.
        double sum = 0;
        for (int dz = -1; dz <= 1; ++dz) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int sx = std::clamp(x + dx, 0, last_sample) - sx0;
            const int sz = std::clamp(z + dz, 0, last_sample) - sz0;
            sum += snapshot[static_cast<std::size_t>(sz * width + sx)] *
                   static_cast<double>((dx == 0 ? 2 : 1) * (dz == 0 ? 2 : 1));
          }
        }
        after += (sum / 16 - before) * brush.smooth_strength * weight;
      } else {
        after += (brush.mode == EditorBrushMode::Raise ? 1 : -1) *
                 brush.strength * weight;
      }
      const float result = static_cast<float>(after);
      if (std::isfinite(result) && std::isfinite(terrain.origin.y + result) &&
          result != before)
        edits.push_back({index(x, z), before, result});
    }
  }
  return edits;
}

std::vector<WorldPosition> EditorTerrainPath::advance(
    std::optional<WorldPosition> hit) {
  if (!hit || !finite(*hit)) {
    previous_.reset();
    remainder_ = 0;
    return {};
  }
  // Height is deliberately excluded: raising a stationary pointer must not
  // manufacture path distance as the terrain under it rises.
  hit->y = 0;
  if (!previous_) {
    previous_ = hit;
    return {*hit};
  }
  const double dx = static_cast<double>(hit->x) - previous_->x;
  const double dz = static_cast<double>(hit->z) - previous_->z;
  const double length = std::hypot(dx, dz);
  std::vector<WorldPosition> stamps;
  double travelled = stamp_spacing - remainder_;
  for (; travelled <= length; travelled += stamp_spacing) {
    const double t = travelled / length;
    stamps.push_back({static_cast<float>(previous_->x + dx * t), 0,
                      static_cast<float>(previous_->z + dz * t)});
  }
  remainder_ = length - (travelled - stamp_spacing);
  previous_ = hit;
  return stamps;
}

bool EditorTerrainStroke::advance(PrototypeTerrain& terrain,
                                  std::optional<WorldPosition> hit) {
  if (hit && !prototypeTerrainContains(terrain, hit->x, hit->z)) hit.reset();
  bool changed = false;
  for (const auto stamp : path.advance(hit)) {
    for (const auto& edit : editorTerrainStamp(terrain, brush, stamp)) {
      before.try_emplace(edit.index, edit.before);
      terrain.heights[edit.index] = edit.after;
      changed = true;
    }
  }
  return changed;
}

std::vector<EditorTerrainSampleEdit> EditorTerrainStroke::changes(
    const PrototypeTerrain& terrain) const {
  std::vector<EditorTerrainSampleEdit> edits;
  for (const auto& [sample, first] : before) {
    const float last = terrain.heights[sample];
    if (first != last) edits.push_back({sample, first, last});
  }
  return edits;
}
