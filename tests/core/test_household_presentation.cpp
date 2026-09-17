#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include "core/render/scene_assets.hpp"
#include "core/world/household.hpp"

namespace {
LevelDocument scene() {
  LevelDocument d;
  d.solids = {{{0, -.25F, 0},
               {10, .25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-3, 0, 0}, 0}}};
  d.default_entry = "default";
  return d;
}
}  // namespace

TEST(HouseholdPresentation, FullRotationTransformsCornersNormalsAndWinding) {
  // 120 degrees about (1,1,1) permutes X/Y/Z to Z/X/Y.
  const OpaqueBoxFrame box{{2, 4, -6}, {1, 2, 3}, {.5F, .5F, .5F, .5F}};
  const auto vertices = buildOpaqueBoxVertices({&box, 1});
  ASSERT_EQ(vertices.size(), 36U);
  const std::array<float, 3> extent{3, 1, 2};
  for (const auto& v : vertices) {
    float length = 0, outward = 0;
    for (std::size_t a = 0; a < 3; ++a) {
      EXPECT_NEAR(std::abs(v.position[a] - box.center[a]), extent[a], .00001F);
      length += v.normal[a] * v.normal[a];
      outward += v.normal[a] * (v.position[a] - box.center[a]);
    }
    EXPECT_NEAR(length, 1, .00001F);
    EXPECT_GT(outward, 0);
  }
  for (std::size_t i = 0; i < vertices.size(); i += 3) {
    std::array<float, 3> a{}, b{};
    for (std::size_t j = 0; j < 3; ++j) {
      a[j] = vertices[i + 1].position[j] - vertices[i].position[j];
      b[j] = vertices[i + 2].position[j] - vertices[i].position[j];
    }
    const std::array<float, 3> cross{a[1] * b[2] - a[2] * b[1],
                                     a[2] * b[0] - a[0] * b[2],
                                     a[0] * b[1] - a[1] * b[0]};
    float dot = 0;
    for (std::size_t j = 0; j < 3; ++j) dot += cross[j] * vertices[i].normal[j];
    EXPECT_GT(dot, 0);
  }
  auto same = box;
  for (auto& value : same.orientation) value = -value;
  const auto opposite = buildOpaqueBoxVertices({&same, 1});
  for (std::size_t i = 0; i < vertices.size(); ++i)
    for (std::size_t axis = 0; axis < 3; ++axis) {
      EXPECT_FLOAT_EQ(vertices[i].position[axis], opposite[i].position[axis]);
      EXPECT_FLOAT_EQ(vertices[i].normal[axis], opposite[i].normal[axis]);
    }
}

TEST(HouseholdPresentation, InvalidRotationsAndDerivedOverflowAreRejected) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  for (const auto rotation :
       {std::array<float, 4>{0, 0, 0, 0}, std::array<float, 4>{1, 1, 1, 1},
        std::array<float, 4>{0, nan, 0, 1},
        std::array<float, 4>{0, 0, inf, 1}}) {
    const OpaqueBoxFrame box{{0, 0, 0}, {1, 1, 1}, rotation};
    EXPECT_THROW((void)buildOpaqueBoxVertices({&box, 1}), std::runtime_error);
  }
  const float maximum = std::numeric_limits<float>::max();
  const OpaqueBoxFrame overflow{{maximum, 0, 0}, {maximum, 1, 1}};
  EXPECT_THROW((void)buildOpaqueBoxVertices({&overflow, 1}),
               std::runtime_error);
}

TEST(HouseholdPresentation, CombinedCapacityKeepsEveryDoorAndHouseholdBox) {
  auto d = scene();
  for (int i = 0; i < 16; ++i)
    d.household.boxes.push_back(
        {"box-" + std::to_string(i), {float(i), 1, 0}, float(i)});
  for (int i = 0; i < 32; ++i)
    d.household.documents.push_back({"note-" + std::to_string(i),
                                     {float(i), 1, 1},
                                     float(i),
                                     "Note",
                                     {"Page"}});
  for (int i = 0; i < 8; ++i) {
    const auto id = "prop-" + std::to_string(i);
    d.props.push_back({id, "apartment-radio", {float(i), 1, 2}, 20, 1, {}});
    d.household.radios.push_back(
        {"radio-" + std::to_string(i), id, "source", i % 2 == 0});
  }
  auto boxes = householdInitialPresentation(d);
  EXPECT_EQ(boxes.size(), 56U);
  for (int i = 0; i < 32; ++i) {
    DoorDefinition door;
    door.id = "door-" + std::to_string(i);
    door.hinge_position = {float(i), 0, 5};
    const auto leaf = doorPresentationBoxes(door, float(i), i % 2 == 0);
    boxes.insert(boxes.end(), leaf.begin(), leaf.end());
    d.doors.push_back(door);
  }
  EXPECT_EQ(boxes.size(), 248U);
  EXPECT_EQ(boxes.size(), frame_maximum_opaque_box_count);
  EXPECT_EQ(buildOpaqueBoxVertices(boxes).size(), 248U * 36U);
  const auto assets = prepareSceneAssets("resources", d);
  ASSERT_TRUE(assets.obstacle_material);
  EXPECT_EQ(assets.materials[*assets.obstacle_material].id,
            "prototype-obstacle");
}

TEST(HouseholdPresentation, DoorFreeScenesPrepareTheirOwnGeneratedMaterial) {
  for (int kind = 0; kind < 3; ++kind) {
    auto d = scene();
    if (kind == 0) d.household.boxes = {{"box", {0, .15F, 0}, 30}};
    if (kind == 1)
      d.household.documents = {{"note", {0, .003F, 0}, 15, "Note", {"Page"}}};
    if (kind == 2) {
      d.props = {{"receiver", "apartment-radio", {1, 0, 0}, 0, 1, {}}};
      d.audio.cues = {
          {"loop", "radio", "radio", AudioCueKind::Ambience, true, true}};
      d.audio.sources = {{"source", "loop"}};
      d.household.radios = {{"control", "receiver", "source", true}};
    }
    ASSERT_TRUE(validateLevelDocument(d).empty());
    const auto initial = householdInitialPresentation(d);
    EXPECT_EQ(initial.size(), 1U);
    const auto assets = prepareSceneAssets("resources", makePrototypeLevel(d));
    ASSERT_TRUE(assets.obstacle_material);
    EXPECT_EQ(assets.materials[*assets.obstacle_material].id,
              "prototype-obstacle");
  }
  const auto empty = scene();
  EXPECT_TRUE(householdInitialPresentation(empty).empty());
  EXPECT_FALSE(prepareSceneAssets("resources", empty).obstacle_material);
}

TEST(HouseholdPresentation, InitialPreviewKeepsDefinitionsAndSkipsBrokenLinks) {
  auto d = scene();
  d.household.boxes = {{"box", {0, .15F, 0}, 45}};
  d.household.documents = {{"note", {1, .003F, 0}, 30, "Note", {"Page"}}};
  d.household.radios = {{"broken", "missing", "source", true}};
  const auto before = d;
  const auto initial = householdInitialPresentation(d);
  ASSERT_EQ(initial.size(), 2U);
  EXPECT_EQ(initial[0].center, (std::array<float, 3>{0, .15F, 0}));
  EXPECT_EQ(initial[0].orientation, yawQuaternion(45));
  EXPECT_EQ(d, before);
  const auto moving = householdBoxPresentation({0, 2, 0}, {.5F, .5F, .5F, .5F});
  EXPECT_EQ(moving.half_extent, initial[0].half_extent);
  EXPECT_EQ(moving.color, initial[0].color);
  EXPECT_NE(moving.center, initial[0].center);
  EXPECT_EQ(householdInitialPresentation(d)[0].center, initial[0].center);
}
