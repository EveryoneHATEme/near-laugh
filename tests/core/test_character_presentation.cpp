#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "core/render/character_presentation.hpp"

namespace {
constexpr AnimationMatrix identity{1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};

std::shared_ptr<CharacterAsset> triangleAsset() {
  auto asset = std::make_shared<CharacterAsset>();
  asset->source = "presentation triangle";
  asset->skeleton_identity = 42;
  asset->nodes = {{-1, {}}, {0, {}}};
  asset->evaluation_order = {0, 1};
  asset->joint_nodes = {0, 1};
  asset->inverse_bind_matrices = {identity, identity};
  asset->vertices = {
      {{0, 0, 0}, {0, 0, 1}, {0, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}},
      {{1, 0, 0}, {0, 1, 0}, {1, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}},
      {{0, 1, 0}, {1, 0, 0}, {0, 1}, {1, 0, 0, 0}, {1, 0, 0, 0}}};
  asset->indices = {2, 0, 1, 1, 0, 2};
  asset->primitives = {{0, 3, {.7F, .1F, .2F, 1}}, {3, 3, {.1F, .8F, .4F, 1}}};
  return asset;
}

struct ExpectedTriangle {
  std::array<CharacterDeformedVertex, 3> vertices;
  std::array<float, 4> base_color;
};

// Interpret exactly the submitted index ranges, including the signed instance
// offset. Expected geometry below is independent of the asset's index stream
// and the skinning/presentation implementation.
void expectTriangles(const CharacterPresentation& scene,
                     std::span<const PositionColorVertex> vertices,
                     std::span<const ExpectedTriangle> expected) {
  std::size_t triangle = 0;
  for (const auto& draw : scene.draws()) {
    ASSERT_LE(draw.first_index, scene.indices().size());
    ASSERT_LE(draw.index_count, scene.indices().size() - draw.first_index);
    ASSERT_EQ(draw.index_count % 3, 0U);
    ASSERT_LT(draw.material, scene.materials().size());
    for (std::size_t i = 0; i < draw.index_count; i += 3) {
      SCOPED_TRACE(triangle);
      ASSERT_LT(triangle, expected.size());
      EXPECT_EQ(scene.materials()[draw.material], expected[triangle].base_color);
      for (std::size_t corner = 0; corner < 3; ++corner) {
        SCOPED_TRACE(corner);
        const auto vertex_index =
            std::int64_t(draw.vertex_offset) +
            scene.indices()[draw.first_index + i + corner];
        ASSERT_GE(vertex_index, 0);
        ASSERT_LT(static_cast<std::uint64_t>(vertex_index), vertices.size());
        const auto& actual = vertices[static_cast<std::size_t>(vertex_index)];
        const auto& wanted = expected[triangle].vertices[corner];
        for (std::size_t axis = 0; axis < 3; ++axis) {
          EXPECT_NEAR(actual.position[axis], wanted.position[axis], 0.00001);
          EXPECT_NEAR(actual.normal[axis], wanted.normal[axis], 0.00001);
        }
        for (std::size_t axis = 0; axis < 2; ++axis)
          EXPECT_FLOAT_EQ(actual.texture_coordinates[axis], wanted.uv[axis]);
        for (const auto channel : actual.color) EXPECT_EQ(channel, 255);
        EXPECT_EQ(actual.texture_layer, 0U);
      }
      ++triangle;
    }
  }
  EXPECT_EQ(triangle, expected.size());
}

void appendTrianglePair(std::vector<ExpectedTriangle>& expected, float x,
                        float tip_height) {
  const CharacterDeformedVertex root{{x, 2, 3}, {1, 0, 0}, {0, 0}};
  const CharacterDeformedVertex right{{x, 2, 2}, {0, 1, 0}, {1, 0}};
  const CharacterDeformedVertex tip{{x, tip_height, 3}, {0, 0, -1}, {0, 1}};
  expected.push_back({{tip, root, right}, {.7F, .1F, .2F, 1}});
  expected.push_back({{right, root, tip}, {.1F, .8F, .4F, 1}});
}
}  // namespace

TEST(CharacterPresentation, EmptySelectionNeedsNoAssetOrGeometry) {
  const CharacterPresentation scene({});
  EXPECT_EQ(scene.vertexCount(), 0U);
  EXPECT_EQ(scene.scratchCount(), 0U);
  EXPECT_TRUE(scene.materials().empty());
  EXPECT_TRUE(scene.draws().empty());
  EXPECT_TRUE(scene.indices().empty());
  EXPECT_NO_THROW(scene.deform({}, {}, {}));
  const std::array<CharacterPoseFrame, 1> unknown{{{13, 42, {}}}};
  EXPECT_THROW(scene.validate(unknown), std::invalid_argument);
}

TEST(CharacterPresentation,
     FourIndependentPosesShareIndicesAndPreserveTriangles) {
  auto asset = triangleAsset();
  const std::array<CharacterRenderInstance, 4> selected{
      {{1, asset}, {2, asset}, {3, asset}, {4, asset}}};
  const CharacterPresentation scene(selected);
  EXPECT_EQ(scene.vertexCount(), 12U);
  EXPECT_EQ(scene.scratchCount(), 3U);
  ASSERT_EQ(scene.indices().size(), 6U);
  EXPECT_TRUE(std::equal(scene.indices().begin(), scene.indices().end(),
                         asset->indices.begin()));
  ASSERT_EQ(scene.materials().size(), 2U);
  EXPECT_EQ(scene.materials()[0], asset->primitives[0].base_color);
  EXPECT_EQ(scene.materials()[1], asset->primitives[1].base_color);
  ASSERT_EQ(scene.draws().size(), 8U);
  std::array<CharacterPose, 4> palettes;
  std::array<CharacterPoseFrame, 4> poses;
  for (std::size_t i = 0; i < poses.size(); ++i) {
    palettes[i] = {identity, identity};
    palettes[i][1][13] = float(i);
    poses[i] = {static_cast<std::uint32_t>(i + 1),
                asset->skeleton_identity,
                palettes[i],
                {float(i) * 10, 2, 3},
                90};
  }
  // Request order is independent of the selected resource order.
  std::reverse(poses.begin(), poses.end());
  std::vector<CharacterDeformedVertex> scratch(scene.scratchCount());
  std::vector<PositionColorVertex> vertices(scene.vertexCount());
  scene.deform(poses, scratch, vertices);
  std::vector<ExpectedTriangle> expected;
  for (std::size_t i = 0; i < poses.size(); ++i) {
    appendTrianglePair(expected, float(i) * 10, 3 + float(i));
    // Both primitive draws reuse this instance's one source-vertex block.
    EXPECT_EQ(scene.draws()[i * 2].first_index, 0U);
    EXPECT_EQ(scene.draws()[i * 2 + 1].first_index, 3U);
    EXPECT_EQ(scene.draws()[i * 2].vertex_offset, i * 3);
    EXPECT_EQ(scene.draws()[i * 2 + 1].vertex_offset, i * 3);
  }
  expectTriangles(scene, vertices, expected);
  // The resource never retains a borrowed palette. The next call consumes it.
  const std::vector<std::uint32_t> retained_indices(scene.indices().begin(),
                                                     scene.indices().end());
  palettes[0][1][13] = 9;
  scene.deform(poses, scratch, vertices);
  expected.clear();
  appendTrianglePair(expected, 0, 12);
  for (std::size_t i = 1; i < poses.size(); ++i)
    appendTrianglePair(expected, float(i) * 10, 3 + float(i));
  expectTriangles(scene, vertices, expected);
  EXPECT_TRUE(std::equal(scene.indices().begin(), scene.indices().end(),
                         retained_indices.begin(), retained_indices.end()));
}

TEST(CharacterPresentation,
     DistinctAssetsWithSameSkeletonRetainTheirOwnIndicesAndMaterials) {
  const auto triangle = triangleAsset();
  auto square = triangleAsset();
  square->vertices = {
      {{0, 0, 0}, {0, 0, 1}, {0, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}},
      {{2, 0, 0}, {0, 0, 1}, {1, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}},
      {{0, 2, 0}, {0, 0, 1}, {0, 1}, {1, 0, 0, 0}, {1, 0, 0, 0}},
      {{2, 2, 0}, {0, 0, 1}, {1, 1}, {1, 0, 0, 0}, {1, 0, 0, 0}}};
  square->indices = {3, 2, 0, 0, 1, 3, 2, 3, 1};
  square->primitives = {
      {0, 6, {.9F, .6F, .1F, 1}}, {6, 3, {.2F, .3F, .9F, 1}}};
  ASSERT_EQ(triangle->skeleton_identity, square->skeleton_identity);
  const std::array<CharacterRenderInstance, 4> selected{
      {{1, triangle}, {2, square}, {3, triangle}, {4, square}}};
  const CharacterPresentation scene(selected);
  EXPECT_EQ(scene.vertexCount(), 14U);
  EXPECT_EQ(scene.scratchCount(), 4U);
  EXPECT_EQ(scene.indices().size(), 15U);
  EXPECT_EQ(scene.materials().size(), 4U);
  const CharacterPose palette{identity, identity};
  const std::array<CharacterPoseFrame, 4> poses{
      {{4, 42, palette, {30, 2, 3}, 90},
       {2, 42, palette, {10, 2, 3}, 90},
       {1, 42, palette, {0, 2, 3}, 90},
       {3, 42, palette, {20, 2, 3}, 90}}};
  std::vector<CharacterDeformedVertex> scratch(scene.scratchCount());
  std::vector<PositionColorVertex> vertices(scene.vertexCount());
  scene.deform(poses, scratch, vertices);
  std::vector<ExpectedTriangle> expected;
  for (std::size_t i = 0; i < 4; ++i) {
    const float x = float(i) * 10;
    if (i % 2 == 0) {
      appendTrianglePair(expected, x, 3);
    } else {
      const CharacterDeformedVertex a{{x, 2, 3}, {1, 0, 0}, {0, 0}};
      const CharacterDeformedVertex b{{x, 2, 1}, {1, 0, 0}, {1, 0}};
      const CharacterDeformedVertex c{{x, 4, 3}, {1, 0, 0}, {0, 1}};
      const CharacterDeformedVertex d{{x, 4, 1}, {1, 0, 0}, {1, 1}};
      expected.push_back({{d, c, a}, {.9F, .6F, .1F, 1}});
      expected.push_back({{a, b, d}, {.9F, .6F, .1F, 1}});
      expected.push_back({{c, d, b}, {.2F, .3F, .9F, 1}});
    }
  }
  expectTriangles(scene, vertices, expected);
}

TEST(CharacterPresentation, RejectsIncompleteDuplicateUnknownAndStalePalettes) {
  auto asset = triangleAsset();
  const std::array<CharacterRenderInstance, 2> selected{
      {{7, asset}, {9, asset}}};
  const CharacterPresentation scene(selected);
  CharacterPose palette{identity, identity};
  std::array<CharacterPoseFrame, 2> poses{{{7, 42, palette}, {9, 42, palette}}};
  EXPECT_NO_THROW(scene.validate(poses));
  EXPECT_THROW(scene.validate(std::span(poses).first(1)),
               std::invalid_argument);
  poses[1].instance = 7;
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
  poses[1].instance = 11;
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
  poses[1].instance = 9;
  poses[1].skeleton_identity = 41;
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
  poses[1].skeleton_identity = 42;
  poses[1].joint_globals = std::span(palette).first(1);
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
  poses[1].joint_globals = palette;
  poses[1].position[0] = std::numeric_limits<float>::infinity();
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
  poses[1].position[0] = 0;
  poses[1].yaw_degrees = std::numeric_limits<float>::quiet_NaN();
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
  poses[1].yaw_degrees = 0;
  palette[1][0] = std::numeric_limits<float>::quiet_NaN();
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
  palette[1] = identity;
  palette[1][0] = 2;
  EXPECT_THROW(scene.validate(poses), std::invalid_argument);
}

TEST(CharacterPresentation, RejectsSelectionAndDerivedRangeOverflow) {
  auto asset = triangleAsset();
  std::vector<CharacterRenderInstance> selected(5, {1, asset});
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  selected.resize(2);
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  selected.resize(1);
  selected[0].asset.reset();
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  selected[0].asset = asset;
  asset->primitives[1].index_count = std::numeric_limits<std::uint32_t>::max();
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  asset->primitives[1].index_count = 3;
  asset->indices[0] = 3;
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  asset->indices[0] = 2;
  asset->vertices[0].joints[0] = 65;
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  asset->vertices[0].joints[0] = 0;
  asset->vertices.resize(10001);
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
}

TEST(CharacterPresentation, RejectsEmptyIncompleteAndOverlappingIndexRanges) {
  const auto asset = triangleAsset();
  const auto valid_primitives = asset->primitives;
  const std::array<CharacterRenderInstance, 1> selected{{{1, asset}}};
  for (const auto count : {0U, 1U, 4U,
                            std::numeric_limits<std::uint32_t>::max()}) {
    SCOPED_TRACE(count);
    asset->primitives[0].index_count = count;
    EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  }
  asset->primitives = valid_primitives;
  for (const auto first : {0U, 2U, 4U,
                            std::numeric_limits<std::uint32_t>::max()}) {
    SCOPED_TRACE(first);
    asset->primitives[1].first_index = first;
    EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  }
  asset->primitives = valid_primitives;
  asset->primitives.pop_back();
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  asset->primitives.clear();
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  asset->primitives = valid_primitives;
  asset->indices.clear();
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
  asset->indices = {2, 0, 1, 1, 0, 2};
  asset->vertices.clear();
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
}

TEST(CharacterPresentation, FullSharedProfileCountsEveryInstanceDraw) {
  const auto asset = triangleAsset();
  const auto source_vertex = asset->vertices.front();
  asset->vertices.resize(10000, source_vertex);
  // 49,998 is the largest complete triangle stream under the 50,000 cap.
  asset->indices.resize(49998);
  for (std::size_t i = 0; i < asset->indices.size(); ++i)
    asset->indices[i] = static_cast<std::uint32_t>(i % asset->vertices.size());
  asset->primitives[0].index_count = 24999;
  asset->primitives[1].first_index = 24999;
  asset->primitives[1].index_count = 24999;
  const std::array<CharacterRenderInstance, 4> selected{
      {{1, asset}, {2, asset}, {3, asset}, {4, asset}}};
  const CharacterPresentation scene(selected);
  EXPECT_EQ(scene.vertexCount(), 40000U);
  EXPECT_EQ(scene.scratchCount(), 10000U);
  EXPECT_EQ(scene.indices().size(), 49998U);
  std::size_t drawn_indices = 0;
  for (const auto& draw : scene.draws()) {
    drawn_indices += draw.index_count;
    ASSERT_GE(draw.vertex_offset, 0);
    ASSERT_LE(draw.first_index, scene.indices().size());
    ASSERT_LE(draw.index_count, scene.indices().size() - draw.first_index);
    for (const auto index : scene.indices().subspan(draw.first_index,
                                                   draw.index_count)) {
      EXPECT_LT(index, 10000U);
      EXPECT_LT(static_cast<std::size_t>(draw.vertex_offset) + index,
                scene.vertexCount());
    }
  }
  EXPECT_EQ(drawn_indices, 199992U);
  asset->indices.resize(50001);
  asset->primitives[1].index_count += 3;
  EXPECT_THROW(CharacterPresentation{selected}, std::invalid_argument);
}

TEST(CharacterPresentation, ValidatesWholeRequestBeforeWritingOutput) {
  auto asset = triangleAsset();
  const std::array<CharacterRenderInstance, 2> selected{
      {{1, asset}, {2, asset}}};
  const CharacterPresentation scene(selected);
  const CharacterPose palette{identity, identity};
  std::array<CharacterPoseFrame, 2> poses{{{1, 42, palette}, {2, 99, palette}}};
  std::vector<CharacterDeformedVertex> scratch(scene.scratchCount());
  std::vector<PositionColorVertex> vertices(scene.vertexCount());
  vertices.front().position[0] = 123;
  EXPECT_THROW(scene.deform(poses, scratch, vertices), std::invalid_argument);
  EXPECT_EQ(vertices.front().position[0], 123);
  poses[1].skeleton_identity = 42;
  EXPECT_THROW(scene.deform(poses, std::span(scratch).first(2), vertices),
               std::invalid_argument);
  EXPECT_EQ(vertices.front().position[0], 123);
  EXPECT_THROW(scene.deform(poses, scratch, std::span(vertices).first(5)),
               std::invalid_argument);
  EXPECT_EQ(vertices.front().position[0], 123);
  vertices.resize(scene.vertexCount() + 1);
  EXPECT_THROW(scene.deform(poses, scratch, vertices), std::invalid_argument);
  EXPECT_EQ(vertices.front().position[0], 123);
}
