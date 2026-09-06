#include <gtest/gtest.h>

#include <limits>

#include "core/gameplay/light_switch_controller.hpp"
#include "core/gameplay/player_flashlight.hpp"
#include "core/simulation/fixed_step.hpp"
#include "prototype_level_fixture.hpp"

namespace {
PlayerViewPose approach(float distance = 1.0F) {
  return {{0.0F, 1.6F, 1.05F + light_switch_half_extent.z + distance},
          {0.0F, 0.0F, -1.0F}};
}
void press(LightSwitchController& controller, const PhysicsWorld& physics,
           PlayerViewPose view = approach()) {
  controller.update(false, true, view, physics);
  controller.update(true, true, view, physics);
}
}  // namespace

TEST(StaticVisibility, SegmentIncludesEndpointAndSolidInteriorButNotBeyond) {
  const auto level = makePrototypeLevel(prototypeLevelDocument());
  const PhysicsWorld physics(level);
  EXPECT_FALSE(physics.staticSegmentBlocked({0, 1.6F, 3}, {0, 1.6F, 1.07F}));
  EXPECT_TRUE(physics.staticSegmentBlocked({0, 1.6F, 3}, {0, 1.6F, 1.0F}));
  EXPECT_TRUE(physics.staticSegmentBlocked({0, 1.6F, 3}, {0, 1.6F, -1}));
  EXPECT_TRUE(physics.staticSegmentBlocked({0, 1.6F, 0}, {0, 1.6F, 3}));
  EXPECT_TRUE(physics.staticSegmentBlocked({0, 1.6F, 0}, {0, 1.6F, 0.1F}));
  EXPECT_TRUE(physics.staticSegmentBlocked({0, 1, 7}, {0, 1, 7}));
  EXPECT_TRUE(physics.staticSegmentBlocked(
      {std::numeric_limits<float>::quiet_NaN(), 1, 7}, {0, 1, 6}));
}

TEST(StaticVisibility,
     TerrainRotatedProxyAndPlayerExclusionUsePhysicsFixtures) {
  auto document = prototypeLevelDocument();
  document.static_prop.yaw_degrees = 90;
  document.static_prop.box_proxy_half_extent = {0.2F, 0.8F, 1.0F};
  const auto level = makePrototypeLevel(document);
  const PhysicsWorld physics(level);
  EXPECT_TRUE(physics.staticSegmentBlocked({10, 1, 10}, {10, -1, 10}));
  EXPECT_TRUE(physics.staticSegmentBlocked({10, -1, 10}, {10, 1, 10}));
  const auto center = prototypeStaticPropProxyWorldCenter(document.static_prop);
  // At +0.8 X this intersects the yawed proxy, outside its unrotated X extent.
  EXPECT_TRUE(
      physics.staticSegmentBlocked({center.x + 0.8F, center.y, center.z + 1},
                                   {center.x + 0.8F, center.y, center.z - 1}));
  EXPECT_TRUE(
      physics.staticSegmentBlocked(center, {center.x, center.y, center.z + 2}));
  EXPECT_FALSE(
      physics.staticSegmentBlocked({center.x, center.y, center.z + 1},
                                   {center.x, center.y, center.z + 0.3F}));
  const auto spawn = document.entries.front().pose.foot_position;
  EXPECT_FALSE(
      physics.staticSegmentBlocked({spawn.x, spawn.y + 0.9F, spawn.z},
                                   {spawn.x, spawn.y + 0.9F, spawn.z - 1}));
  EXPECT_EQ(physics.staticBodyCount(), document.solids.size() + 1);
}

TEST(LightSwitchTarget, InclusiveReachMissInsideAndYaw) {
  PrototypeLightSwitch value{{0, 1.6F, 1.05F}, 0, 0, true};
  const auto view = approach(2.0F);
  const auto hit = lightSwitchRayDistance(
      value, {view.position.x, view.position.y, view.position.z}, {0, 0, -4});
  ASSERT_TRUE(hit);
  EXPECT_FLOAT_EQ(*hit, 2.0F);
  EXPECT_FALSE(lightSwitchRayDistance(value, value.position, {0, 0, -1}));
  EXPECT_FALSE(lightSwitchRayDistance(value, {1, 1.6F, 3}, {0, 0, -1}));
  EXPECT_FALSE(lightSwitchRayDistance(value, {0, 1.6F, 3}, {0, 0, 1}));
  EXPECT_FALSE(lightSwitchRayDistance(value, {0, 1.6F, 3}, {}));
  value.yaw_degrees = 90;
  const auto yawed =
      lightSwitchRayDistance(value, {1.02F, 1.6F, 1.05F}, {-1, 0, 0});
  ASSERT_TRUE(yawed);
  EXPECT_NEAR(*yawed, 1.0F, 0.000001F);
}

TEST(LightSwitchController,
     InitialValuesTogglesAndRestartPreserveAuthoredState) {
  const auto level = makePrototypeLevel(prototypeLevelDocument());
  const PhysicsWorld physics(level);
  for (const auto slot : {0U, 1U}) {
    for (const bool on : {false, true}) {
      const std::optional definition{
          PrototypeLightSwitch{{0, 1.6F, 1.05F}, 0, slot, on}};
      const auto original = definition;
      LightSwitchController controller(definition);
      EXPECT_EQ(controller.pointLightEnabled()[slot], on);
      EXPECT_TRUE(controller.pointLightEnabled()[1 - slot]);
      controller.update(true, true, approach(), physics);  // unarmed startup
      EXPECT_EQ(controller.pointLightEnabled()[slot], on);
      press(controller, physics, approach(2.0F));
      EXPECT_EQ(controller.pointLightEnabled()[slot], !on);
      for (int i = 0; i < 3; ++i)
        controller.update(true, true, approach(), physics);
      EXPECT_EQ(controller.pointLightEnabled()[slot], !on);
      press(controller, physics);
      EXPECT_EQ(controller.pointLightEnabled()[slot], on);
      EXPECT_TRUE(controller.pointLightEnabled()[1 - slot]);
      EXPECT_EQ(definition, original);
      press(controller, physics);
      const LightSwitchController restarted(definition);
      EXPECT_EQ(restarted.pointLightEnabled()[slot], on);
    }
  }
  const std::optional<PrototypeLightSwitch> absent;
  LightSwitchController none(absent);
  press(none, physics);
  EXPECT_EQ(none.pointLightEnabled(), (std::array<bool, 2>{true, true}));
}

TEST(LightSwitchController, RejectedAndInactivePressesCannotBeDeferred) {
  const auto level = makePrototypeLevel(prototypeLevelDocument());
  const PhysicsWorld physics(level);
  LightSwitchController controller(level.lightSwitch());
  for (const auto view :
       {approach(2.01F), PlayerViewPose{{1, 1.6F, 3}, {0, 0, -1}},
        PlayerViewPose{{0, 1.6F, -0.81F}, {0, 0, 1}},
        PlayerViewPose{{0, 1.6F, 0.5F}, {0, 0, 1}},
        PlayerViewPose{{0, 1.6F, 1.05F}, {0, 0, 1}}}) {
    press(controller, physics, view);
    controller.update(true, true, approach(), physics);
    EXPECT_TRUE(controller.pointLightEnabled()[0]);
  }
  controller.update(false, false, {}, physics);
  controller.update(true, false, {}, physics);
  controller.update(true, true, approach(), physics);
  EXPECT_TRUE(controller.pointLightEnabled()[0]);
  press(controller, physics);
  EXPECT_FALSE(controller.pointLightEnabled()[0]);
}

TEST(LightSwitchController, EventBatchesAndPresentationDoNotReplayPresses) {
  auto document = prototypeLevelDocument();
  document.entries.front().pose.foot_position = {
      0, prototypeTerrainHeightAt(*document.terrain, 0, 2.5F), 2.5F};
  const auto level = makePrototypeLevel(document);
  PhysicsWorld physics(level);
  PlayerController player(physics, -90);
  PlayerFlashlight flashlight;
  LightSwitchController controller(level.lightSwitch());
  controller.update(false, true, player.viewPose(0), physics);
  FixedStepAccumulator clock;
  bool enabled = true;
  for (const double elapsed : {0.0, 1.0 / 60, 0.075}) {
    PlayerActionSnapshot input{};
    input.interact = true;
    player.sampleInput(input, true);
    const auto batch = clock.advance(elapsed);
    for (int i = 0; i < batch.complete_steps; ++i) player.fixedStep(1.0F / 60);
    const auto view = player.viewPose(batch.interpolation_alpha);
    controller.update(input.interact, true, view, physics);
    enabled = !enabled;
    EXPECT_EQ(controller.pointLightEnabled()[0], enabled);
    for (const auto outcome : {FrameOutcome::Skipped, FrameOutcome::Recovered,
                               FrameOutcome::Rendered}) {
      EXPECT_TRUE(runtimeContinuesAfter(outcome));
      controller.update(true, true, view, physics);
      EXPECT_EQ(controller.pointLightEnabled()[0], enabled);
    }
    controller.update(false, true, view, physics);
  }
  for (const bool captured : {true, false}) {
    PlayerActionSnapshot input{};
    input.interact = true;
    input.menu = captured;
    input.primary_action = !captured;
    const auto transition = playerCursorTransition(captured, input);
    const bool active = playerControlsActive(captured, transition);
    controller.update(true, active, player.viewPose(0), physics);
    flashlight.samplePrimaryAction(input.primary_action, active);
    controller.update(true, true, player.viewPose(0), physics);
    EXPECT_EQ(controller.pointLightEnabled()[0], enabled);
    controller.update(false, false, {}, physics);
  }
  // A minimized poll, waited press, then restored held batch.
  controller.update(false, false, {}, physics);
  controller.update(true, false, {}, physics);
  clock.reset();
  controller.update(true, true, player.viewPose(0), physics);
  EXPECT_EQ(controller.pointLightEnabled()[0], enabled);
  EXPECT_FALSE(flashlight.enabled());
}
