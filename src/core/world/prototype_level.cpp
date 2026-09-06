#include "core/world/prototype_level.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/world/light_switch.hpp"

namespace {
std::size_t terrainSampleIndex(std::size_t sample_x,
                               std::size_t sample_z) noexcept {
  return sample_z * prototype_terrain_sample_count + sample_x;
}

float terrainHeight(const PrototypeTerrain& terrain, std::size_t sample_x,
                    std::size_t sample_z) noexcept {
  return terrain.origin.y +
         terrain.heights[terrainSampleIndex(sample_x, sample_z)];
}

bool terrainTriangleHasSupportedSlope(const PrototypeTerrain& terrain,
                                      std::size_t sample_x,
                                      std::size_t sample_z,
                                      bool first) noexcept {
  const WorldPosition p00 =
      prototypeTerrainSamplePosition(terrain, sample_x, sample_z);
  const WorldPosition p01 =
      prototypeTerrainSamplePosition(terrain, sample_x, sample_z + 1);
  const WorldPosition p11 =
      prototypeTerrainSamplePosition(terrain, sample_x + 1, sample_z + 1);
  const WorldPosition p10 =
      prototypeTerrainSamplePosition(terrain, sample_x + 1, sample_z);
  const WorldPosition& a = p00;
  const WorldPosition& b = first ? p01 : p11;
  const WorldPosition& c = first ? p11 : p10;
  const float ab_x = b.x - a.x;
  const float ab_y = b.y - a.y;
  const float ab_z = b.z - a.z;
  const float ac_x = c.x - a.x;
  const float ac_y = c.y - a.y;
  const float ac_z = c.z - a.z;
  const float normal_x = ab_y * ac_z - ab_z * ac_y;
  const float normal_y = ab_z * ac_x - ab_x * ac_z;
  const float normal_z = ab_x * ac_y - ab_y * ac_x;
  const float normal_length = std::sqrt(
      normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
  if (!(normal_length > 0.0F) || !std::isfinite(normal_length)) {
    return false;
  }
  const float minimum_normal_y =
      std::cos(prototype_terrain_maximum_slope_degrees *
               std::numbers::pi_v<float> / 180.0F);
  return normal_y / normal_length >= minimum_normal_y;
}

bool overlaps(float first_min, float first_max, float second_min,
              float second_max) noexcept {
  return first_min < second_max && first_max > second_min;
}

constexpr float support_tolerance = 0.0001F;

bool finite(WorldPosition p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

bool finiteBounds(WorldPosition p, WorldExtent e) {
  return finite({p.x - e.x, p.y - e.y, p.z - e.z}) &&
         finite({p.x + e.x, p.y + e.y, p.z + e.z}) &&
         finite({2 * e.x, 2 * e.y, 2 * e.z});
}

// Local double-precision geometry keeps validation independent of Jolt.
struct Vec {
  double x, y, z;
  Vec operator+(Vec b) const { return {x + b.x, y + b.y, z + b.z}; }
  Vec operator-(Vec b) const { return {x - b.x, y - b.y, z - b.z}; }
  Vec operator*(double s) const { return {x * s, y * s, z * s}; }
};
Vec vec(WorldPosition p) { return {p.x, p.y, p.z}; }
double dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec cross(Vec a, Vec b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double pointTriangleDistanceSquared(Vec p, Vec a, Vec b, Vec c) {
  const Vec ab = b - a, ac = c - a, ap = p - a;
  const double d1 = dot(ab, ap), d2 = dot(ac, ap);
  if (d1 <= 0 && d2 <= 0) return dot(ap, ap);
  const Vec bp = p - b;
  const double d3 = dot(ab, bp), d4 = dot(ac, bp);
  if (d3 >= 0 && d4 <= d3) return dot(bp, bp);
  const double vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0) {
    const Vec delta = p - (a + ab * (d1 / (d1 - d3)));
    return dot(delta, delta);
  }
  const Vec cp = p - c;
  const double d5 = dot(ab, cp), d6 = dot(ac, cp);
  if (d6 >= 0 && d5 <= d6) return dot(cp, cp);
  const double vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0) {
    const Vec delta = p - (a + ac * (d2 / (d2 - d6)));
    return dot(delta, delta);
  }
  const double va = d3 * d6 - d5 * d4;
  if (va <= 0 && d4 >= d3 && d5 >= d6) {
    const Vec delta = p - (b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6))));
    return dot(delta, delta);
  }
  const double denominator = va + vb + vc;
  if (!(denominator > 0)) return 0;  // Degenerate data is never admitted.
  const Vec delta = p - (a + ab * (vb / denominator) + ac * (vc / denominator));
  return dot(delta, delta);
}
double segmentDistanceSquared(Vec p, Vec q, Vec a, Vec b) {
  const Vec u = q - p, v = b - a, w = p - a;
  const double uu = dot(u, u), vv = dot(v, v), uv = dot(u, v);
  const double uw = dot(u, w), vw = dot(v, w);
  if (!(vv > 0)) return dot(w, w);
  const double denominator = uu * vv - uv * uv;
  double s = denominator > 0
                 ? std::clamp((uv * vw - vv * uw) / denominator, 0.0, 1.0)
                 : 0;
  double t = (uv * s + vw) / vv;
  if (t < 0) {
    t = 0;
    s = uu > 0 ? std::clamp(-uw / uu, 0.0, 1.0) : 0;
  }
  if (t > 1) {
    t = 1;
    s = uu > 0 ? std::clamp((uv - uw) / uu, 0.0, 1.0) : 0;
  }
  const Vec delta = w + u * s - v * t;
  return dot(delta, delta);
}
double segmentTriangleDistanceSquared(Vec p, Vec q, Vec a, Vec b, Vec c) {
  const Vec normal = cross(b - a, c - a);
  const double denominator = dot(normal, q - p);
  if (std::abs(denominator) > 1e-15) {
    const double t = dot(normal, a - p) / denominator;
    if (t >= 0 && t <= 1 &&
        pointTriangleDistanceSquared(p + (q - p) * t, a, b, c) < 1e-16)
      return 0;
  }
  return std::min({pointTriangleDistanceSquared(p, a, b, c),
                   pointTriangleDistanceSquared(q, a, b, c),
                   segmentDistanceSquared(p, q, a, b),
                   segmentDistanceSquared(p, q, b, c),
                   segmentDistanceSquared(p, q, c, a)});
}

bool spawnSupported(const PrototypeTerrain* terrain,
                    std::span<const PrototypeSolid> solids, WorldPosition p) {
  if (!finite(p)) return false;
  if (terrain && prototypeTerrainContains(*terrain, p.x, p.z)) {
    const float h = prototypeTerrainHeightAt(*terrain, p.x, p.z);
    if (p.y < h - support_tolerance) return false;
    if (std::abs(p.y - h) < support_tolerance) return true;
  }
  return std::any_of(solids.begin(), solids.end(), [p](const auto& solid) {
    return prototypeSolidIsValid(solid) &&
           std::abs(p.x - solid.center.x) <= solid.half_extent.x &&
           std::abs(p.z - solid.center.z) <= solid.half_extent.z &&
           std::abs(p.y - (solid.center.y + solid.half_extent.y)) <
               support_tolerance;
  });
}

bool spawnClear(const PrototypeTerrain* terrain,
                std::span<const PrototypeSolid> solids,
                const PrototypeStaticProp& prop, WorldPosition p, float radius,
                float height) {
  if (!finite(p) || !std::isfinite(radius) || !std::isfinite(height) ||
      !(radius > 0) || !(height >= 2 * radius) || !std::isfinite(p.y + height))
    return false;
  const float min_y = p.y + support_tolerance;
  const float max_y = p.y + height - support_tolerance;
  for (const auto& solid : solids) {
    if (overlaps(p.x - radius, p.x + radius,
                 solid.center.x - solid.half_extent.x,
                 solid.center.x + solid.half_extent.x) &&
        overlaps(min_y, max_y, solid.center.y - solid.half_extent.y,
                 solid.center.y + solid.half_extent.y) &&
        overlaps(p.z - radius, p.z + radius,
                 solid.center.z - solid.half_extent.z,
                 solid.center.z + solid.half_extent.z))
      return false;
  }
  const auto center = prototypeStaticPropProxyWorldCenter(prop);
  const auto extent = prototypeStaticPropProxyWorldHalfExtent(prop);
  if (overlaps(min_y, max_y, center.y - extent.y, center.y + extent.y)) {
    const double yaw =
        static_cast<double>(prop.yaw_degrees) * std::numbers::pi / 180;
    const double co = std::cos(yaw), si = std::sin(yaw);
    const double dx = static_cast<double>(p.x) - center.x,
                 dz = static_cast<double>(p.z) - center.z;
    const double projected_radius = radius * (std::abs(co) + std::abs(si));
    if (std::abs(dx) <
            radius + std::abs(co) * extent.x + std::abs(si) * extent.z &&
        std::abs(dz) <
            radius + std::abs(si) * extent.x + std::abs(co) * extent.z &&
        std::abs(co * dx - si * dz) < extent.x + projected_radius &&
        std::abs(si * dx + co * dz) < extent.z + projected_radius)
      return false;
  }
  if (!terrain) return true;
  const bool on_terrain =
      prototypeTerrainContains(*terrain, p.x, p.z) &&
      std::abs(p.y - prototypeTerrainHeightAt(*terrain, p.x, p.z)) <
          support_tolerance;
  const Vec top{p.x, static_cast<double>(p.y) + height - radius, p.z};
  // A foot authored on a supported slope starts with ordinary bottom-cap
  // contact. Allow its bounded vertical separation (r / cos(slope) - r),
  // while keeping the standing top and structural clearance unchanged.
  const double ground_radius =
      on_terrain ? radius / std::cos(prototype_terrain_maximum_slope_degrees *
                                     std::numbers::pi / 180)
                 : radius;
  const Vec bottom{
      p.x, std::min(top.y, static_cast<double>(p.y) + ground_radius), p.z};
  for (std::size_t z = 0; z < prototype_terrain_cell_count; ++z) {
    for (std::size_t x = 0; x < prototype_terrain_cell_count; ++x) {
      const Vec a = vec(prototypeTerrainSamplePosition(*terrain, x, z));
      const Vec c = vec(prototypeTerrainSamplePosition(*terrain, x + 1, z + 1));
      if (c.x < p.x - radius || a.x > p.x + radius || c.z < p.z - radius ||
          a.z > p.z + radius)
        continue;
      const Vec b = vec(prototypeTerrainSamplePosition(*terrain, x, z + 1));
      const Vec d = vec(prototypeTerrainSamplePosition(*terrain, x + 1, z));
      for (const auto& triangle :
           {std::array<Vec, 3>{a, b, c}, std::array<Vec, 3>{a, c, d}}) {
        const double distance = segmentTriangleDistanceSquared(
            bottom, top, triangle[0], triangle[1], triangle[2]);
        if (distance <
            (radius - support_tolerance) * (radius - support_tolerance))
          return false;
      }
    }
  }
  return true;
}

bool solidKindIsValid(PrototypeSolidKind kind) noexcept {
  switch (kind) {
    case PrototypeSolidKind::Floor:
    case PrototypeSolidKind::Boundary:
    case PrototypeSolidKind::Obstacle:
    case PrototypeSolidKind::WalkableStep:
    case PrototypeSolidKind::LowClearance:
      return true;
  }
  return false;
}

PrototypeSurface expectedSurface(PrototypeSolidKind kind) noexcept {
  switch (kind) {
    case PrototypeSolidKind::Floor:
      return PrototypeSurface::Floor;
    case PrototypeSolidKind::Boundary:
      return PrototypeSurface::Boundary;
    case PrototypeSolidKind::Obstacle:
    case PrototypeSolidKind::WalkableStep:
    case PrototypeSolidKind::LowClearance:
      return PrototypeSurface::Obstacle;
  }
  return static_cast<PrototypeSurface>(prototype_surface_count);
}

void addValidation(std::vector<LevelDiagnostic>& diagnostics,
                   const std::filesystem::path& source_path,
                   std::string document_path, std::string message) {
  diagnostics.push_back({LevelDiagnosticCategory::Validation, source_path,
                         std::move(document_path), std::move(message)});
}
}  // namespace

PrototypeLevel::PrototypeLevel(LevelDocument document)
    : terrain_(std::move(document.terrain)),
      solids_(std::move(document.solids)),
      entries_(std::move(document.entries)),
      default_entry_(std::move(document.default_entry)),
      environment_light_(std::move(document.environment_light)),
      static_prop_(std::move(document.static_prop)),
      light_switch_(std::move(document.light_switch)) {}

bool levelEntryIdIsValid(std::string_view id) noexcept {
  return !id.empty() && id.size() <= level_maximum_entry_id_length &&
         id.front() >= 'a' && id.front() <= 'z' &&
         std::all_of(id.begin(), id.end(), [](char c) {
           return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
         });
}

const LevelEntry* findLevelEntry(const LevelDocument& document,
                                 std::string_view id) noexcept {
  const auto it =
      std::find_if(document.entries.begin(), document.entries.end(),
                   [id](const auto& entry) { return entry.id == id; });
  return it == document.entries.end() ? nullptr : &*it;
}

const LevelEntry* PrototypeLevel::entry(std::string_view id) const noexcept {
  const auto it =
      std::find_if(entries_.begin(), entries_.end(),
                   [id](const auto& entry) { return entry.id == id; });
  return it == entries_.end() ? nullptr : &*it;
}

PrototypeLevel makePrototypeLevel(const LevelDocument& document) {
  const std::vector<LevelDiagnostic> diagnostics =
      validateLevelDocument(document);
  if (!diagnostics.empty()) {
    throw std::invalid_argument(formatLevelDiagnostics(diagnostics));
  }
  return PrototypeLevel(document);
}

PrototypeLevel loadPrototypeLevel(const std::filesystem::path& path) {
  LevelDocumentLoadResult result = loadLevelDocument(path);
  if (!result) {
    throw std::runtime_error(formatLevelDiagnostics(result.diagnostics));
  }
  const std::vector<LevelDiagnostic> diagnostics = validateLevelDocument(
      *result.document, std::filesystem::absolute(path).lexically_normal());
  if (!diagnostics.empty()) {
    throw std::runtime_error(formatLevelDiagnostics(diagnostics));
  }
  return makePrototypeLevel(*result.document);
}

bool prototypeEnvironmentLightIsValid(
    const PrototypeEnvironmentLight& light) noexcept {
  const bool point_lights_are_valid = std::all_of(
      light.point_lights.begin(), light.point_lights.end(),
      [](const PrototypePointLight& point_light) {
        return std::isfinite(point_light.position.x) &&
               std::isfinite(point_light.position.y) &&
               std::isfinite(point_light.position.z) &&
               std::all_of(point_light.color.begin(), point_light.color.end(),
                           [](float component) {
                             return std::isfinite(component) &&
                                    component >= 0.0F;
                           }) &&
               std::isfinite(point_light.intensity) &&
               point_light.intensity > 0.0F &&
               std::isfinite(point_light.radius) && point_light.radius > 0.0F;
      });
  return point_lights_are_valid && std::isfinite(light.ambient_intensity) &&
         light.ambient_intensity >= 0.0F &&
         light.ambient_intensity <= prototype_maximum_ambient_intensity;
}

bool prototypeSurfaceIsValid(PrototypeSurface surface) noexcept {
  switch (surface) {
    case PrototypeSurface::Floor:
    case PrototypeSurface::Boundary:
    case PrototypeSurface::Obstacle:
      return true;
  }
  return false;
}

bool prototypeSolidIsValid(const PrototypeSolid& solid) noexcept {
  return std::isfinite(solid.center.x) && std::isfinite(solid.center.y) &&
         std::isfinite(solid.center.z) && std::isfinite(solid.half_extent.x) &&
         std::isfinite(solid.half_extent.y) &&
         std::isfinite(solid.half_extent.z) && solid.half_extent.x > 0.0F &&
         solid.half_extent.y > 0.0F && solid.half_extent.z > 0.0F &&
         finiteBounds(solid.center, solid.half_extent) &&
         solidKindIsValid(solid.kind) &&
         prototypeSurfaceIsValid(solid.surface) &&
         solid.surface == expectedSurface(solid.kind);
}

WorldPosition prototypeStaticPropProxyWorldCenter(
    const PrototypeStaticProp& prop) noexcept {
  const float yaw = prop.yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  const float cosine = std::cos(yaw);
  const float sine = std::sin(yaw);
  const float local_x = prop.box_proxy_center.x * prop.uniform_scale;
  const float local_y = prop.box_proxy_center.y * prop.uniform_scale;
  const float local_z = prop.box_proxy_center.z * prop.uniform_scale;
  return {prop.translation.x + cosine * local_x + sine * local_z,
          prop.translation.y + local_y,
          prop.translation.z - sine * local_x + cosine * local_z};
}

WorldExtent prototypeStaticPropProxyWorldHalfExtent(
    const PrototypeStaticProp& prop) noexcept {
  return {prop.box_proxy_half_extent.x * prop.uniform_scale,
          prop.box_proxy_half_extent.y * prop.uniform_scale,
          prop.box_proxy_half_extent.z * prop.uniform_scale};
}

bool prototypeStaticPropIsValid(const PrototypeStaticProp& prop) noexcept {
  const WorldPosition center = prototypeStaticPropProxyWorldCenter(prop);
  const WorldExtent half_extent = prototypeStaticPropProxyWorldHalfExtent(prop);
  const double yaw =
      static_cast<double>(prop.yaw_degrees) * std::numbers::pi / 180;
  const float c = static_cast<float>(std::abs(std::cos(yaw)));
  const float s = static_cast<float>(std::abs(std::sin(yaw)));
  const WorldExtent bounds{c * half_extent.x + s * half_extent.z, half_extent.y,
                           s * half_extent.x + c * half_extent.z};
  return std::isfinite(prop.translation.x) &&
         std::isfinite(prop.translation.y) &&
         std::isfinite(prop.translation.z) && std::isfinite(prop.yaw_degrees) &&
         std::isfinite(prop.uniform_scale) && prop.uniform_scale > 0.0F &&
         prop.surface == PrototypeSurface::Obstacle &&
         std::isfinite(prop.box_proxy_center.x) &&
         std::isfinite(prop.box_proxy_center.y) &&
         std::isfinite(prop.box_proxy_center.z) && std::isfinite(center.x) &&
         std::isfinite(center.y) && std::isfinite(center.z) &&
         std::isfinite(half_extent.x) && std::isfinite(half_extent.y) &&
         std::isfinite(half_extent.z) && half_extent.x > 0.0F &&
         half_extent.y > 0.0F && half_extent.z > 0.0F &&
         finiteBounds(center, bounds);
}

WorldPosition prototypeTerrainSamplePosition(const PrototypeTerrain& terrain,
                                             std::size_t sample_x,
                                             std::size_t sample_z) noexcept {
  if (sample_x >= prototype_terrain_sample_count ||
      sample_z >= prototype_terrain_sample_count) {
    return {std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN()};
  }
  return {
      terrain.origin.x + static_cast<float>(sample_x) * terrain.sample_spacing,
      terrainHeight(terrain, sample_x, sample_z),
      terrain.origin.z + static_cast<float>(sample_z) * terrain.sample_spacing};
}

bool prototypeTerrainContains(const PrototypeTerrain& terrain, float x,
                              float z) noexcept {
  if (!std::isfinite(x) || !std::isfinite(z) ||
      !(terrain.sample_spacing > 0.0F) ||
      !std::isfinite(terrain.sample_spacing)) {
    return false;
  }
  const float maximum_x =
      terrain.origin.x +
      static_cast<float>(prototype_terrain_cell_count) * terrain.sample_spacing;
  const float maximum_z =
      terrain.origin.z +
      static_cast<float>(prototype_terrain_cell_count) * terrain.sample_spacing;
  return x >= terrain.origin.x && x <= maximum_x && z >= terrain.origin.z &&
         z <= maximum_z;
}

float prototypeTerrainHeightAt(const PrototypeTerrain& terrain, float x,
                               float z) noexcept {
  if (!prototypeTerrainContains(terrain, x, z)) {
    return std::numeric_limits<float>::quiet_NaN();
  }
  const float local_x = (x - terrain.origin.x) / terrain.sample_spacing;
  const float local_z = (z - terrain.origin.z) / terrain.sample_spacing;
  const std::size_t sample_x =
      std::min(static_cast<std::size_t>(std::floor(local_x)),
               prototype_terrain_cell_count - 1);
  const std::size_t sample_z =
      std::min(static_cast<std::size_t>(std::floor(local_z)),
               prototype_terrain_cell_count - 1);
  const float fraction_x =
      std::min(local_x - static_cast<float>(sample_x), 1.0F);
  const float fraction_z =
      std::min(local_z - static_cast<float>(sample_z), 1.0F);
  const float h00 = terrainHeight(terrain, sample_x, sample_z);
  const float h01 = terrainHeight(terrain, sample_x, sample_z + 1);
  const float h11 = terrainHeight(terrain, sample_x + 1, sample_z + 1);
  const float h10 = terrainHeight(terrain, sample_x + 1, sample_z);
  if (fraction_x <= fraction_z) {
    return h00 * (1.0F - fraction_z) + h01 * (fraction_z - fraction_x) +
           h11 * fraction_x;
  }
  return h00 * (1.0F - fraction_x) + h11 * fraction_z +
         h10 * (fraction_x - fraction_z);
}

float prototypeTerrainMinimumHeight(const PrototypeTerrain& terrain) noexcept {
  float minimum = std::numeric_limits<float>::infinity();
  for (const float height : terrain.heights) {
    minimum = std::min(minimum, terrain.origin.y + height);
  }
  return minimum;
}

std::vector<LevelDiagnostic> validateLevelDocument(
    const LevelDocument& document, const std::filesystem::path& source_path) {
  std::vector<LevelDiagnostic> diagnostics;
  if (document.version != level_format_version) {
    addValidation(diagnostics, source_path, "version",
                  "unsupported level format version");
  }

  if (document.terrain) {
    const PrototypeTerrain& terrain = *document.terrain;
    if (!std::isfinite(terrain.origin.x)) {
      addValidation(diagnostics, source_path, "terrain.origin.x",
                    "must be finite");
    }
    if (!std::isfinite(terrain.origin.y)) {
      addValidation(diagnostics, source_path, "terrain.origin.y",
                    "must be finite");
    }
    if (!std::isfinite(terrain.origin.z)) {
      addValidation(diagnostics, source_path, "terrain.origin.z",
                    "must be finite");
    }
    if (!std::isfinite(terrain.sample_spacing) ||
        terrain.sample_spacing != prototype_terrain_sample_spacing) {
      addValidation(diagnostics, source_path, "terrain.sample_spacing",
                    "must equal the fixed 0.5 metre spacing");
    }
    for (std::size_t index = 0; index < terrain.heights.size(); ++index) {
      if (!std::isfinite(terrain.heights[index])) {
        addValidation(diagnostics, source_path,
                      "terrain.heights[" + std::to_string(index) + "]",
                      "must be finite");
        diagnostics.back().terrain_location = TerrainDiagnosticLocation{
            index % prototype_terrain_sample_count,
            index / prototype_terrain_sample_count, std::nullopt};
      }
    }
    if (std::isfinite(terrain.sample_spacing) &&
        terrain.sample_spacing > 0.0F &&
        std::all_of(terrain.heights.begin(), terrain.heights.end(),
                    [](float height) { return std::isfinite(height); })) {
      for (std::size_t sample_z = 0; sample_z < prototype_terrain_cell_count;
           ++sample_z) {
        for (std::size_t sample_x = 0; sample_x < prototype_terrain_cell_count;
             ++sample_x) {
          for (const bool first : {true, false}) {
            if (!terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z,
                                                  first)) {
              addValidation(
                  diagnostics, source_path,
                  "terrain.cells[" + std::to_string(sample_z) + "][" +
                      std::to_string(sample_x) + "]",
                  first ? "first triangle exceeds the supported slope"
                        : "second triangle exceeds the supported slope");
              diagnostics.back().terrain_location = TerrainDiagnosticLocation{
                  sample_x, sample_z, first ? 0U : 1U};
            }
          }
        }
      }
    }
  }

  if (document.solids.empty()) {
    addValidation(diagnostics, source_path, "solids",
                  "must contain at least one structural solid");
  }
  if (document.solids.size() > level_maximum_solid_count) {
    addValidation(diagnostics, source_path, "solids",
                  "exceeds the 240-solid limit");
  }
  for (std::size_t index = 0; index < document.solids.size(); ++index) {
    const PrototypeSolid& solid = document.solids[index];
    const std::string path = "solids[" + std::to_string(index) + "]";
    const std::array<std::pair<const char*, float>, 6> finite_values{{
        {"center.x", solid.center.x},
        {"center.y", solid.center.y},
        {"center.z", solid.center.z},
        {"half_extent.x", solid.half_extent.x},
        {"half_extent.y", solid.half_extent.y},
        {"half_extent.z", solid.half_extent.z},
    }};
    for (const auto& [field, value] : finite_values) {
      if (!std::isfinite(value)) {
        addValidation(diagnostics, source_path, path + "." + field,
                      "must be finite");
      }
    }
    if (!(solid.half_extent.x > 0.0F) || !(solid.half_extent.y > 0.0F) ||
        !(solid.half_extent.z > 0.0F)) {
      addValidation(diagnostics, source_path, path + ".half_extent",
                    "all components must be positive");
    }
    if (!finiteBounds(solid.center, solid.half_extent))
      addValidation(diagnostics, source_path, path + ".bounds",
                    "transformed bounds must remain finite");
    if (!solidKindIsValid(solid.kind)) {
      addValidation(diagnostics, source_path, path + ".kind",
                    "is not a supported solid kind");
    }
    if (!prototypeSurfaceIsValid(solid.surface)) {
      addValidation(diagnostics, source_path, path + ".surface",
                    "is not a supported surface role");
    } else if (solidKindIsValid(solid.kind) &&
               solid.surface != expectedSurface(solid.kind)) {
      addValidation(diagnostics, source_path, path + ".surface",
                    "does not match the fixed role for this solid kind");
    }
  }

  const PrototypeEnvironmentLight& light = document.environment_light;
  for (std::size_t index = 0; index < light.point_lights.size(); ++index) {
    const PrototypePointLight& point = light.point_lights[index];
    const std::string path =
        "environment_light.point_lights[" + std::to_string(index) + "]";
    if (!std::isfinite(point.position.x) || !std::isfinite(point.position.y) ||
        !std::isfinite(point.position.z)) {
      addValidation(diagnostics, source_path, path + ".position",
                    "all components must be finite");
    }
    for (std::size_t component = 0; component < point.color.size();
         ++component) {
      if (!std::isfinite(point.color[component]) ||
          point.color[component] < 0.0F) {
        addValidation(diagnostics, source_path,
                      path + ".color[" + std::to_string(component) + "]",
                      "must be finite and non-negative");
      }
    }
    if (!std::isfinite(point.intensity) || !(point.intensity > 0.0F)) {
      addValidation(diagnostics, source_path, path + ".intensity",
                    "must be finite and positive");
    }
    if (!std::isfinite(point.radius) || !(point.radius > 0.0F)) {
      addValidation(diagnostics, source_path, path + ".radius",
                    "must be finite and positive");
    }
  }
  if (!std::isfinite(light.ambient_intensity) ||
      light.ambient_intensity < 0.0F ||
      light.ambient_intensity > prototype_maximum_ambient_intensity) {
    addValidation(diagnostics, source_path,
                  "environment_light.ambient_intensity",
                  "must be finite and between 0.0 and 0.2");
  }

  const PrototypeStaticProp& prop = document.static_prop;
  if (!std::isfinite(prop.translation.x) ||
      !std::isfinite(prop.translation.y) ||
      !std::isfinite(prop.translation.z)) {
    addValidation(diagnostics, source_path, "static_prop.translation",
                  "all components must be finite");
  }
  if (!std::isfinite(prop.yaw_degrees)) {
    addValidation(diagnostics, source_path, "static_prop.yaw_degrees",
                  "must be finite");
  }
  if (!std::isfinite(prop.uniform_scale) || !(prop.uniform_scale > 0.0F)) {
    addValidation(diagnostics, source_path, "static_prop.uniform_scale",
                  "must be finite and positive");
  }
  if (prop.surface != PrototypeSurface::Obstacle) {
    addValidation(diagnostics, source_path, "static_prop.surface",
                  "must use the fixed obstacle surface");
  }
  if (!std::isfinite(prop.box_proxy_center.x) ||
      !std::isfinite(prop.box_proxy_center.y) ||
      !std::isfinite(prop.box_proxy_center.z)) {
    addValidation(diagnostics, source_path, "static_prop.box_proxy.center",
                  "all components must be finite");
  }
  if (!std::isfinite(prop.box_proxy_half_extent.x) ||
      !std::isfinite(prop.box_proxy_half_extent.y) ||
      !std::isfinite(prop.box_proxy_half_extent.z) ||
      !(prop.box_proxy_half_extent.x > 0.0F) ||
      !(prop.box_proxy_half_extent.y > 0.0F) ||
      !(prop.box_proxy_half_extent.z > 0.0F)) {
    addValidation(diagnostics, source_path, "static_prop.box_proxy.half_extent",
                  "all components must be finite and positive");
  }
  const WorldPosition proxy_center = prototypeStaticPropProxyWorldCenter(prop);
  const WorldExtent proxy_extent =
      prototypeStaticPropProxyWorldHalfExtent(prop);
  if (!prototypeStaticPropIsValid(prop))
    addValidation(
        diagnostics, source_path, "static_prop.box_proxy",
        "proxy and its transformed bounds must remain finite and positive");
  if (!std::isfinite(proxy_center.x) || !std::isfinite(proxy_center.y) ||
      !std::isfinite(proxy_center.z) || !std::isfinite(proxy_extent.x) ||
      !std::isfinite(proxy_extent.y) || !std::isfinite(proxy_extent.z)) {
    addValidation(diagnostics, source_path, "static_prop.box_proxy",
                  "world transform must remain finite");
  }

  if (document.entries.empty() ||
      document.entries.size() > level_maximum_entry_count)
    addValidation(diagnostics, source_path, "entries",
                  "must contain between 1 and 16 entries");
  if (!levelEntryIdIsValid(document.default_entry) ||
      !findLevelEntry(document, document.default_entry))
    addValidation(diagnostics, source_path, "default_entry",
                  "must identify an existing entry");
  const PrototypeTerrain* support_terrain =
      document.terrain && prototypeTerrainIsValid(*document.terrain)
          ? &*document.terrain
          : nullptr;
  for (std::size_t i = 0; i < document.entries.size(); ++i) {
    const auto& entry = document.entries[i];
    const std::string path = "entries[" + std::to_string(i) + "]";
    const std::string label =
        entry.id.empty() ? "" : "entry '" + entry.id + "': ";
    if (!levelEntryIdIsValid(entry.id))
      addValidation(diagnostics, source_path, path + ".id",
                    "must match [a-z][a-z0-9-]{0,63}");
    for (std::size_t j = 0; j < i; ++j) {
      if (document.entries[j].id == entry.id) {
        addValidation(diagnostics, source_path, path + ".id",
                      label + "duplicate identifier");
        break;
      }
    }
    if (!finite(entry.pose.foot_position))
      addValidation(diagnostics, source_path, path + ".foot_position",
                    label + "must be finite");
    if (!std::isfinite(entry.pose.yaw_degrees))
      addValidation(diagnostics, source_path, path + ".yaw_degrees",
                    label + "must be finite");
    if (!spawnSupported(support_terrain, document.solids,
                        entry.pose.foot_position))
      addValidation(diagnostics, source_path, path + ".foot_position",
                    label +
                        "must match a supporting terrain surface or solid top "
                        "at its authored height");
    if (!spawnClear(support_terrain, document.solids, prop,
                    entry.pose.foot_position, prototype_spawn_validation_radius,
                    prototype_spawn_validation_height))
      addValidation(
          diagnostics, source_path, path + ".foot_position",
          label + "standing player clearance overlaps blocking geometry");
  }

  if (document.light_switch) {
    const auto& value = *document.light_switch;
    if (!std::isfinite(value.position.x) || !std::isfinite(value.position.y) ||
        !std::isfinite(value.position.z))
      addValidation(diagnostics, source_path, "light_switch.position",
                    "all components must be finite");
    if (!std::isfinite(value.yaw_degrees))
      addValidation(diagnostics, source_path, "light_switch.yaw_degrees",
                    "must be finite");
    if (value.point_light_index >= prototype_point_light_count)
      addValidation(diagnostics, source_path, "light_switch.point_light_index",
                    "must select point light 0 or 1");
    for (const auto& corner : lightSwitchCorners(value)) {
      if (!std::isfinite(corner.x) || !std::isfinite(corner.y) ||
          !std::isfinite(corner.z)) {
        addValidation(diagnostics, source_path, "light_switch.position",
                      "transformed bounds must be finite");
        break;
      }
    }
  }
  return diagnostics;
}

bool prototypeTerrainIsValid(const PrototypeTerrain& terrain) noexcept {
  if (!std::isfinite(terrain.origin.x) || !std::isfinite(terrain.origin.y) ||
      !std::isfinite(terrain.origin.z) ||
      terrain.sample_spacing != prototype_terrain_sample_spacing ||
      !std::all_of(terrain.heights.begin(), terrain.heights.end(),
                   [](float height) { return std::isfinite(height); })) {
    return false;
  }
  for (std::size_t sample_z = 0; sample_z < prototype_terrain_cell_count;
       ++sample_z) {
    for (std::size_t sample_x = 0; sample_x < prototype_terrain_cell_count;
         ++sample_x) {
      if (!terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z,
                                            true) ||
          !terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z,
                                            false)) {
        return false;
      }
    }
  }
  return true;
}

bool prototypeLevelIsValid(const PrototypeLevel& level) {
  const LevelDocument document{level_format_version,   level.terrain(),
                               level.solids(),         level.entries(),
                               level.defaultEntryId(), level.environmentLight(),
                               level.staticProp(),     level.lightSwitch()};
  return validateLevelDocument(document).empty();
}

bool prototypeSpawnIsClear(const PrototypeLevel& level, float player_radius,
                           float player_height) noexcept {
  return spawnClear(level.terrain() ? &*level.terrain() : nullptr,
                    level.solids(), level.staticProp(),
                    level.playerSpawn().foot_position, player_radius,
                    player_height);
}
