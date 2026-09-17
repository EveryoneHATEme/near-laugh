#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/gameplay/door_controller.hpp"
#include "core/physics/physics_world.hpp"
#include "core/rotation.hpp"
#include "core/world/door.hpp"

namespace {
constexpr float dt = 1.F / 60;
constexpr std::array<float, 4> identity{0, 0, 0, 1};

LevelDocument boxScene(std::size_t count = 1) {
  LevelDocument d;
  d.solids = {{{0, -.25F, 0},
               {10, .25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-4, 0, -4}, 0}}};
  d.default_entry = "default";
  for (std::size_t i = 0; i < count; ++i)
    d.household.boxes.push_back(
        {"box-" + std::to_string(i),
         {float(i % 4) * .6F, .15F + float(i / 4) * .6F, 0},
         float(i) * 7});
  return d;
}

class PhysicsFailure {
 public:
  explicit PhysicsFailure(const std::string& stage) { set(stage.c_str()); }
  ~PhysicsFailure() { set(""); }

 private:
  static void set(const char* stage) {
#if defined(_WIN32)
    (void)_putenv_s("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE", stage);
#else
    if (*stage)
      (void)setenv("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE", stage, 1);
    else
      (void)unsetenv("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE");
#endif
  }
};

float speed(PhysicsVector v) { return std::hypot(v.x, v.y, v.z); }

void finiteState(const PhysicsBoxState& state) {
  EXPECT_TRUE(std::isfinite(state.center.x));
  EXPECT_TRUE(std::isfinite(state.center.y));
  EXPECT_TRUE(std::isfinite(state.center.z));
  EXPECT_TRUE(quaternionIsValid(state.orientation));
  EXPECT_LE(speed(state.linear_velocity), 12.001F);
  EXPECT_LE(speed(state.angular_velocity), 12.001F);
}

void sameState(const PhysicsBoxState& a, const PhysicsBoxState& b,
               float position_tolerance = 0) {
  EXPECT_NEAR(a.center.x, b.center.x, position_tolerance);
  EXPECT_NEAR(a.center.y, b.center.y, position_tolerance);
  EXPECT_NEAR(a.center.z, b.center.z, position_tolerance);
  double dot = 0, length_a = 0, length_b = 0;
  for (int i = 0; i < 4; ++i) {
    dot += double(a.orientation[i]) * b.orientation[i];
    length_a += double(a.orientation[i]) * a.orientation[i];
    length_b += double(b.orientation[i]) * b.orientation[i];
  }
  ASSERT_GT(length_a, 0);
  ASSERT_GT(length_b, 0);
  const double rotation_error_degrees =
      2 *
      std::acos(
          std::clamp(std::abs(dot) / std::sqrt(length_a * length_b), 0., 1.)) *
      180 / std::numbers::pi;
  EXPECT_LE(rotation_error_degrees, .1);
  EXPECT_NEAR(a.linear_velocity.x, b.linear_velocity.x, .01F);
  EXPECT_NEAR(a.linear_velocity.y, b.linear_velocity.y, .01F);
  EXPECT_NEAR(a.linear_velocity.z, b.linear_velocity.z, .01F);
  EXPECT_NEAR(a.angular_velocity.x, b.angular_velocity.x, .01F);
  EXPECT_NEAR(a.angular_velocity.y, b.angular_velocity.y, .01F);
  EXPECT_NEAR(a.angular_velocity.z, b.angular_velocity.z, .01F);
  EXPECT_EQ(a.sleeping, b.sleeping);
}

// Exact segment/AABB distance: slab crossings partition the segment into
// intervals where squared distance is one quadratic. Minimize each quadratic,
// including its endpoints, instead of sampling corners or reducing the OBB to
// its world AABB (both can miss a tilted face penetrating the capsule).
double segmentBoxDistance(std::array<float, 3> start, std::array<float, 3> end,
                          WorldExtent half_extent) {
  const std::array<double, 3> half{half_extent.x, half_extent.y, half_extent.z};
  std::array<double, 3> delta;
  std::vector<double> boundaries{0, 1};
  for (int axis = 0; axis < 3; ++axis) {
    delta[axis] = double(end[axis]) - start[axis];
    if (delta[axis] == 0) continue;
    for (double sign : {-1., 1.}) {
      const double crossing = (sign * half[axis] - start[axis]) / delta[axis];
      if (crossing > 0 && crossing < 1) boundaries.push_back(crossing);
    }
  }
  std::sort(boundaries.begin(), boundaries.end());
  const auto distance_squared = [&](double t) {
    double result = 0;
    for (int axis = 0; axis < 3; ++axis) {
      const double gap =
          std::max(0., std::abs(start[axis] + delta[axis] * t) - half[axis]);
      result += gap * gap;
    }
    return result;
  };
  double best = std::numeric_limits<double>::infinity();
  for (std::size_t i = 1; i < boundaries.size(); ++i) {
    const double low = boundaries[i - 1], high = boundaries[i];
    const double middle = (low + high) / 2;
    double quadratic = 0, linear = 0;
    for (int axis = 0; axis < 3; ++axis) {
      const double p = start[axis] + delta[axis] * middle;
      if (std::abs(p) <= half[axis]) continue;
      const double offset = start[axis] - std::copysign(half[axis], p);
      quadratic += delta[axis] * delta[axis];
      linear += delta[axis] * offset;
    }
    best = std::min({best, distance_squared(low), distance_squared(high)});
    if (quadratic > 0)
      best = std::min(
          best, distance_squared(std::clamp(-linear / quadratic, low, high)));
  }
  return std::sqrt(best);
}

double capsuleBoxClearance(const PhysicsCharacterState& player,
                           WorldPosition center, WorldExtent half_extent,
                           std::array<float, 4> orientation) {
  const float height = player.stance == PhysicsPlayerStance::Crouched
                           ? player_crouched_height
                           : player_standing_height;
  const auto feet = player.foot_position;
  // CharacterVirtual raises both its physical capsule and owned inner body by
  // padding. The additional virtual skin is not physical interpenetration.
  const std::array<float, 3> bottom{feet.x - center.x,
                                    feet.y + prototype_player_contact_padding +
                                        player_capsule_radius - center.y,
                                    feet.z - center.z};
  const std::array<float, 3> top{bottom[0],
                                 feet.y + prototype_player_contact_padding +
                                     height - player_capsule_radius - center.y,
                                 bottom[2]};
  for (int axis = 0; axis < 3; ++axis) orientation[axis] = -orientation[axis];
  return segmentBoxDistance(rotateVector(orientation, bottom),
                            rotateVector(orientation, top), half_extent) -
         player_capsule_radius;
}

double checkPlayerClearance(const PhysicsCharacterState& player,
                            const PhysicsWorld& physics) {
  double nearest_box = std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < physics.boxCount(); ++index) {
    const auto box = physics.boxState(index);
    const double clearance = capsuleBoxClearance(
        player, box.center,
        {household_box_half_extent, household_box_half_extent,
         household_box_half_extent},
        box.orientation);
    nearest_box = std::min(nearest_box, clearance);
    EXPECT_GE(clearance, -.001)
        << "box=" << index
        << " skin gap=" << clearance - prototype_player_contact_padding;
  }
  for (std::size_t index = 0; index < physics.staticBodyCount(); ++index) {
    const auto solid = physics.staticBody(index);
    const double clearance =
        capsuleBoxClearance(player, solid.center, solid.half_extent,
                            yawQuaternion(solid.yaw_degrees));
    EXPECT_GE(clearance, -.001)
        << "static=" << index
        << " skin gap=" << clearance - prototype_player_contact_padding;
  }
  return nearest_box;
}

void advance(PhysicsWorld& physics, int count) {
  for (int i = 0; i < count; ++i) physics.advanceWorld(dt);
}

void addLandingLedge(LevelDocument& d, float height) {
  // Entries require an authored support. Reach the drop with ordinary motion,
  // so the falling-player cases also exercise accepted inner-body transforms.
  d.entries[0].pose.foot_position = {-1, height, 0};
  d.solids.push_back({{-1, height - .1F, 0},
                      {.6F, .1F, 1},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::Floor});
}

PhysicsCharacterState walkOffLandingLedge(PhysicsWorld& physics) {
  auto player = physics.characterState();
  for (int step = 0; step < 60; ++step) {
    physics.advanceWorld(dt);
    (void)checkPlayerClearance(player, physics);
    player = physics.stepCharacter(
        {{1, (player.supported() ? 0.F : player.linear_velocity.y) - .3F, 0}},
        dt);
    (void)checkPlayerClearance(player, physics);
  }
  EXPECT_NEAR(player.foot_position.x, 0, .03F);
  return player;
}
}  // namespace

TEST(HouseholdPhysics,
     ClearanceOracleDetectsFaceCornerAndFullRotationContacts) {
  EXPECT_DOUBLE_EQ(segmentBoxDistance({-2, 2, 0}, {2, -2, 0}, {1, 1, 1}), 0);
  EXPECT_DOUBLE_EQ(segmentBoxDistance({2, 0, 0}, {2, 2, 0}, {1, 1, 1}), 1);
  EXPECT_NEAR(segmentBoxDistance({2, 2, 2}, {2, 3, 2}, {1, 1, 1}),
              std::sqrt(3.), 1e-12);
  PhysicsCharacterState player;
  player.foot_position = {.5F, 0, 0};
  EXPECT_NEAR(
      capsuleBoxClearance(player, {0, .9F, 0}, {.15F, .15F, .15F}, identity), 0,
      1e-6);
  EXPECT_LT(capsuleBoxClearance(player, {0, .9F, 0}, {.15F, .15F, .15F},
                                yawQuaternion(45)),
            -.06);
  player.foot_position = {0, .4F, 0};
  EXPECT_NEAR(
      capsuleBoxClearance(player, {0, .25F, 0}, {.15F, .15F, .15F}, identity),
      prototype_player_contact_padding, 1e-6);
  const float half_angle = std::numbers::pi_v<float> / 8;
  EXPECT_LT(
      capsuleBoxClearance(player, {0, .25F, 0}, {.15F, .15F, .15F},
                          {std::sin(half_angle), 0, 0, std::cos(half_angle)}),
      -.04);
}

TEST(HouseholdPhysics, InitialStateGravityRotationRestAndWakeUseSameBody) {
  auto d = boxScene();
  d.household.boxes[0].center.y = 2;
  d.household.boxes[0].yaw_degrees = 37;
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  ASSERT_EQ(physics.boxCount(), 1U);
  EXPECT_THROW((void)physics.boxState(1), std::out_of_range);
  auto state = physics.boxState(0);
  EXPECT_EQ(state.center, d.household.boxes[0].center);
  for (int i = 0; i < 4; ++i)
    EXPECT_NEAR(state.orientation[i], yawQuaternion(37)[i], .000001F);
  EXPECT_EQ(speed(state.linear_velocity), 0);
  EXPECT_EQ(speed(state.angular_velocity), 0);
  physics.applyBoxImpulse(0, {}, {.03F, .01F, .02F});
  advance(physics, 10);
  state = physics.boxState(0);
  EXPECT_LT(state.center.y, 1.9F);
  EXPECT_GT(std::abs(state.orientation[0]), .01F);
  advance(physics, 600);
  state = physics.boxState(0);
  finiteState(state);
  EXPECT_NEAR(state.center.y, .15F, .008F);
  EXPECT_TRUE(state.sleeping);
  ASSERT_TRUE(
      physics.beginBoxHold(0, {state.center.x, .8F, state.center.z}, identity));
  EXPECT_FALSE(physics.boxState(0).sleeping);
  advance(physics, 90);
  EXPECT_GT(physics.boxState(0).center.y, .7F);
  EXPECT_EQ(physics.boxCount(), 1U);
}

TEST(HouseholdPhysics, StartupFailuresUnwindInnerAndPartialBoxOwnership) {
  const auto level = makePrototypeLevel(boxScene(16));
  std::vector<std::string> failures{"player-inner-body", "character"};
  for (int i = 1; i <= 16; ++i)
    failures.push_back("box-body-" + std::to_string(i));
  for (const auto& stage : failures) {
    SCOPED_TRACE(stage);
    {
      PhysicsFailure failure(stage);
      EXPECT_THROW((void)PhysicsWorld(level), std::runtime_error);
    }
    PhysicsWorld recovered(level);
    ASSERT_EQ(recovered.boxCount(), 16U);
    recovered.advanceWorld(dt);
  }
}

TEST(HouseholdPhysics, FailedStanceRetainsBothPlayerShapesAndSelfRayIsClear) {
  auto d = boxScene();
  d.entries[0].pose.foot_position = {0, 0, 0};
  d.household.boxes[0].center = {0, 1.5F, 1.5F};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  {
    PhysicsFailure failure("player-stance");
    EXPECT_EQ(physics.stepCharacter({{}, {0, -18, 0}, true}, dt).stance,
              PhysicsPlayerStance::Standing);
  }
  // No character updates follow: impact must hit the retained standing body.
  physics.applyBoxImpulse(0, {0, 0, -12});
  for (int i = 0; i < 10; ++i) {
    physics.advanceWorld(dt);
    EXPECT_GT(physics.boxState(0).center.z, .40F);
  }
  EXPECT_TRUE(physics.characterState().stance == PhysicsPlayerStance::Standing);
  EXPECT_FALSE(physics.worldSegmentBlocked({0, 1.5F, 0}, {0, 1.5F, -.5F}));
  EXPECT_EQ(physics.stepCharacter({{}, {0, -18, 0}, true}, dt).stance,
            PhysicsPlayerStance::Crouched);
  {
    PhysicsFailure failure("player-stance");
    EXPECT_EQ(physics.stepCharacter({{}, {0, -18, 0}, false}, dt).stance,
              PhysicsPlayerStance::Crouched);
  }
}

TEST(HouseholdPhysics,
     MovingBoxHitsStationaryStandingAndCrouchedPlayerWithoutPropulsion) {
  for (bool crouched : {false, true}) {
    auto d = boxScene();
    d.entries[0].pose.foot_position = {0, 0, 0};
    d.household.boxes[0].center = {0, crouched ? .65F : 1.1F, 1.5F};
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    (void)physics.stepCharacter({{}, {0, -18, 0}, crouched}, dt);
    const auto before = physics.characterState();
    physics.applyBoxImpulse(0, {0, 0, -12}, {.08F, .08F, .08F});
    for (int i = 0; i < 12; ++i) {
      physics.advanceWorld(dt);
      const auto state = physics.boxState(0);
      finiteState(state);
      EXPECT_GT(state.center.z, .35F);
    }
    const auto after = physics.stepCharacter({{}, {0, -18, 0}, crouched}, dt);
    EXPECT_NEAR(after.foot_position.x, before.foot_position.x, .0001F);
    EXPECT_NEAR(after.foot_position.z, before.foot_position.z, .0001F);
    EXPECT_LE(speed(after.linear_velocity), .001F);
  }
}

TEST(HouseholdPhysics,
     VisibilityExcludesOnlySelectedSurfaceAndAlwaysRejectsInside) {
  auto d = boxScene(2);
  d.household.boxes = {{"front", {0, 1, 0}, 37}, {"rear", {0, 1, 1}, 0}};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1, -1}, {0, 1, -.18F}));
  EXPECT_FALSE(
      physics.worldSegmentBlocked({0, 1, -1}, {0, 1, -.18F}, {}, "front"));
  EXPECT_TRUE(
      physics.worldSegmentBlocked({0, 1, -1}, {0, 1, .85F}, {}, "rear"));
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1, 0}, {0, 1, -1}, {}, "front"));
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1, 0}, {0, 1, 0}, {}, "front"));
}

TEST(HouseholdPhysics,
     MotorFailureDropThrowAndRepeatedReleaseAreTransactional) {
  const auto level = makePrototypeLevel(boxScene());
  for (int cycle = 0; cycle < 3; ++cycle) {
    PhysicsWorld physics(level);
    const auto initial = physics.boxState(0);
    for (const auto* stage : {"hold-constraint", "hold-constraint-created"}) {
      PhysicsFailure failure(stage);
      EXPECT_FALSE(physics.beginBoxHold(0, {0, .8F, 0}, identity));
      EXPECT_FALSE(physics.heldBox());
      sameState(initial, physics.boxState(0));
    }
    for (int release = 0; release < 30; ++release) {
      const auto state = physics.boxState(0);
      ASSERT_TRUE(physics.beginBoxHold(0, state.center, state.orientation));
      EXPECT_FALSE(physics.beginBoxHold(0, state.center, state.orientation));
      sameState(state, physics.boxState(0));
      physics.dropHeldBox();
      sameState(state, physics.boxState(0));
      physics.dropHeldBox();
      EXPECT_FALSE(physics.throwHeldBox({0, 0, 1}));
      EXPECT_EQ(physics.boxCount(), 1U);
    }
    ASSERT_TRUE(physics.beginBoxHold(0, {0, .8F, 0}, identity));
    advance(physics, 60);
    const auto held = physics.boxState(0);
    ASSERT_TRUE(physics.throwHeldBox({0, 0, 2}));
    const auto thrown = physics.boxState(0);
    EXPECT_EQ(thrown.center, held.center);
    EXPECT_EQ(thrown.orientation, held.orientation);
    EXPECT_NEAR(thrown.linear_velocity.z - held.linear_velocity.z, 6, .0001F);
    EXPECT_EQ(thrown.linear_velocity.x, held.linear_velocity.x);
    EXPECT_EQ(thrown.linear_velocity.y, held.linear_velocity.y);
    EXPECT_FALSE(physics.heldBox());
    EXPECT_FALSE(physics.throwHeldBox({0, 0, 1}));
    sameState(thrown, physics.boxState(0));
    // Destruction with a live constraint exercises dependency order too.
    ASSERT_TRUE(physics.beginBoxHold(0, thrown.center, thrown.orientation));
  }
}

TEST(HouseholdPhysics,
     StillHoldFollowsNewTargetAndInvalidOrDistantTargetsRelease) {
  const auto level = makePrototypeLevel(boxScene());
  PhysicsWorld physics(level);
  ASSERT_TRUE(physics.beginBoxHold(0, {0, 1, 0}, identity));
  advance(physics, 600);
  EXPECT_FALSE(physics.boxState(0).sleeping);
  const auto suspended = physics.boxState(0);
  // Presentation/suspension has no world call and cannot advance the motor.
  sameState(suspended, physics.boxState(0));
  ASSERT_TRUE(physics.updateBoxHold({.8F, 1, 0}, yawQuaternion(90)));
  sameState(suspended, physics.boxState(0));
  physics.advanceWorld(dt);
  const auto following = physics.boxState(0);
  finiteState(following);
  ASSERT_EQ(physics.heldBox(), 0U);
  // Require observable progress and remaining lag, without fixing the motor's
  // acceleration curve or its exact per-step displacement.
  EXPECT_GT(following.center.x, suspended.center.x);
  EXPECT_LT(following.center.x, .8F);
  advance(physics, 120);
  auto state = physics.boxState(0);
  EXPECT_NEAR(state.center.x, .8F, .015F);
  EXPECT_NEAR(state.orientation[1], yawQuaternion(90)[1], .015F);
  EXPECT_FALSE(physics.updateBoxHold({4, 1, 0}, identity));
  EXPECT_FALSE(physics.heldBox());
  sameState(state, physics.boxState(0));
  ASSERT_TRUE(physics.beginBoxHold(0, state.center, state.orientation));
  EXPECT_FALSE(physics.updateBoxHold(
      {std::numeric_limits<float>::quiet_NaN(), 0, 0}, identity));
  sameState(state, physics.boxState(0));
}

TEST(HouseholdPhysics, BlockedHoldAndNearWallThrowNeverRelocateBox) {
  auto d = boxScene();
  d.household.boxes[0].center = {0, 1, -.3F};
  d.solids.push_back({{0, 1.5F, 0},
                      {3, 1.5F, .01F},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::Boundary});
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  ASSERT_TRUE(physics.beginBoxHold(0, {0, 1, .8F}, identity));
  for (int i = 0; i < 300; ++i) {
    physics.advanceWorld(dt);
    const auto state = physics.boxState(0);
    finiteState(state);
    EXPECT_LT(state.center.z, -.13F);
  }
  const auto pose = physics.boxState(0);
  ASSERT_TRUE(physics.throwHeldBox({0, 0, 1}));
  EXPECT_EQ(physics.boxState(0).center, pose.center);
  for (int i = 0; i < 30; ++i) {
    physics.advanceWorld(dt);
    EXPECT_LT(physics.boxState(0).center.z, -.12F);
  }
}

TEST(HouseholdPhysics, MaximumSpeedSpinningCubeCannotCrossThinWallOrDoor) {
  for (bool door : {false, true})
    for (float yaw : {0.F, 31.F, 73.F}) {
      auto d = boxScene();
      d.household.boxes[0] = {"box", {.5F, 1.1F, -.5F}, yaw};
      if (door) {
        DoorDefinition leaf;
        leaf.id = "door";
        leaf.hinge_position = {-1, .02F, 0};
        leaf.width = 2.5F;
        leaf.thickness = .04F;
        d.doors = {leaf};
      } else
        d.solids.push_back({{0, 1.5F, 0},
                            {3, 1.5F, .01F},
                            {255, 255, 255, 255},
                            PrototypeSolidKind::Boundary});
      const auto level = makePrototypeLevel(d);
      PhysicsWorld physics(level);
      physics.applyBoxImpulse(0, {0, 0, 120}, {.5F, .7F, .3F});
      EXPECT_NEAR(speed(physics.boxState(0).linear_velocity), 12, .0001F);
      EXPECT_NEAR(speed(physics.boxState(0).angular_velocity), 12, .0001F);
      for (int i = 0; i < 35; ++i) {
        physics.advanceWorld(dt);
        const auto state = physics.boxState(0);
        finiteState(state);
        EXPECT_LT(state.center.z, -.1F)
            << "door=" << door << " yaw=" << yaw << " step=" << i;
      }
      if (door) EXPECT_EQ(physics.doorAngle(0), 0);
    }
}

TEST(HouseholdPhysics,
     CapacityStacksSettleAndContactWakesWithoutEnergyExplosion) {
  auto d = boxScene(16);
  for (std::size_t i = 0; i < 16; ++i)
    d.household.boxes[i] = {"box-" + std::to_string(i),
                            {float(i % 4), .15F + .30F * float(i / 4), 0},
                            0};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  advance(physics, 600);
  for (std::size_t i = 0; i < 16; ++i) {
    finiteState(physics.boxState(i));
    EXPECT_TRUE(physics.boxState(i).sleeping);
  }
  const auto top = physics.boxState(12);
  ASSERT_TRUE(physics.beginBoxHold(12, {top.center.x, 1.5F, 0}, identity));
  advance(physics, 90);
  ASSERT_TRUE(physics.throwHeldBox({1, 0, 0}));
  bool contact_woke_neighbor = false;
  for (int step = 0; step < 600; ++step) {
    physics.advanceWorld(dt);
    for (std::size_t i = 0; i < 16; ++i) finiteState(physics.boxState(i));
    contact_woke_neighbor |= !physics.boxState(13).sleeping;
  }
  EXPECT_TRUE(contact_woke_neighbor);
  EXPECT_EQ(physics.boxCount(), 16U);
}

TEST(HouseholdPhysics,
     StableIdsAndFixedBoundaryCommandsMatchAcrossBatchPartitions) {
  std::vector<PhysicsBoxState> baseline;
  for (bool reverse : {false, true}) {
    auto d = boxScene(2);
    if (reverse)
      std::reverse(d.household.boxes.begin(), d.household.boxes.end());
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    const std::size_t selected = reverse ? 1 : 0;
    int boundary = 0;
    int held_count = 0, throw_count = 0, drop_count = 0;
    while (boundary < 240) {
      const int batch_size = reverse ? std::min(5, 240 - boundary) : 1;
      for (int within = 0; within < batch_size; ++within, ++boundary) {
        if (boundary == 0)
          held_count += physics.beginBoxHold(selected, {0, .8F, 0}, identity);
        if (boundary > 0 && boundary < 120) {
          const float angle = float(boundary) * .01F;
          ASSERT_TRUE(physics.updateBoxHold(
              {.25F * std::sin(angle), .8F, .25F * std::cos(angle)},
              yawQuaternion(float(boundary))));
        }
        if (boundary == 120) throw_count += physics.throwHeldBox({0, .2F, 1});
        if (boundary == 160) {
          const auto box = physics.boxState(selected);
          held_count +=
              physics.beginBoxHold(selected, box.center, box.orientation);
        }
        if (boundary == 180) {
          physics.dropHeldBox();
          ++drop_count;
        }
        physics.advanceWorld(dt);
      }
    }
    EXPECT_EQ(held_count, 2);
    EXPECT_EQ(throw_count, 1);
    EXPECT_EQ(drop_count, 1);
    for (std::size_t i = 0; i < 2; ++i) {
      const auto state = physics.boxState(reverse ? 1 - i : i);
      if (!reverse)
        baseline.push_back(state);
      else
        sameState(state, baseline[i], .001F);
    }
  }
}

TEST(HouseholdPhysics, UpdateErrorsAndCorruptStateAreReported) {
  const auto level = makePrototypeLevel(boxScene());
  PhysicsWorld physics(level);
  {
    PhysicsFailure failure("world-update");
    EXPECT_THROW(physics.advanceWorld(dt), std::runtime_error);
  }
  {
    PhysicsFailure failure("box-state");
    EXPECT_THROW(physics.advanceWorld(dt), std::runtime_error);
  }
  EXPECT_THROW(physics.applyBoxImpulse(
                   0, {std::numeric_limits<float>::infinity(), 0, 0}),
               std::invalid_argument);
}

TEST(HouseholdPhysics, ControlledMotionPushesFreeBoxWithoutUsingItAsAStair) {
  auto d = boxScene();
  d.household.boxes[0].center = {0, .15F, 0};
  d.entries[0].pose.foot_position = {0, 0, -1};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  for (int i = 0; i < 180; ++i) {
    physics.advanceWorld(dt);
    const auto player = physics.stepCharacter({{0, -.3F, 1}}, dt);
    const auto box = physics.boxState(0);
    finiteState(box);
    EXPECT_LT(player.foot_position.y, .05F);
    EXPECT_GT(box.center.z - player.foot_position.z, .40F);
    EXPECT_LE(speed(player.linear_velocity), 1.05F);
  }
  EXPECT_GT(physics.boxState(0).center.z, 1);
}

TEST(HouseholdPhysics,
     HeldOverheadBoxRejectsStandingThenAllowsItAfterClearance) {
  auto d = boxScene();
  d.entries[0].pose.foot_position = {-1.2F, 0, 0};
  d.household.boxes[0].center = {0, 1.5F, 0};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  ASSERT_TRUE(physics.beginBoxHold(0, {0, 1.5F, 0}, identity));
  for (int i = 0; i < 72; ++i) {
    physics.advanceWorld(dt);
    (void)physics.stepCharacter({{1, -.3F, 0}, {0, -18, 0}, true}, dt);
  }
  const auto before = physics.characterState();
  EXPECT_NEAR(before.foot_position.x, 0, .03F);
  EXPECT_EQ(
      physics.stepCharacter({{0, -.3F, 0}, {0, -18, 0}, false}, dt).stance,
      PhysicsPlayerStance::Crouched);
  ASSERT_TRUE(physics.updateBoxHold({1.2F, 1.5F, 0}, identity));
  advance(physics, 120);
  EXPECT_EQ(
      physics.stepCharacter({{0, -.3F, 0}, {0, -18, 0}, false}, dt).stance,
      PhysicsPlayerStance::Standing);
}

TEST(HouseholdPhysics, IsolatedTiltedAndStackedLandingsDoNotCreateSupport) {
  for (int arrangement = 0; arrangement < 3; ++arrangement) {
    auto d = boxScene(arrangement == 2 ? 2 : 1);
    addLandingLedge(d, 3);
    d.household.boxes[0].center = {0, .15F, 0};
    if (arrangement == 2) d.household.boxes[1] = {"top", {0, .45F, 0}, 0};
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    if (arrangement == 1) {
      const float half = .25F;
      ASSERT_TRUE(physics.beginBoxHold(0, {0, .28F, 0},
                                       {std::sin(half), 0, 0, std::cos(half)}));
      advance(physics, 120);
    }
    bool contacted = false;
    double nearest_box = std::numeric_limits<double>::infinity();
    auto player = walkOffLandingLedge(physics);
    for (int i = 0; i < 240; ++i) {
      SCOPED_TRACE("arrangement=" + std::to_string(arrangement) +
                   " step=" + std::to_string(i));
      physics.advanceWorld(dt);
      (void)checkPlayerClearance(player, physics);
      const auto old = player;
      const float vertical =
          (player.supported() ? 0.F : player.linear_velocity.y) - .3F;
      player = physics.stepCharacter({{0, vertical, 0}}, dt);
      nearest_box =
          std::min(nearest_box, checkPlayerClearance(player, physics));
      const float move =
          std::hypot(player.foot_position.x - old.foot_position.x,
                     player.foot_position.y - old.foot_position.y,
                     player.foot_position.z - old.foot_position.z);
      // Cover the requested fall and the bounded 1 m/s horizontal slide on
      // supported contact normals. This remains valid after the ledge approach
      // changes the initial fall speed; there is no arbitrary pose allowance.
      const float maximum_step =
          dt * std::max(std::abs(vertical), std::sqrt(2.F));
      EXPECT_LE(move, maximum_step + .001F) << arrangement << " step=" << i;
      EXPECT_GE(player.foot_position.y, -.01F);
      if (player.foot_position.y > .1F && player.foot_position.y < .8F) {
        contacted |= player.ground_state == PhysicsGroundState::Unsupported;
        EXPECT_FALSE(player.supported());
      }
      for (std::size_t box = 0; box < physics.boxCount(); ++box)
        finiteState(physics.boxState(box));
    }
    EXPECT_TRUE(contacted) << arrangement;
    EXPECT_LT(nearest_box, .03)
        << "The falling capsule must actually contact a box";
    EXPECT_LT(player.foot_position.y, .05F) << arrangement;
    EXPECT_GT(std::hypot(player.foot_position.x, player.foot_position.z), .4F)
        << arrangement;
  }
}

TEST(HouseholdPhysics, ConfinedLandingKeepsCollisionWithoutForcedRelocation) {
  auto d = boxScene();
  addLandingLedge(d, 4.6F);
  d.household.boxes[0].center = {0, .15F, 0};
  for (int axis = 0; axis < 2; ++axis)
    for (float side : {-1.F, 1.F}) {
      d.solids.push_back(
          {{axis == 0 ? side * .42F : 0, 2, axis == 1 ? side * .42F : 0},
           {axis == 0 ? .02F : .5F, 2, axis == 1 ? .02F : .5F},
           {255, 255, 255, 255},
           PrototypeSolidKind::Boundary});
    }
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  auto player = walkOffLandingLedge(physics);
  double nearest_box = std::numeric_limits<double>::infinity();
  for (int i = 0; i < 240; ++i) {
    SCOPED_TRACE("step=" + std::to_string(i));
    physics.advanceWorld(dt);
    (void)checkPlayerClearance(player, physics);
    const auto previous = player;
    const float vertical =
        (player.supported() ? 0.F : player.linear_velocity.y) - .3F;
    player = physics.stepCharacter({{0, vertical, 0}}, dt);
    nearest_box = std::min(nearest_box, checkPlayerClearance(player, physics));
    EXPECT_LT(std::hypot(player.foot_position.x - previous.foot_position.x,
                         player.foot_position.z - previous.foot_position.z),
              .05F);
    EXPECT_LT(std::abs(player.foot_position.x), .06F);
    EXPECT_LT(std::abs(player.foot_position.z), .06F);
    EXPECT_GT(player.foot_position.y, .24F);
    if (player.foot_position.y < .8F) EXPECT_FALSE(player.supported());
    if (i > 90)
      EXPECT_GT(player.linear_velocity.y, -.31F)
          << "Blocked unsupported contact must not accumulate downward speed";
    const auto box = physics.boxState(0);
    finiteState(box);
    EXPECT_LT(std::abs(box.center.x), .25F);
    EXPECT_LT(std::abs(box.center.z), .25F);
    EXPECT_GT(box.center.y, .13F);
  }
  EXPECT_LT(nearest_box, .03)
      << "The confined capsule must actually contact the box";
}

TEST(HouseholdPhysics, ActorWaitsAtBoxThenResumesAndCannotBeMovedByAnImpact) {
  auto d = boxScene();
  d.household.boxes[0].center = {0, .15F, 1};
  d.characters.marks = {{"start", {0, 0, 0}, 0}};
  d.characters.actors = {{"actor", "test-mannequin", "start", {}, 1, {}, {}}};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  const auto waiting = physics.advanceActor(0, {0, 0, 2}, 0);
  EXPECT_EQ(waiting.obstruction, PhysicsActorObstruction::Box);
  EXPECT_LT(waiting.state.feet_position.z, .7F);
  EXPECT_NEAR(waiting.state.feet_position.y, 0, .003F);
  const auto accepted = physics.actorState(0);
  physics.applyBoxImpulse(0, {0, 0, -12});
  for (int i = 0; i < 15; ++i) {
    physics.advanceWorld(dt);
    EXPECT_EQ(physics.actorState(0).feet_position, accepted.feet_position);
    EXPECT_GT(physics.boxState(0).center.z, accepted.feet_position.z + .3F);
  }
  auto box = physics.boxState(0);
  ASSERT_TRUE(physics.beginBoxHold(
      0, {box.center.x + 1, box.center.y + .3F, box.center.z}, identity));
  advance(physics, 180);
  const auto resumed = physics.advanceActor(0, {0, 0, 2}, 0);
  EXPECT_EQ(resumed.obstruction, PhysicsActorObstruction::None);
  EXPECT_NEAR(resumed.horizontal_distance, 2, .01F);
}

TEST(HouseholdPhysics, DoorStopsBeforeFreeOrHeldBoxAndRequiresNewPress) {
  for (bool held : {false, true}) {
    auto d = boxScene();
    d.household.boxes[0].center = {0, .15F, -1};
    DoorDefinition leaf;
    leaf.id = "door";
    leaf.hinge_position = {-1, .02F, 0};
    leaf.width = 2;
    d.doors = {leaf};
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    advance(physics, 300);
    const auto original = physics.boxState(0);
    if (held)
      ASSERT_TRUE(
          physics.beginBoxHold(0, original.center, original.orientation));
    (void)doors.act(0, DoorAction::Interact, {-4, 1.5F, -4});
    for (int i = 0; i < 100; ++i) {
      physics.advanceWorld(dt);
      doors.fixedStep(dt, physics);
    }
    EXPECT_EQ(doors.state(0).feedback, DoorResultKind::Obstructed);
    EXPECT_FALSE(doors.state(0).moving);
    const auto stopped = doors.state(0).angle;
    EXPECT_GT(stopped, 0);
    EXPECT_LT(stopped, 90);
    EXPECT_NEAR(physics.boxState(0).center.x, original.center.x, .01F);
    EXPECT_NEAR(physics.boxState(0).center.z, original.center.z, .01F);
    if (held)
      ASSERT_TRUE(physics.updateBoxHold({0, .8F, -2.3F}, identity));
    else
      ASSERT_TRUE(physics.beginBoxHold(0, {0, .8F, -2.3F}, identity));
    for (int i = 0; i < 180; ++i) {
      physics.advanceWorld(dt);
      doors.fixedStep(dt, physics);
    }
    EXPECT_EQ(doors.state(0).angle, stopped);
    EXPECT_FALSE(doors.state(0).moving);
    EXPECT_TRUE(doors.state(0).target_open);
    EXPECT_EQ(doors.act(0, DoorAction::Interact, {-4, 1.5F, -4}).kind,
              DoorResultKind::Closing);
    for (int i = 0; i < 100; ++i) {
      physics.advanceWorld(dt);
      doors.fixedStep(dt, physics);
    }
    EXPECT_FLOAT_EQ(doors.state(0).angle, 0);
    EXPECT_FALSE(doors.state(0).moving);
    EXPECT_EQ(doors.state(0).feedback, DoorResultKind::Closed);
    EXPECT_EQ(doors.act(0, DoorAction::Interact, {-4, 1.5F, -4}).kind,
              DoorResultKind::Opening);
    for (int i = 0; i < 100; ++i) {
      physics.advanceWorld(dt);
      doors.fixedStep(dt, physics);
    }
    EXPECT_FLOAT_EQ(doors.state(0).angle, 90);
    EXPECT_FALSE(doors.state(0).moving);
    EXPECT_EQ(doors.state(0).feedback, DoorResultKind::Opened);
  }
}
