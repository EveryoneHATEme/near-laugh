#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "editor/editor_terrain.hpp"

namespace {
PrototypeTerrain flatTerrain() {
  PrototypeTerrain terrain;
  terrain.sample_spacing = prototype_terrain_sample_spacing;
  return terrain;
}
void stamp(PrototypeTerrain& terrain, const EditorTerrainBrush& brush,
           WorldPosition center) {
  for (const auto& edit : editorTerrainStamp(terrain, brush, center))
    terrain.heights[edit.index] = edit.after;
}
constexpr std::size_t sample(int x, int z) {
  return static_cast<std::size_t>(z) * prototype_terrain_sample_count + x;
}
}  // namespace

TEST(EditorTerrain, ControlsAcceptBoundsAndRejectInvalidFieldsTransactionally) {
  struct Field {
    float EditorTerrainBrush::* member;
    float low, high;
    const char* name;
  };
  for (const Field field :
       {Field{&EditorTerrainBrush::radius, 0.5F, 8.0F, "radius"},
        Field{&EditorTerrainBrush::strength, 0.01F, 1.0F, "strength"},
        Field{&EditorTerrainBrush::smooth_strength, 0.0F, 1.0F, "strength"},
        Field{&EditorTerrainBrush::falloff, 0.0F, 1.0F, "falloff"}}) {
    for (const float value : {field.low, field.high}) {
      EditorTerrainBrush current, candidate;
      candidate.*field.member = value;
      std::string error;
      EXPECT_TRUE(commitEditorBrush(current, candidate, error));
      EXPECT_EQ(current, candidate);
      EXPECT_TRUE(error.empty());
    }
    for (const float value :
         {std::nextafter(field.low, -INFINITY),
          std::nextafter(field.high, INFINITY),
          std::numeric_limits<float>::quiet_NaN(), INFINITY, -INFINITY}) {
      EditorTerrainBrush current, candidate;
      const auto before = current;
      candidate.*field.member = value;
      std::string error;
      EXPECT_FALSE(commitEditorBrush(current, candidate, error));
      EXPECT_EQ(current, before);
      EXPECT_NE(error.find(field.name), std::string::npos);
    }
  }
  for (const auto mode : {EditorBrushMode::Raise, EditorBrushMode::Lower,
                          EditorBrushMode::Smooth}) {
    EditorTerrainBrush brush;
    brush.mode = mode;
    EXPECT_TRUE(editorBrushFieldError(brush).empty());
  }
}

TEST(EditorTerrain, FalloffHasFixedCenterInteriorBoundaryAndOutsideWeights) {
  EXPECT_DOUBLE_EQ(editorBrushWeight(0, 2, 1), 1);
  EXPECT_DOUBLE_EQ(editorBrushWeight(1, 2, 1), 0.5);
  EXPECT_DOUBLE_EQ(editorBrushWeight(1, 2, 0.5), 0.75);
  EXPECT_DOUBLE_EQ(editorBrushWeight(1.99, 2, 0), 1);
  EXPECT_DOUBLE_EQ(editorBrushWeight(2, 2, 0), 0);
  EXPECT_DOUBLE_EQ(editorBrushWeight(2, 2, 1), 0);
  EXPECT_DOUBLE_EQ(editorBrushWeight(3, 2, 1), 0);
}

TEST(EditorTerrain,
     RaiseAndLowerAreExactDeterministicAndBoundedAtTerrainEdges) {
  for (const auto mode : {EditorBrushMode::Raise, EditorBrushMode::Lower}) {
    auto terrain = flatTerrain();
    EditorTerrainBrush brush{mode, 1, 0.25F, 0.5F, 1};
    const auto edits = editorTerrainStamp(terrain, brush, {0, 0, 0});
    ASSERT_FALSE(edits.empty());
    EXPECT_TRUE(std::is_sorted(edits.begin(), edits.end(), [](auto a, auto b) {
      return a.index < b.index;
    }));
    const auto same = editorTerrainStamp(terrain, brush, {0, 0, 0});
    EXPECT_EQ(edits, same);
    auto repeated = terrain;
    stamp(terrain, brush, {0, 0, 0});
    stamp(repeated, brush, {0, 0, 0});
    EXPECT_EQ(terrain, repeated);
    const float sign = mode == EditorBrushMode::Raise ? 1.0F : -1.0F;
    EXPECT_FLOAT_EQ(terrain.heights[0], sign * 0.25F);
    EXPECT_FLOAT_EQ(terrain.heights[1], sign * 0.125F);
    EXPECT_EQ(terrain.heights[2], 0);
    for (int z = 0; z <= 96; ++z) {
      for (int x = 0; x <= 96; ++x) {
        if (std::hypot(x * 0.5, z * 0.5) >= 1)
          EXPECT_EQ(terrain.heights[sample(x, z)], 0);
        EXPECT_TRUE(std::isfinite(terrain.heights[sample(x, z)]));
      }
    }
    EXPECT_TRUE(editorTerrainStamp(terrain, brush, {-10, 0, -10}).empty());
  }
}

TEST(EditorTerrain,
     SmoothingUsesPreStampWeightedNeighborhoodAndClampedBorders) {
  auto terrain = flatTerrain();
  terrain.heights[sample(0, 0)] = 16;
  EditorTerrainBrush brush{EditorBrushMode::Smooth, 2, 0.1F, 1, 0};
  const auto before = terrain;
  const auto edits = editorTerrainStamp(terrain, brush, {0, 0, 0});
  auto reversed = terrain;
  for (auto it = edits.rbegin(); it != edits.rend(); ++it)
    reversed.heights[it->index] = it->after;
  stamp(terrain, brush, {0, 0, 0});
  EXPECT_EQ(terrain, reversed);
  EXPECT_FLOAT_EQ(terrain.heights[sample(0, 0)], 9);
  EXPECT_FLOAT_EQ(terrain.heights[sample(1, 0)], 3);
  EXPECT_FLOAT_EQ(terrain.heights[sample(1, 1)], 1);
  EXPECT_EQ(terrain.origin, before.origin);
  EXPECT_EQ(terrain.sample_spacing, before.sample_spacing);
  EXPECT_EQ(terrain.heights[sample(4, 0)], before.heights[sample(4, 0)]);

  // An interior impulse gives the symmetric 3x3 kernel, which an in-place
  // row traversal would bias toward the later samples.
  terrain = flatTerrain();
  terrain.heights[sample(40, 40)] = 16;
  stamp(terrain, brush, {20, 0, 20});
  EXPECT_FLOAT_EQ(terrain.heights[sample(40, 40)], 4);
  EXPECT_FLOAT_EQ(terrain.heights[sample(39, 40)], 2);
  EXPECT_EQ(terrain.heights[sample(39, 40)], terrain.heights[sample(41, 40)]);
  EXPECT_EQ(terrain.heights[sample(40, 39)], terrain.heights[sample(40, 41)]);
  EXPECT_FLOAT_EQ(terrain.heights[sample(41, 41)], 1);
  brush.smooth_strength = 0;
  EXPECT_TRUE(editorTerrainStamp(terrain, brush, {20, 0, 20}).empty());
}

TEST(EditorTerrain, NonFiniteInputAndResultsNeverProduceInvalidWrites) {
  auto terrain = flatTerrain();
  terrain.heights[0] = std::numeric_limits<float>::infinity();
  terrain.heights[1] = std::numeric_limits<float>::max();
  for (auto mode : {EditorBrushMode::Raise, EditorBrushMode::Lower,
                    EditorBrushMode::Smooth}) {
    EditorTerrainBrush brush;
    brush.mode = mode;
    for (const auto& edit : editorTerrainStamp(terrain, brush, {0, 0, 0})) {
      EXPECT_NE(edit.index, 0U);
      EXPECT_TRUE(std::isfinite(edit.after));
    }
    EXPECT_TRUE(editorTerrainStamp(terrain, brush, {NAN, 0, 0}).empty());
  }
}

TEST(EditorTerrain, PathCarriesRemainderAndIsIndependentOfInputBatching) {
  const std::vector<WorldPosition> path{{10, 0, 10}, {10.125F, 1, 10},
                                        {11, 3, 10}, {11, 2, 10.125F},
                                        {11, 0, 12}, {11, 5, 12}};
  const auto run = [&](std::size_t batch_size) {
    EditorTerrainPath resampler;
    std::vector<WorldPosition> result;
    for (std::size_t batch = 0; batch < path.size(); batch += batch_size) {
      for (std::size_t i = batch; i < std::min(batch + batch_size, path.size());
           ++i) {
        const auto stamps = resampler.advance(path[i]);
        result.insert(result.end(), stamps.begin(), stamps.end());
      }
    }
    return result;
  };
  const auto stamps = run(1);
  EXPECT_EQ(stamps, run(3));
  EXPECT_EQ(stamps, run(path.size()));
  EXPECT_EQ(stamps.size(), 13U);
  EditorTerrainPath coarse;
  std::vector<WorldPosition> coarse_stamps;
  for (const auto p : {path[0], path[2], path[4]}) {
    const auto next = coarse.advance(p);
    coarse_stamps.insert(coarse_stamps.end(), next.begin(), next.end());
  }
  EXPECT_EQ(stamps, coarse_stamps);
  auto a = flatTerrain(), b = a;
  for (auto p : stamps) stamp(a, {}, p);
  for (auto p : coarse_stamps) stamp(b, {}, p);
  EXPECT_EQ(a, b);
  EXPECT_TRUE(coarse.advance(std::nullopt).empty());
  EXPECT_EQ(coarse.advance({{30, 0, 30}}).size(), 1U);
}

TEST(EditorTerrain, OverlappingStampsRecordFirstBeforeAndFinalAfterOnlyOnce) {
  auto terrain = flatTerrain();
  const auto original = terrain;
  EditorTerrainStroke stroke;
  EXPECT_TRUE(stroke.advance(terrain, {{20, 0, 20}}));
  EXPECT_TRUE(stroke.advance(terrain, {{21, 0, 20}}));
  EXPECT_FALSE(stroke.advance(terrain, {{21, 10, 20}}));
  const auto edits = stroke.changes(terrain);
  ASSERT_FALSE(edits.empty());
  std::size_t previous = 0;
  for (const auto& edit : edits) {
    EXPECT_GT(edit.index, previous);
    previous = edit.index;
    EXPECT_EQ(edit.before, original.heights[edit.index]);
    EXPECT_EQ(edit.after, terrain.heights[edit.index]);
    terrain.heights[edit.index] = edit.before;
  }
  EXPECT_EQ(terrain, original);
  EXPECT_TRUE(stroke.changes(terrain).empty());
}
