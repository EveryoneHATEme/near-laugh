#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "core/gameplay/authored_interaction.hpp"
#include "core/simulation/fixed_step.hpp"
#include "core/world/scene_assets.hpp"
#include "prototype_level_fixture.hpp"

namespace {
LevelDocument doorLevel() {
  auto doc = prototypeLevelDocument();
  doc.terrain.reset();
  doc.props.clear();
  doc.solids = {{{0, -0.25F, 0},
                 {10, 0.25F, 10},
                 {160, 160, 160, 255},
                 PrototypeSolidKind::Floor,
                 "prototype-floor"}};
  doc.entries = {{"start", {{0, 0, 3}, 180}}};
  doc.default_entry = "start";
  doc.light_switch = PrototypeLightSwitch{{0, 1.2F, -1}, 0, 0, true};
  DoorDefinition door;
  door.id = "room";
  door.hinge_position = {-0.45F, 0.02F, 0};
  doc.doors = {door};
  return doc;
}
void steps(PhysicsWorld& physics, DoorController& doors, int count = 1,
           PhysicsVector velocity = {}) {
  for (int i = 0; i < count; ++i) {
    (void)physics.stepCharacter({velocity}, 1.0F / 60);
    doors.fixedStep(1.0F / 60, physics);
  }
}
PlayerViewPose doorView() { return {{0, 1.2F, 0.8F}, {0, 0, -1}}; }
WorldPosition insideEye() { return {0, 1.2F, 0.8F}; }
bool doorDiagnostic(const LevelDocument& doc) {
  const auto failures = validateLevelDocument(doc);
  return std::any_of(failures.begin(), failures.end(), [](const auto& f) {
    return f.document_path.starts_with("doors");
  });
}
}  // namespace

TEST(DoorGeometry, SharedPoseCornersAndPresentationAgreeForBothDirections) {
  auto door = doorLevel().doors.front();
  for (float angle : {-90.0F, 0.0F, 90.0F}) {
    const auto pose = doorLeafPose(door, angle);
    const auto boxes = doorPresentationBoxes(door, angle, false);
    EXPECT_FLOAT_EQ(boxes[0].center[0], pose.center.x);
    EXPECT_FLOAT_EQ(boxes[0].center[2], pose.center.z);
    for (auto corner : doorCorners(door, angle)) {
      const auto local = doorLocalPoint(door, angle, corner);
      EXPECT_TRUE(std::abs(local.x) < 0.00001F ||
                  std::abs(local.x - door.width) < 0.00001F);
      EXPECT_NEAR(std::abs(local.z), door.thickness / 2, 0.00001F);
    }
  }
  EXPECT_GT(doorLeafPose(door, -90).center.z, 0);
  EXPECT_LT(doorLeafPose(door, 90).center.z, 0);
}

TEST(DoorGeometry, RayReachInsideAndMiss) {
  const auto door = doorLevel().doors.front();
  const auto hit = doorRayDistance(door, 0, {0, 1, 2.03F}, {0, 0, -2});
  ASSERT_TRUE(hit);
  EXPECT_NEAR(*hit, 2.0F, 0.00001F);
  EXPECT_FALSE(doorRayDistance(door, 0, {0, 1, 0}, {0, 0, 1}));
  EXPECT_FALSE(doorRayDistance(door, 0, {2, 1, 1}, {0, 0, -1}));
  EXPECT_FALSE(doorRayDistance(door, 0, {0, 1, 1}, {0, 0, 0}));
  EXPECT_FALSE(doorRayDistance(door, 90, {0, 1, 1}, {0, 0, -1}));
}

TEST(DoorWorld, ProfileIdentityAndInitialLocksValidate) {
  auto doc = doorLevel();
  ASSERT_TRUE(validateLevelDocument(doc).empty());
  doc.doors.front().initially_locked = true;
  EXPECT_TRUE(validateLevelDocument(doc).empty());
  doc.doors.front().initially_open = true;
  EXPECT_TRUE(doorDiagnostic(doc));
  doc.doors.front().initially_open = false;
  doc.doors.front().lock_side = DoorLockSide::None;
  EXPECT_TRUE(doorDiagnostic(doc));
  doc = doorLevel();
  doc.doors.front().width = 0.39F;
  EXPECT_TRUE(doorDiagnostic(doc));
  doc = doorLevel();
  doc.doors.push_back(doc.doors.front());
  doc.doors.back().hinge_position.x = 5;
  EXPECT_TRUE(doorDiagnostic(doc));
  doc = doorLevel();
  doc.doors.front().open_angle_degrees = 0;
  EXPECT_TRUE(doorDiagnostic(doc));
  doc = doorLevel();
  doc.doors.front().hinge_position.x = std::numeric_limits<float>::infinity();
  EXPECT_TRUE(doorDiagnostic(doc));
}

TEST(DoorWorld, MaximumIndependentDoorDefinitionsFitThePresentationBound) {
  auto doc = doorLevel();
  doc.doors.clear();
  doc.entries[0].pose.foot_position = {9, 0, 9};
  for (std::size_t i = 0; i < level_maximum_door_count; ++i) {
    DoorDefinition door;
    door.id = "door-" + std::to_string(i);
    door.hinge_position = {-9 + float(i % 6) * 3, .02F, -9 + float(i / 6) * 3};
    doc.doors.push_back(door);
  }
  ASSERT_TRUE(validateLevelDocument(doc).empty());
  const auto level = makePrototypeLevel(doc);
  DoorController doors(level.doors());
  EXPECT_EQ(doors.presentation().size(), frame_maximum_opaque_box_count);
  auto extra = doc.doors[0];
  extra.id = "overflow";
  extra.hinge_position = {9, .02F, -9};
  doc.doors.push_back(extra);
  const auto errors = validateLevelDocument(doc);
  EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const auto& error) {
    return error.document_path == "doors";
  }));
  EXPECT_THROW((void)DoorController(doc.doors), std::invalid_argument);
}

TEST(DoorWorld, InitialDoorChecksEveryEntryAndAllStaticProxies) {
  auto doc = doorLevel();
  doc.entries.push_back({"other", {{0, 0, 0}, 0}});
  EXPECT_TRUE(doorDiagnostic(doc));
  doc = doorLevel();
  doc.props.push_back({"obstacle",
                       "apartment-chair",
                       {0, 0, 0},
                       0,
                       1,
                       {{{0, 1, 0}, {0.1F, 0.3F, 0.1F}}}});
  EXPECT_TRUE(doorDiagnostic(doc));
  doc.props.front().collision_boxes.clear();
  EXPECT_FALSE(doorDiagnostic(doc));
  doc.doors.front().hinge_position.y = -0.1F;
  EXPECT_TRUE(doorDiagnostic(doc));
}

TEST(DoorWorld, TerrainIntersectionAndUpperFloorAreHeightSpecific) {
  auto doc = doorLevel();
  doc.terrain = PrototypeTerrain{{-24, -1, -24}, 0.5F, {}};
  EXPECT_FALSE(doorDiagnostic(doc));
  doc.terrain->origin.y = 0.5F;
  EXPECT_TRUE(doorDiagnostic(doc));
  doc = doorLevel();
  doc.solids.push_back({{0, 2.75F, 0},
                        {5, 0.25F, 5},
                        {150, 150, 150, 255},
                        PrototypeSolidKind::Floor,
                        "prototype-floor"});
  doc.doors.front().hinge_position.y = 3.02F;
  doc.entries.push_back({"upper", {{0, 3, 3}, 180}});
  EXPECT_FALSE(doorDiagnostic(doc));
}

TEST(DoorWorld, InitialStandingEntryIncludesRaisedCapsuleAndContactSkin) {
  auto doc = doorLevel();
  doc.entries[0].pose.foot_position = {0, 0, 0};
  doc.doors[0].hinge_position.y = 1.81F;
  EXPECT_TRUE(doorDiagnostic(doc));
  doc.doors[0].hinge_position.y = 1.85F;
  EXPECT_FALSE(doorDiagnostic(doc));
  const auto level = makePrototypeLevel(doc);
  EXPECT_TRUE(prototypeSpawnIsClear(level, .35F, 1.8F));
  EXPECT_FALSE(prototypeSpawnIsClear(level, .35F, 1.82F));
  doc.doors[0].hinge_position = {.36F, .02F, 0};
  EXPECT_TRUE(doorDiagnostic(doc));
  doc.doors[0].hinge_position.x = .38F;
  EXPECT_FALSE(doorDiagnostic(doc));
}

TEST(DoorWorld, RoundTripPreservesIdentityStateAndRuntimeDoesNotWrite) {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("near_laugh_doors_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".json");
  auto doc = doorLevel();
  ASSERT_TRUE(saveLevelDocument(path, doc));
  const auto bytes = [&] {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
  };
  const auto original = bytes();
  auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded.document->doors, doc.doors);
  {
    const auto level = makePrototypeLevel(*loaded.document);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    (void)doors.act(0, DoorAction::Interact, insideEye());
    steps(physics, doors, 65);
    EXPECT_FLOAT_EQ(doors.state(0).angle, 90);
  }
  EXPECT_EQ(bytes(), original);
  ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
  EXPECT_EQ(bytes(), original);
  std::filesystem::remove(path);
}

TEST(DoorPhysics, VisibilityTargetsOwnLeafAndBlocksSwitch) {
  const auto level = makePrototypeLevel(doorLevel());
  PhysicsWorld physics(level);
  EXPECT_FALSE(physics.worldSegmentBlocked({0, 1, .8F}, {0, 1, .5F}));
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1, .8F}, {0, 1, .03F}));
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1, 0.8F}, {0, 1, -0.98F}));
  EXPECT_FALSE(
      physics.worldSegmentBlocked({0, 1, 0.8F}, {0, 1, 0.03F}, "room"));
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1, 0}, {0, 1, 0.8F}, "room"));
  ASSERT_FALSE(physics.advanceDoor(0, 90).obstructed);
  EXPECT_FALSE(physics.worldSegmentBlocked({0, 1, 0.8F}, {0, 1, -0.98F}));
}

TEST(DoorPhysics, ContinuousArcStopsAtThinIntermediateBlocker) {
  auto doc = doorLevel();
  auto& door = doc.doors.front();
  door.width = 2;
  door.hinge_position.x = 0;
  doc.solids.push_back({{0.9F, 1, -0.9F},
                        {0.003F, 0.4F, 0.003F},
                        {150, 150, 150, 255},
                        PrototypeSolidKind::Obstacle,
                        "prototype-obstacle"});
  ASSERT_FALSE(yawedBoxesOverlap(
      doorLeafPose(door, 0),
      {doc.solids.back().center, doc.solids.back().half_extent, 0}));
  ASSERT_FALSE(yawedBoxesOverlap(
      doorLeafPose(door, 90),
      {doc.solids.back().center, doc.solids.back().half_extent, 0}));
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  const auto result = physics.advanceDoor(0, 90);
  EXPECT_TRUE(result.obstructed);
  EXPECT_GT(result.angle, 40);
  EXPECT_LT(result.angle, 45);
  EXPECT_FALSE(yawedBoxesOverlap(
      doorLeafPose(door, result.angle),
      {doc.solids.back().center, doc.solids.back().half_extent, 0}));
}

TEST(DoorPhysics, ContinuousArcIncludesTerrainAndEveryAuthoredPropBox) {
  for (const bool terrain : {false, true}) {
    auto doc = doorLevel();
    doc.doors[0].width = 2;
    doc.doors[0].hinge_position.x = 0;
    if (terrain) {
      doc.terrain = PrototypeTerrain{{-24, 0, -24}, .5F, {}};
      doc.terrain->heights[46 * prototype_terrain_sample_count + 50] = .25F;
    } else {
      doc.props.push_back(
          {"blocker",
           "apartment-chair",
           {.9F, 0, -.9F},
           35,
           1,
           {{{5, 1, 5}, {.3F, .3F, .3F}}, {{0, 1, 0}, {.003F, .4F, .003F}}}});
    }
    ASSERT_FALSE(doorDiagnostic(doc));
    const auto level = makePrototypeLevel(doc);
    PhysicsWorld physics(level);
    const auto result = physics.advanceDoor(0, 90);
    EXPECT_TRUE(result.obstructed);
    EXPECT_GT(result.angle, 0);
    EXPECT_LT(result.angle, 90);
  }
}

TEST(DoorGameplay, LocksKnockReversalAndEndpointState) {
  const auto level = makePrototypeLevel(doorLevel());
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  EXPECT_EQ(doors.act(0, DoorAction::Lock, {0, 1, -1}).kind,
            DoorResultKind::Refused);
  EXPECT_EQ(doors.act(0, DoorAction::Lock, insideEye()).kind,
            DoorResultKind::Locked);
  EXPECT_EQ(doors.act(0, DoorAction::Interact, insideEye()).kind,
            DoorResultKind::Refused);
  EXPECT_EQ(doors.act(0, DoorAction::Knock, insideEye()).kind,
            DoorResultKind::Knocked);
  EXPECT_TRUE(doors.state(0).locked);
  EXPECT_FLOAT_EQ(doors.state(0).angle, 0);
  EXPECT_EQ(doors.act(0, DoorAction::Lock, insideEye()).kind,
            DoorResultKind::Unlocked);
  (void)doors.act(0, DoorAction::Interact, insideEye());
  steps(physics, doors, 15);
  EXPECT_FLOAT_EQ(doors.state(0).angle, 22.5F);
  EXPECT_EQ(doors.act(0, DoorAction::Lock, insideEye()).kind,
            DoorResultKind::Refused);
  EXPECT_EQ(doors.act(0, DoorAction::Interact, insideEye()).kind,
            DoorResultKind::Closing);
  steps(physics, doors, 20);
  EXPECT_FLOAT_EQ(doors.state(0).angle, 0);
  EXPECT_FALSE(doors.state(0).moving);
}

TEST(DoorGameplay, PlayerObstructionStopsWithoutResumeOrPush) {
  const auto level = makePrototypeLevel(doorLevel());
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  (void)doors.act(0, DoorAction::Interact, insideEye());
  steps(physics, doors, 65);
  steps(physics, doors, 90, {0, 0, -2});
  const auto position = physics.characterState().foot_position;
  (void)doors.act(0, DoorAction::Interact, insideEye());
  for (int i = 0; i < 65; ++i) doors.fixedStep(1.0F / 60, physics);
  EXPECT_GT(doors.state(0).angle, 0);
  EXPECT_FALSE(doors.state(0).moving);
  EXPECT_FLOAT_EQ(physics.characterState().foot_position.z, position.z);
  const float stopped = doors.state(0).angle;
  steps(physics, doors, 100, {0, 0, 2});
  EXPECT_FLOAT_EQ(doors.state(0).angle, stopped);
  EXPECT_EQ(doors.act(0, DoorAction::Interact, insideEye()).kind,
            DoorResultKind::Opening);
  steps(physics, doors, 65);
  EXPECT_FLOAT_EQ(doors.state(0).angle, 90);
}

TEST(DoorPhysics, CrouchedCapsuleClearsLeafAndCannotStandIntoIt) {
  auto doc = doorLevel();
  doc.entries[0].pose.foot_position = {0, 0, 0};
  doc.doors[0].hinge_position.y = 1.45F;
  doc.doors[0].initially_open = true;
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  (void)physics.stepCharacter({{}, {0, -18, 0}, true}, 1.0F / 60);
  ASSERT_EQ(physics.characterState().stance, PhysicsPlayerStance::Crouched);
  // The interpolated stance still includes the previous standing capsule.
  const auto changing = physics.advanceDoor(0, 0);
  EXPECT_TRUE(changing.obstructed);
  EXPECT_GT(changing.angle, 0);
  (void)physics.stepCharacter({{}, {0, -18, 0}, true}, 1.0F / 60);
  const auto crouched = physics.advanceDoor(0, 0);
  EXPECT_FALSE(crouched.obstructed);
  EXPECT_FLOAT_EQ(crouched.angle, 0);
  const auto before = physics.characterState().foot_position;
  (void)physics.stepCharacter({{}, {0, -18, 0}, false}, 1.0F / 60);
  EXPECT_EQ(physics.characterState().stance, PhysicsPlayerStance::Crouched);
  EXPECT_NEAR(physics.characterState().foot_position.x, before.x, .001F);
  EXPECT_NEAR(physics.characterState().foot_position.z, before.z, .001F);
}

TEST(DoorPhysics, StandingContactSkinRemainsOutsideMovingLeaf) {
  auto doc = doorLevel();
  doc.entries[0].pose.foot_position = {0, 0, 0};
  doc.doors[0].hinge_position.y = player_standing_height + .01F;
  doc.doors[0].initially_open = true;
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  // CharacterVirtual's raised capsule and contact skin exceed nominal height.
  const auto result = physics.advanceDoor(0, 0);
  EXPECT_TRUE(result.obstructed);
  EXPECT_GT(result.angle, 0);
  EXPECT_FLOAT_EQ(physics.characterState().foot_position.y, 0);
}

TEST(DoorPhysics, LowLeafAlsoStopsBeforeCrouchedPlayer) {
  auto doc = doorLevel();
  doc.entries[0].pose.foot_position = {0, 0, 0};
  doc.doors[0].initially_open = true;
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  for (int i = 0; i < 2; ++i)
    (void)physics.stepCharacter({{}, {0, -18, 0}, true}, 1.0F / 60);
  ASSERT_EQ(physics.characterState().stance, PhysicsPlayerStance::Crouched);
  const auto position = physics.characterState().foot_position;
  const auto result = physics.advanceDoor(0, 0);
  EXPECT_TRUE(result.obstructed);
  EXPECT_GT(result.angle, 0);
  EXPECT_FLOAT_EQ(physics.characterState().foot_position.x, position.x);
  EXPECT_FLOAT_EQ(physics.characterState().foot_position.z, position.z);
}

TEST(DoorPhysics, DiagonalVacatedSpaceIsProtectedUntilNextPlayerStep) {
  auto doc = doorLevel();
  doc.entries[0].pose.foot_position = {0, 0, 0};
  doc.doors[0].initially_open = true;
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  const auto previous = physics.characterState().foot_position;
  (void)physics.stepCharacter({{120, 0, 120}}, 1.0F / 60);
  const auto current = physics.characterState().foot_position;
  ASSERT_GT(current.x, 1.5F);
  ASSERT_GT(current.z, 1.5F);
  const auto envelope = [](PhysicsVector foot) {
    return DoorLeafPose{{foot.x, foot.y + player_standing_height / 2, foot.z},
                        {player_capsule_radius, player_standing_height / 2,
                         player_capsule_radius},
                        0};
  };
  EXPECT_FALSE(
      yawedBoxesOverlap(doorLeafPose(level.doors()[0], 0), envelope(current)));
  const auto stopped = physics.advanceDoor(0, 0);
  EXPECT_TRUE(stopped.obstructed);
  EXPECT_GT(stopped.angle, 0);
  EXPECT_FALSE(yawedBoxesOverlap(doorLeafPose(level.doors()[0], stopped.angle),
                                 envelope(previous)));
  EXPECT_FALSE(yawedBoxesOverlap(doorLeafPose(level.doors()[0], stopped.angle),
                                 envelope(current)));
  (void)physics.stepCharacter({{}}, 1.0F / 60);
  EXPECT_FALSE(physics.advanceDoor(0, 0).obstructed);
  EXPECT_FLOAT_EQ(physics.doorAngle(0), 0);
}

TEST(DoorGameplay, EndpointRetryAfterObstructionWithNoProgress) {
  auto doc = doorLevel();
  // An exact adjoining obstacle makes the conservative first interval unsafe.
  doc.solids.push_back({{0, 1, -0.04F},
                        {0.3F, 0.3F, 0.01F},
                        {150, 150, 150, 255},
                        PrototypeSolidKind::Obstacle,
                        "prototype-obstacle"});
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  (void)doors.act(0, DoorAction::Interact, insideEye());
  steps(physics, doors);
  EXPECT_FLOAT_EQ(doors.state(0).angle, 0);
  EXPECT_FALSE(doors.state(0).moving);
  EXPECT_EQ(doors.act(0, DoorAction::Interact, insideEye()).kind,
            DoorResultKind::Opening);
}

TEST(DoorInteraction, HeldMissTransitionsAndActionPriorityDoNotReplay) {
  const auto level = makePrototypeLevel(doorLevel());
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  LightSwitchController light(level.lightSwitch());
  AuthoredInteraction interaction;
  PlayerActionSnapshot input;
  const auto update = [&](bool active, PlayerViewPose view = doorView()) {
    return interaction.update(input, active, view, level, physics, doors,
                              light);
  };
  input.interact = true;
  EXPECT_FALSE(update(true));  // No observed initial release.
  input.interact = false;
  (void)update(true);
  input.interact = true;
  EXPECT_FALSE(update(true, {{3, 1, 1}, {0, 0, -1}}));
  EXPECT_FALSE(update(true));
  EXPECT_FALSE(doors.state(0).moving);
  input.interact = false;
  (void)update(false);
  input.interact = true;
  EXPECT_FALSE(update(false));
  EXPECT_FALSE(update(true));
  input = {};
  (void)update(true);
  input.lock = true;
  input.interact = true;
  const auto result = update(true);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->kind, DoorResultKind::Locked);
  EXPECT_FALSE(doors.state(0).moving);
  EXPECT_TRUE(light.pointLightEnabled()[0]);
  EXPECT_FALSE(update(true));
}

TEST(DoorInteraction,
     OpenLeafAllowsSwitchAndNearestUnsupportedActionDoesNotFallThrough) {
  const auto level = makePrototypeLevel(doorLevel());
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  LightSwitchController light(level.lightSwitch());
  AuthoredInteraction interaction;
  PlayerActionSnapshot input;
  (void)interaction.update(input, true, doorView(), level, physics, doors,
                           light);
  input.interact = true;
  ASSERT_TRUE(interaction.update(input, true, doorView(), level, physics, doors,
                                 light));
  EXPECT_TRUE(light.pointLightEnabled()[0]);
  steps(physics, doors, 65);
  input = {};
  (void)interaction.update(input, true, doorView(), level, physics, doors,
                           light);
  input.interact = true;
  (void)interaction.update(input, true, doorView(), level, physics, doors,
                           light);
  EXPECT_FALSE(light.pointLightEnabled()[0]);
  input = {};
  (void)interaction.update(input, true, doorView(), level, physics, doors,
                           light);
  input.lock = true;
  (void)interaction.update(input, true, doorView(), level, physics, doors,
                           light);
  EXPECT_FALSE(light.pointLightEnabled()[0]);
  EXPECT_FALSE(doors.state(0).locked);
}

TEST(DoorInteraction, NearestLeafWinsIndependentlyOfStorageOrderAndReach) {
  for (const bool reverse : {false, true}) {
    auto doc = doorLevel();
    auto far = doc.doors[0];
    far.id = "a-far";
    far.hinge_position.z = -.5F;
    doc.doors.push_back(far);
    if (reverse) std::reverse(doc.doors.begin(), doc.doors.end());
    const auto level = makePrototypeLevel(doc);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    LightSwitchController light(level.lightSwitch());
    AuthoredInteraction interaction;
    const auto press = [&](float z) {
      const PlayerViewPose view{{0, 1.2F, z}, {0, 0, -1}};
      (void)interaction.update({}, true, view, level, physics, doors, light);
      PlayerActionSnapshot input;
      input.interact = true;
      return interaction.update(input, true, view, level, physics, doors,
                                light);
    };
    EXPECT_FALSE(press(2.031F));
    const auto result = press(2.03F);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->id, "room");
    EXPECT_EQ(result->kind, DoorResultKind::Opening);
    EXPECT_TRUE(light.pointLightEnabled()[0]);
    for (std::size_t i = 0; i < level.doors().size(); ++i)
      EXPECT_EQ(doors.state(i).moving, level.doors()[i].id == "room");
  }
}

TEST(DoorInteraction, TypeTieUsesAbsoluteMinimumTolerance) {
  for (const float offset : {0.0F, .00005F, .001F}) {
    auto doc = doorLevel();
    doc.light_switch->position.z = .01F + offset;
    const auto level = makePrototypeLevel(doc);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    LightSwitchController light(level.lightSwitch());
    AuthoredInteraction interaction;
    (void)interaction.update({}, true, doorView(), level, physics, doors,
                             light);
    PlayerActionSnapshot input;
    input.interact = true;
    const auto result = interaction.update(input, true, doorView(), level,
                                           physics, doors, light);
    if (offset <= .0001F) {
      ASSERT_TRUE(result);
      EXPECT_EQ(result->id, "room");
      EXPECT_TRUE(doors.state(0).moving);
      EXPECT_TRUE(light.pointLightEnabled()[0]);
    } else {
      EXPECT_FALSE(result);
      EXPECT_FALSE(doors.state(0).moving);
      EXPECT_FALSE(light.pointLightEnabled()[0]);
    }
  }
}

TEST(DoorGameplay, FrameBatchesBoundStepsAndConsumeMinimizedPresses) {
  auto doc = doorLevel();
  doc.entries[0].pose = {{0, 0, 1.8F}, -90};
  const auto original = doc;
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  PlayerController player(physics, -90);
  DoorController doors(level.doors());
  LightSwitchController light(level.lightSwitch());
  AuthoredInteraction interaction;
  FixedStepAccumulator clock;
  PlayerActionSnapshot input;
  const auto frame = [&](double elapsed) {
    player.sampleInput(input, true);
    const auto batch = clock.advance(elapsed);
    for (int i = 0; i < batch.complete_steps; ++i) {
      player.fixedStep(1.0F / 60);
      doors.fixedStep(1.0F / 60, physics);
    }
    return interaction.update(input, true,
                              player.viewPose(batch.interpolation_alpha), level,
                              physics, doors, light);
  };
  (void)frame(0);
  input.interact = true;
  ASSERT_TRUE(frame(0));
  EXPECT_FLOAT_EQ(doors.state(0).angle, 0);  // Request follows simulation.
  EXPECT_FALSE(frame(0));
  EXPECT_FALSE(frame(1.0 / 60));
  EXPECT_FLOAT_EQ(doors.state(0).angle, 1.5F);
  EXPECT_FALSE(frame(5.0 / 60));
  EXPECT_FLOAT_EQ(doors.state(0).angle, 9);
  const float paused = doors.state(0).angle;
  const float feedback_time = doors.state(0).feedback_seconds;
  input.interact = false;
  (void)interaction.update(input, false, {}, level, physics, doors, light);
  input.interact = true;
  (void)interaction.update(input, false, {}, level, physics, doors, light);
  clock.reset();
  const auto restored = clock.sample(FixedStepAccumulator::Clock::time_point{} +
                                     std::chrono::hours(1));
  EXPECT_EQ(restored.complete_steps, 0);
  EXPECT_FALSE(frame(0));
  EXPECT_FLOAT_EQ(doors.state(0).angle, paused);
  EXPECT_FLOAT_EQ(doors.state(0).feedback_seconds, feedback_time);
  EXPECT_FALSE(frame(10));  // A long active stall admits only 100 ms.
  EXPECT_FLOAT_EQ(doors.state(0).angle, paused + 9);
  for (const auto outcome : {FrameOutcome::Skipped, FrameOutcome::Recovered,
                             FrameOutcome::Rendered}) {
    EXPECT_TRUE(runtimeContinuesAfter(outcome));
    EXPECT_FALSE(frame(0));
    EXPECT_FLOAT_EQ(doors.state(0).angle, paused + 9);
  }
  EXPECT_EQ(doc, original);
  const DoorController restarted(level.doors());
  EXPECT_FLOAT_EQ(restarted.state(0).angle, 0);
  EXPECT_FALSE(restarted.state(0).moving);
}

TEST(DoorFeedback, RefusalAndKnockAreDistinctWithoutChangingLeafGeometry) {
  const auto door = doorLevel().doors.front();
  const auto base = doorPresentationBoxes(door, 0, false);
  const auto refused = doorPresentationBoxes(door, 0, false, 1, 0);
  const auto knock = doorPresentationBoxes(door, 0, false, 0, 1);
  EXPECT_EQ(base[0].center, refused[0].center);
  EXPECT_EQ(base[0].center, knock[0].center);
  EXPECT_NE(base[1].center, refused[1].center);
  EXPECT_EQ(base[1].center, knock[1].center);
  EXPECT_NE(base[4].color, knock[4].color);
  EXPECT_EQ(base[4].color, refused[4].color);
}

TEST(DoorGameplay, FeedbackReplacementExpiresWithoutMovingOrUnlockingLeaf) {
  auto doc = doorLevel();
  doc.doors[0].initially_locked = true;
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  const auto initial = doorPresentationBoxes(level.doors()[0], 0, true);
  EXPECT_EQ(doors.act(0, DoorAction::Interact, insideEye()).kind,
            DoorResultKind::Refused);
  steps(physics, doors, 6);
  EXPECT_NE(doors.presentation()[1].center, initial[1].center);
  EXPECT_EQ(doors.act(0, DoorAction::Knock, insideEye()).kind,
            DoorResultKind::Knocked);
  steps(physics, doors, 6);
  const auto current = doors.presentation();
  EXPECT_EQ(current[1].center, initial[1].center);
  EXPECT_NE(current[4].color, initial[4].color);
  EXPECT_EQ(current[0].center, initial[0].center);
  steps(physics, doors, 20);
  EXPECT_FLOAT_EQ(doors.state(0).feedback_seconds, 0);
  EXPECT_EQ(doors.presentation()[4].color, initial[4].color);
  EXPECT_TRUE(doors.state(0).locked);
  EXPECT_FALSE(doors.state(0).moving);
  EXPECT_FLOAT_EQ(doors.state(0).angle, 0);
  EXPECT_FLOAT_EQ(physics.doorAngle(0), 0);
}

TEST(DoorPhysics, MeetingLeavesUseStableIdentityOrder) {
  std::array<std::array<float, 2>, 2> results{};
  for (int order = 0; order < 2; ++order) {
    auto doc = doorLevel();
    auto a = doc.doors.front();
    a.id = "a";
    a.hinge_position = {0, 0.02F, 0};
    a.width = 1;
    auto b = a;
    b.id = "b";
    b.hinge_position = {1, 0.02F, -1};
    b.closed_yaw_degrees = 180;
    doc.doors = order == 0 ? std::vector<DoorDefinition>{a, b}
                           : std::vector<DoorDefinition>{b, a};
    const auto level = makePrototypeLevel(doc);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    (void)doors.act(0, DoorAction::Interact, insideEye());
    (void)doors.act(1, DoorAction::Interact, insideEye());
    steps(physics, doors, 100);
    EXPECT_FALSE(
        yawedBoxesOverlap(doorLeafPose(doc.doors[0], doors.state(0).angle),
                          doorLeafPose(doc.doors[1], doors.state(1).angle)));
    for (std::size_t i = 0; i < 2; ++i)
      results[order][doc.doors[i].id == "a" ? 0 : 1] = doors.state(i).angle;
  }
  EXPECT_NEAR(results[0][0], results[1][0], 0.001F);
  EXPECT_NEAR(results[0][1], results[1][1], 0.001F);
  EXPECT_TRUE(results[0][0] < 90 || results[0][1] < 90);
}

TEST(DoorPhysics, BothSwingSignsAndBoundsRetainFloorClearance) {
  for (float sign : {-1.0F, 1.0F}) {
    auto doc = doorLevel();
    auto& door = doc.doors.front();
    door.hinge_position.x = -5;
    door.width = 2.5F;
    door.height = 3.5F;
    door.thickness = 0.04F;
    door.open_angle_degrees = 170 * sign;
    door.speed_degrees_per_second = 180;
    const auto level = makePrototypeLevel(doc);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    (void)doors.act(0, DoorAction::Interact, insideEye());
    steps(physics, doors, 65);
    EXPECT_FLOAT_EQ(doors.state(0).angle, 170 * sign);
    EXPECT_FALSE(doors.state(0).moving);
    EXPECT_THROW((void)physics.advanceDoor(0, 180 * sign),
                 std::invalid_argument);
  }
}

TEST(DoorInput, LockMappingRetainsOtherActionsAndLook) {
  InputAccumulator input;
  input.setKey(PhysicalKey::R, true);
  auto actions = PlayerInputMapper{}.map(input.snapshot());
  EXPECT_TRUE(actions.lock);
  EXPECT_FALSE(actions.interact);
  EXPECT_FALSE(actions.primary_action);
  EXPECT_FALSE(actions.secondary_action);
  input.beginEventBatch();
  EXPECT_TRUE(PlayerInputMapper{}.map(input.snapshot()).lock);
  input.setKey(PhysicalKey::R, false);
  EXPECT_FALSE(PlayerInputMapper{}.map(input.snapshot()).lock);
}
