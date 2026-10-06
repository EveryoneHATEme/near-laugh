#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "core/gameplay/authored_interaction.hpp"
#include "core/gameplay/character_controller.hpp"
#include "core/gameplay/household_controller.hpp"
#include "core/gameplay/player_flashlight.hpp"
#include "core/render/scene_assets.hpp"
#include "core/text/caption_font.hpp"
#include "core/world/household.hpp"

namespace {
constexpr float dt = 1.F / 60;
LevelDocument scene(bool radio = false) {
  LevelDocument d;
  d.solids = {{{0, -.25F, 0},
               {10, .25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{0, 0, 0}, -90}}};
  d.default_entry = "default";
  d.household.boxes = {{"parcel", {0, 1.65F, -1}, 0}};
  d.household.documents = {{"note",
                            {1, .8F, -1},
                            25,
                            "Письмо Ёжика",
                            {"Первая страница.", "Вторая: ёлка / Latin."}}};
  if (radio) {
    d.props = {{"receiver", "apartment-radio", {2, 0, 0}, 0, 1, {}}};
    d.audio.cues = {
        {"loop", "radio", "radio", AudioCueKind::Ambience, true, true}};
    d.audio.sources = {{"receiver-loop", "loop", {7, 1, 7}, 1, 1, 20, false}};
    d.household.radios = {{"radio-control", "receiver", "receiver-loop", true}};
  }
  return d;
}

struct HouseholdRun {
  PrototypeLevel level;
  std::vector<std::shared_ptr<const CharacterAsset>> character_assets;
  CueCoordinator audio;
  PhysicsWorld physics;
  PlayerController player;
  CharacterController characters;
  HouseholdController household;
  DoorController doors;
  LightSwitchController light;
  PlayerFlashlight flashlight;
  AuthoredInteraction interaction;
  bool captured = true;
  explicit HouseholdRun(LevelDocument document = scene(),
                        AudioOutput output = AudioOutput::Silent)
      : level(makePrototypeLevel(document)),
        character_assets(
            prepareCharacterAssets("resources", level.characters())),
        audio(level.audio(), level.doors(),
              prepareAudioContent("resources", level.audio()), output, 0),
        physics(level),
        player(physics, -90),
        characters(level.characters(), character_assets, physics, audio),
        household(level, physics, audio),
        doors(level.doors()),
        light(level.environmentLight(), level.lightSwitches()) {}

  PlayerActionSnapshot batch(PlayerActionSnapshot input = {}, int steps = 0,
                             bool suspended = false,
                             std::optional<PlayerViewPose> displayed = {}) {
    auto cursor = input;
    if (household.consumesEscape()) cursor.menu = false;
    const auto transition = playerCursorTransition(captured, cursor);
    const bool active =
        !suspended && playerControlsActive(captured, transition);
    if (transition == PlayerCursorCaptureTransition::Release) captured = false;
    if (transition == PlayerCursorCaptureTransition::Capture) captured = true;
    auto controls = household.sampleInput(input, active, suspended);
    player.sampleInput(controls, household.worldActionsAllowed(),
                       household.preservesStance());
    flashlight.samplePrimaryAction(controls.primary_action,
                                   household.worldActionsAllowed());
    for (int i = 0; i < steps && !suspended; ++i) {
      household.beforeFixedStep(player.viewPose(1));
      physics.advanceWorld(dt);
      player.fixedStep(dt);
      characters.fixedStep(dt);
      doors.fixedStep(dt, physics);
    }
    characters.handoffAudio();
    (void)interaction.update(controls, active,
                             displayed.value_or(player.viewPose(1)), level,
                             physics, doors, light, &household);
    return controls;
  }
  void pickup() {
    batch();
    PlayerActionSnapshot e;
    e.interact = true;
    batch(e);
    ASSERT_TRUE(household.commandPending());
    household.beforeFixedStep(player.viewPose(1));
    ASSERT_EQ(household.heldBox(), 0U);
  }
};

void samePose(const PhysicsBoxState& a, const PhysicsBoxState& b) {
  EXPECT_EQ(a.center, b.center);
  EXPECT_EQ(a.orientation, b.orientation);
  EXPECT_EQ(a.linear_velocity.x, b.linear_velocity.x);
  EXPECT_EQ(a.linear_velocity.y, b.linear_velocity.y);
  EXPECT_EQ(a.linear_velocity.z, b.linear_velocity.z);
  EXPECT_EQ(a.angular_velocity.x, b.angular_velocity.x);
  EXPECT_EQ(a.angular_velocity.y, b.angular_velocity.y);
  EXPECT_EQ(a.angular_velocity.z, b.angular_velocity.z);
}
}  // namespace

TEST(HouseholdRuntime,
     AcceptedOutcomesDescribePhysicalApplicationAndNeverQueueOrSafetyRelease) {
  AcceptedInteractions accepted;
  {
    HouseholdRun run(scene(true));
    run.batch();
    ASSERT_TRUE(run.household.requestPickup(0, run.player.viewPose(1)));
    EXPECT_TRUE(accepted.pending().empty());
    run.household.beforeFixedStep(run.player.viewPose(1), &accepted);
    ASSERT_EQ(accepted.pending().size(), 1U);
    EXPECT_EQ(accepted.pending()[0].target, "parcel");
    EXPECT_EQ(accepted.pending()[0].action,
              NarrativeInteractionAction::BoxPickup);
    EXPECT_EQ(accepted.pending()[0].result,
              AcceptedInteractionResult::PickedUp);
    const auto serial = accepted.pending()[0].occurrence;
    accepted.consume();
    run.household.beforeFixedStep(run.player.viewPose(1), &accepted);
    EXPECT_TRUE(accepted.pending().empty());
    ASSERT_TRUE(run.household.requestDrop());
    EXPECT_TRUE(accepted.pending().empty());
    run.household.beforeFixedStep(run.player.viewPose(1), &accepted);
    ASSERT_EQ(accepted.pending().size(), 1U);
    EXPECT_GT(accepted.pending()[0].occurrence, serial);
    EXPECT_EQ(accepted.pending()[0].action,
              NarrativeInteractionAction::BoxDrop);
    accepted.consume();
    ASSERT_TRUE(run.household.requestPickup(0, run.player.viewPose(1)));
    run.household.beforeFixedStep(run.player.viewPose(1), &accepted);
    accepted.consume();
    ASSERT_TRUE(run.household.requestThrow({0, 0, -1}));
    run.household.beforeFixedStep(run.player.viewPose(1), &accepted);
    ASSERT_EQ(accepted.pending().size(), 1U);
    EXPECT_EQ(accepted.pending()[0].action,
              NarrativeInteractionAction::BoxThrow);
  }
  {
    HouseholdRun safety;
    safety.pickup();
    accepted.consume();
    (void)safety.household.sampleInput({}, false, false);
    safety.household.beforeFixedStep(safety.player.viewPose(1), &accepted);
    EXPECT_TRUE(accepted.pending().empty());
    EXPECT_FALSE(safety.household.heldBox());
  }
  {
    HouseholdRun suspended;
    suspended.batch();
    ASSERT_TRUE(
        suspended.household.requestPickup(0, suspended.player.viewPose(1)));
    suspended.household.suspend();
    suspended.household.beforeFixedStep(suspended.player.viewPose(1),
                                        &accepted);
    EXPECT_TRUE(accepted.pending().empty());
    EXPECT_FALSE(suspended.household.heldBox());
  }
}

TEST(HouseholdRuntime,
     DocumentRadioAndRefusedActionsPublishOnlyAcceptedOutcomes) {
  HouseholdRun run(scene(true));
  AcceptedInteractions accepted;
  run.household.toggleRadio(0, &accepted);  // Inactive at startup.
  run.household.openDocument(0, &accepted);
  EXPECT_TRUE(accepted.pending().empty());
  run.batch();
  run.household.toggleRadio(0, &accepted);
  ASSERT_EQ(accepted.pending().size(), 1U);
  EXPECT_EQ(accepted.pending()[0].action, NarrativeInteractionAction::RadioOff);
  run.household.toggleRadio(0, &accepted);
  ASSERT_EQ(accepted.pending().size(), 2U);
  EXPECT_EQ(accepted.pending()[1].action, NarrativeInteractionAction::RadioOn);
  run.household.openDocument(0, &accepted);
  ASSERT_EQ(accepted.pending().size(), 3U);
  EXPECT_EQ(accepted.pending()[2].target, "note");
  EXPECT_EQ(accepted.pending()[2].action,
            NarrativeInteractionAction::DocumentOpen);
  run.household.openDocument(0, &accepted);
  run.household.toggleRadio(0, &accepted);
  EXPECT_EQ(accepted.pending().size(), 3U);
  ASSERT_TRUE(run.household.setRadioEnabled(0, false));
  EXPECT_EQ(accepted.pending().size(),
            3U);  // Author command is not player input.
}

TEST(HouseholdRuntime, AuthoredRadioStatePreservesInstancesAndPlayerModes) {
  {
    HouseholdRun run(scene(true));
    const auto original = run.level.household();
    const auto first = run.audio.instance("receiver-loop");
    run.audio.update(.5);
    ASSERT_TRUE(run.household.setRadioEnabled(0, true));
    EXPECT_EQ(run.audio.instance("receiver-loop"), first);
    EXPECT_DOUBLE_EQ(run.audio.offset("receiver-loop"), .5);
    run.batch();
    run.household.toggleRadio(0);
    EXPECT_FALSE(run.household.radioOn(0));
    run.household.openDocument(0);
    ASSERT_EQ(run.household.readingDocument(), 0U);
    ASSERT_TRUE(run.household.setRadioEnabled(0, true));
    EXPECT_GT(run.audio.instance("receiver-loop"), first);
    EXPECT_EQ(run.household.readingDocument(), 0U);
    EXPECT_EQ(run.household.page(), 0U);
    ASSERT_TRUE(run.household.setRadioEnabled(0, false));
    EXPECT_EQ(run.household.readingDocument(), 0U);
    EXPECT_EQ(run.audio.status("receiver-loop"), CueStatus::Cancelled);
    EXPECT_FALSE(run.household.setRadioEnabled(1, true));
    EXPECT_EQ(run.level.household(), original);
  }
  HouseholdRun carrying(scene(true));
  carrying.pickup();
  ASSERT_TRUE(carrying.household.setRadioEnabled(0, false));
  ASSERT_TRUE(carrying.household.setRadioEnabled(0, true));
  EXPECT_EQ(carrying.household.heldBox(), 0U);
  EXPECT_EQ(carrying.audio.status("receiver-loop"), CueStatus::Playing);
}

TEST(HouseholdRuntime,
     PickupAndThrowWaitForOneBoundaryAndKeepAcceptedDirection) {
  HouseholdRun run;
  run.batch();
  const auto initial = run.physics.boxState(0);
  PlayerActionSnapshot e;
  e.interact = true;
  run.batch(e);
  ASSERT_TRUE(run.household.commandPending());
  EXPECT_FALSE(run.household.heldBox());
  for (int batch = 0; batch < 8; ++batch) run.batch(e);
  samePose(initial, run.physics.boxState(0));
  run.household.beforeFixedStep(run.player.viewPose(1));
  ASSERT_EQ(run.household.heldBox(), 0U);
  EXPECT_FALSE(run.household.commandPending());
  samePose(initial, run.physics.boxState(0));
  run.batch();
  PlayerActionSnapshot throw_input;
  throw_input.secondary_action = true;
  run.batch(throw_input);
  ASSERT_TRUE(run.household.commandPending());
  for (int batch = 0; batch < 8; ++batch) run.batch(throw_input);
  samePose(initial, run.physics.boxState(0));
  auto changed_look = run.player.viewPose(1);
  changed_look.direction = {1, 0, 0};
  run.household.beforeFixedStep(changed_look);
  const auto thrown = run.physics.boxState(0);
  EXPECT_FALSE(run.household.heldBox());
  EXPECT_EQ(run.household.result(), HouseholdResult::Thrown);
  EXPECT_NEAR(thrown.linear_velocity.z, -6, .0001F);
  EXPECT_NEAR(thrown.linear_velocity.x, 0, .0001F);
  EXPECT_EQ(thrown.center, initial.center);
  for (int boundary = 0; boundary < 6; ++boundary)
    run.household.beforeFixedStep(changed_look);
  samePose(thrown, run.physics.boxState(0));
  EXPECT_EQ(run.physics.boxCount(), 1U);
}

TEST(HouseholdRuntime, DropOutranksThrowAndPendingCommandsRefuseCompetition) {
  HouseholdRun run;
  run.pickup();
  run.batch();
  PlayerActionSnapshot both;
  both.interact = both.secondary_action = both.lock = true;
  const auto held = run.physics.boxState(0);
  run.batch(both);
  ASSERT_TRUE(run.household.commandPending());
  run.batch();
  PlayerActionSnapshot right;
  right.secondary_action = true;
  run.batch(right);
  EXPECT_EQ(run.household.result(), HouseholdResult::Busy);
  run.household.beforeFixedStep(run.player.viewPose(1));
  EXPECT_EQ(run.household.result(), HouseholdResult::Dropped);
  EXPECT_FALSE(run.household.heldBox());
  samePose(held, run.physics.boxState(0));
  run.batch(right);
  EXPECT_FALSE(run.household.commandPending());
}

TEST(HouseholdRuntime, RecheckCannotReachOrReplaceMovedSelectedBox) {
  HouseholdRun run;
  run.batch();
  PlayerActionSnapshot e;
  e.interact = true;
  run.batch(e);
  ASSERT_TRUE(run.household.commandPending());
  auto far = run.player.viewPose(1);
  far.position.x = 4;
  run.household.beforeFixedStep(far);
  EXPECT_FALSE(run.household.heldBox());
  EXPECT_FALSE(run.household.commandPending());
  EXPECT_EQ(run.household.result(), HouseholdResult::Refused);
  run.batch(e, 3);
  EXPECT_FALSE(run.household.heldBox());
}

TEST(HouseholdRuntime,
     SuspensionCancelsRequestWhileSafetyReleaseSurvivesRecapture) {
  HouseholdRun run;
  run.batch();
  PlayerActionSnapshot e;
  e.interact = true;
  run.batch(e);
  run.batch(e, 0, true);
  EXPECT_FALSE(run.household.commandPending());
  run.batch(e, 1);
  EXPECT_FALSE(run.household.heldBox());
  run.pickup();
  const auto held = run.physics.boxState(0);
  run.batch({}, 0, true);
  EXPECT_EQ(run.household.heldBox(), 0U);
  samePose(held, run.physics.boxState(0));
  run.batch();
  PlayerActionSnapshot escape;
  escape.menu = true;
  run.batch(escape);  // no physics boundary yet
  EXPECT_FALSE(run.captured);
  ASSERT_TRUE(run.household.safetyReleaseOwed());
  ASSERT_TRUE(run.household.heldBox());
  run.batch({}, 0, true);
  PlayerActionSnapshot capture;
  capture.primary_action = true;
  run.batch(capture, 0, true);
  EXPECT_TRUE(run.captured);
  EXPECT_TRUE(run.household.safetyReleaseOwed());
  run.batch(capture);
  run.household.beforeFixedStep(run.player.viewPose(1));
  EXPECT_FALSE(run.household.heldBox());
  EXPECT_FALSE(run.household.safetyReleaseOwed());
  EXPECT_EQ(run.household.result(), HouseholdResult::Released);
  samePose(held, run.physics.boxState(0));
}

TEST(HouseholdRuntime, ReadingRetainsStanceAndBlocksInheritedActionsAndEscape) {
  HouseholdRun run;
  PlayerActionSnapshot crouch;
  crouch.crouch = true;
  run.batch(crouch, 1);
  ASSERT_EQ(run.player.state().stance, PhysicsPlayerStance::Crouched);
  run.household.openDocument(0);
  ASSERT_TRUE(run.household.readingDocument());
  const auto before = run.player.state().foot_position;
  const auto yaw = run.player.yawDegrees();
  const auto box_height = run.physics.boxState(0).center.y;
  PlayerActionSnapshot moving;
  moving.move_forward = moving.jump = moving.primary_action = moving.lock =
      true;
  moving.look_delta_x = 400;
  const auto controls = run.batch(moving, 10);
  EXPECT_FALSE(controls.move_forward);
  EXPECT_EQ(run.player.yawDegrees(), yaw);
  EXPECT_EQ(run.player.state().stance, PhysicsPlayerStance::Crouched);
  EXPECT_NEAR(run.player.state().foot_position.x, before.x, .001F);
  EXPECT_NEAR(run.player.state().foot_position.z, before.z, .001F);
  EXPECT_LT(run.physics.boxState(0).center.y, box_height);
  moving.menu = true;
  run.batch(moving, 1);
  EXPECT_FALSE(run.household.readingDocument());
  EXPECT_TRUE(run.captured);
  EXPECT_TRUE(run.household.consumesEscape());
  auto filtered = run.batch(moving, 1);
  EXPECT_TRUE(run.captured);
  EXPECT_FALSE(filtered.move_forward);
  EXPECT_FALSE(filtered.primary_action);
  EXPECT_FALSE(filtered.jump);
  moving.look_delta_x = 0;
  run.batch({}, 1);
  moving.menu = false;
  filtered = run.batch(moving, 1);
  EXPECT_TRUE(filtered.move_forward);
  EXPECT_TRUE(filtered.primary_action);
  EXPECT_TRUE(filtered.jump);
}

TEST(HouseholdRuntime, ReaderPagesAndFeedbackSurviveSuspensionWithoutReplay) {
  HouseholdRun run;
  run.batch();
  run.household.openDocument(0);
  run.batch();
  PlayerActionSnapshot next;
  next.move_right = true;
  run.batch(next);
  EXPECT_EQ(run.household.page(), 1U);
  for (int i = 0; i < 6; ++i) run.batch(next);
  EXPECT_EQ(run.household.page(), 1U);
  run.audio.update(1);
  const std::string feedback(run.household.text().feedback);
  ASSERT_FALSE(feedback.empty());
  run.audio.suspend(true, 1);
  run.batch({}, 0, true);
  PlayerActionSnapshot previous;
  previous.move_left = true;
  run.batch(previous, 0, true);
  run.audio.update(1001);
  run.audio.suspend(false, 1001);
  run.batch(previous);
  EXPECT_EQ(run.household.page(), 1U);
  EXPECT_EQ(run.household.text().feedback, feedback);
  run.batch();
  run.batch(previous);
  EXPECT_EQ(run.household.page(), 0U);
  run.batch();
  run.batch(previous);
  EXPECT_EQ(run.household.page(), 0U);
  run.audio.update(1002.6);
  EXPECT_TRUE(run.household.text().feedback.empty());
}

TEST(HouseholdRuntime, ReadingAllowsConcurrentCharacterBoxAndRadioActivity) {
  auto d = scene(true);
  d.characters.marks = {{"start", {3, 0, -3}, 0}, {"end", {3, 0, 3}, 180}};
  d.characters.actors = {
      {"walker", "test-mannequin", "start", "walk", .75F, {}, {}}};
  d.characters.routes = {{"walk", "walker", {"end"}, {}}};
  HouseholdRun run(d);
  run.batch();
  run.household.openDocument(0);
  const auto actor = run.physics.actorState(0);
  const auto box = run.physics.boxState(0);
  const auto serial = run.audio.instance("receiver-loop");
  for (int step = 0; step < 60; ++step) {
    run.batch({}, 1);
    run.audio.update((step + 1) * double(dt));
  }
  EXPECT_TRUE(run.household.readingDocument());
  EXPECT_GT(run.physics.actorState(0).feet_position.z,
            actor.feet_position.z + .5F);
  EXPECT_LT(run.physics.boxState(0).center.y, box.center.y - 1.F);
  EXPECT_GT(run.audio.offset("receiver-loop"), .99);
  EXPECT_EQ(run.audio.instance("receiver-loop"), serial);
  const auto suspended_actor = run.physics.actorState(0);
  const auto suspended_box = run.physics.boxState(0);
  const auto offset = run.audio.offset("receiver-loop");
  run.audio.suspend(true, 60 * double(dt));
  run.batch({}, 100, true);
  run.audio.update(1000. + 60 * double(dt));
  EXPECT_EQ(run.physics.actorState(0).feet_position,
            suspended_actor.feet_position);
  samePose(suspended_box, run.physics.boxState(0));
  EXPECT_NEAR(run.audio.offset("receiver-loop"), offset, .000001);
  const CaptionFont font("resources");
  EXPECT_NO_THROW(
      (void)font.layout(run.audio.captions(), 800, 600, run.household.text()));
}

TEST(HouseholdRuntime, CarryingKeepsFlashlightAndExcludesTargetedWorldActions) {
  auto d = scene(true);
  DoorDefinition door;
  door.id = "carry-door";
  door.hinge_position = {-.5F, .02F, -1.5F};
  door.width = 1;
  door.lock_side = DoorLockSide::PositiveZ;
  d.doors = {door};
  HouseholdRun run(d);
  // Aim beside the parcel so the same real leaf remains reachable after drop.
  const PlayerViewPose door_view{{0, 1.65F, 0}, {.3F, 0, -1}};
  const auto target =
      selectAuthoredTarget(door_view, run.level, run.physics, run.doors);
  ASSERT_TRUE(target);
  ASSERT_EQ(target->kind, AuthoredTargetKind::Door);
  run.pickup();
  run.batch();
  PlayerActionSnapshot primary;
  primary.primary_action = true;
  const auto before = run.flashlight.enabled();
  run.batch(primary);
  EXPECT_NE(run.flashlight.enabled(), before);
  primary.lock = true;
  run.batch(primary, 0, false, door_view);
  EXPECT_EQ(run.household.result(), HouseholdResult::Refused);
  EXPECT_TRUE(run.household.heldBox());
  EXPECT_FALSE(run.doors.state(0).locked);
  EXPECT_FALSE(run.doors.state(0).moving);
  EXPECT_FLOAT_EQ(run.doors.state(0).angle, 0);
  run.household.openDocument(0);
  run.household.toggleRadio(0);
  EXPECT_FALSE(run.household.readingDocument());
  EXPECT_TRUE(run.household.radioOn(0));
  EXPECT_FALSE(run.household.commandPending());

  run.batch();
  PlayerActionSnapshot interact;
  interact.interact = true;
  run.batch(interact, 0, false, door_view);
  ASSERT_TRUE(run.household.commandPending());
  EXPECT_TRUE(run.household.heldBox());
  EXPECT_FALSE(run.doors.state(0).locked);
  EXPECT_FALSE(run.doors.state(0).moving);
  EXPECT_FLOAT_EQ(run.doors.state(0).angle, 0);
  run.household.beforeFixedStep(run.player.viewPose(1));
  ASSERT_FALSE(run.household.heldBox());
  EXPECT_EQ(run.household.result(), HouseholdResult::Dropped);

  run.batch();
  PlayerActionSnapshot lock;
  lock.lock = true;
  run.batch(lock, 0, false, door_view);
  EXPECT_TRUE(run.doors.state(0).locked);
  EXPECT_EQ(run.doors.state(0).feedback, DoorResultKind::Locked);
  run.batch();
  run.batch(lock, 0, false, door_view);
  EXPECT_FALSE(run.doors.state(0).locked);
  EXPECT_EQ(run.doors.state(0).feedback, DoorResultKind::Unlocked);
  run.batch();
  run.batch(interact, 0, false, door_view);
  EXPECT_TRUE(run.doors.state(0).moving);
  EXPECT_TRUE(run.doors.state(0).target_open);
  EXPECT_EQ(run.doors.state(0).feedback, DoorResultKind::Opening);
}

TEST(HouseholdRuntime, RadioOnOffRestartUsesOneOwnedSourceAndCurrentCaptions) {
  const CaptionFont font("resources");
  for (const bool muted : {false, true}) {
    HouseholdRun run(scene(true));
    run.audio.mute(muted);
    run.audio.listener({2, 0, 0}, {0, 0, -1});
    run.batch();
    ASSERT_TRUE(run.household.radioOn(0));
    const auto first = run.audio.instance("receiver-loop");
    ASSERT_NE(first, 0U);
    if (muted)
      EXPECT_EQ(run.audio.effectiveGain("receiver-loop"), 0);
    else
      EXPECT_GT(run.audio.effectiveGain("receiver-loop"), .99F);
    EXPECT_EQ(run.audio.definitions().sources[0].position,
              (WorldPosition{7, 1, 7}));
    EXPECT_FALSE(run.audio.captions().ambience.text.empty());
    run.household.toggleRadio(0);
    EXPECT_FALSE(run.household.radioOn(0));
    EXPECT_EQ(run.audio.status("receiver-loop"), CueStatus::Cancelled);
    EXPECT_TRUE(run.audio.captions().ambience.text.empty());
    EXPECT_EQ(run.household.text().feedback, "Радио выключено");
    EXPECT_EQ(run.household.text().hint, "E — включить радио");
    run.household.toggleRadio(0);
    EXPECT_TRUE(run.household.radioOn(0));
    const auto second = run.audio.instance("receiver-loop");
    EXPECT_GT(second, first);
    for (int i = 0; i < 8; ++i) run.batch();
    EXPECT_EQ(run.audio.instance("receiver-loop"), second);
    EXPECT_EQ(run.level.household().radios[0].initially_on, true);
    EXPECT_NO_THROW((void)font.layout(run.audio.captions(), 800, 600,
                                      run.household.text()));
  }
}

TEST(HouseholdRuntime, ActualPhysicalPoseIsTheOnlyDisplayedBox) {
  HouseholdRun run;
  run.pickup();
  PlayerActionSnapshot move;
  for (int i = 0; i < 180; ++i) {
    move.look_delta_x = i < 90 ? .6 : -.4;
    run.batch(move, 1);
    const auto state = run.physics.boxState(0);
    const auto geometry = run.household.presentation(run.doors.presentation());
    ASSERT_EQ(geometry.size(), 2U);
    EXPECT_EQ(geometry[0].center,
              (std::array{state.center.x, state.center.y, state.center.z}));
    EXPECT_EQ(geometry[0].orientation, state.orientation);
  }
  EXPECT_EQ(run.physics.boxCount(), 1U);
}

TEST(HouseholdTarget, FullOrientationAndInsideOriginsUseActualBounds) {
  const OpaqueBoxFrame box{{0, 0, 0}, {.15F, .15F, .15F}, {.5F, .5F, .5F, .5F}};
  EXPECT_TRUE(householdPointInside(box, {0, 0, 0}));
  EXPECT_FALSE(householdRayDistance(box, {0, 0, 0}, {1, 0, 0}));
  ASSERT_TRUE(householdRayDistance(box, {0, 0, -1}, {0, 0, 2}));
  EXPECT_NEAR(*householdRayDistance(box, {0, 0, -1}, {0, 0, 2}), .85F, .00001F);
  auto tilted = box;
  const float half_angle = float(std::acos(-1.) / 8);
  tilted.orientation = {std::sin(half_angle), 0, 0, std::cos(half_angle)};
  EXPECT_NEAR(*householdRayDistance(tilted, {0, 0, -1}, {0, 0, 1}),
              1 - .15F * std::sqrt(2.F), .00001F);
  EXPECT_FALSE(householdRayDistance(tilted, {.3F, 0, -1}, {0, 0, 1}));
}

TEST(HouseholdTarget, DocumentPriorityAndIdsDoNotFallThroughToBox) {
  for (bool reversed : {false, true}) {
    auto d = scene();
    d.household.documents = {
        {"z-note", {0, 1.65F, -.7F}, 0, "Записка", {"Текст"}},
        {"a-note", {0, 1.65F, -.7F}, 0, "Записка", {"Текст"}}};
    if (reversed)
      std::reverse(d.household.documents.begin(), d.household.documents.end());
    HouseholdRun run(d);
    const auto selected = selectAuthoredTarget(
        run.player.viewPose(1), run.level, run.physics, run.doors);
    ASSERT_TRUE(selected);
    EXPECT_EQ(selected->kind, AuthoredTargetKind::Document);
    EXPECT_EQ(run.level.household().documents[selected->index].id, "a-note");
    run.batch();
    PlayerActionSnapshot right;
    right.secondary_action = true;
    run.batch(right);
    EXPECT_FALSE(run.household.commandPending());
    EXPECT_FALSE(run.household.readingDocument());
    right = {};
    right.interact = true;
    run.batch(right);
    EXPECT_TRUE(run.household.readingDocument());
    EXPECT_FALSE(run.household.commandPending());
  }
}

TEST(HouseholdTarget, BoxOwnFrontIsExcludedWhileOtherEqualBlockersRemain) {
  for (bool blocker : {false, true}) {
    auto d = scene();
    if (blocker)
      d.solids.push_back({{0, 1.65F, -.8F},
                          {.5F, .4F, .05F},
                          {255, 255, 255, 255},
                          PrototypeSolidKind::Obstacle});
    HouseholdRun run(d);
    const auto target = selectAuthoredTarget(run.player.viewPose(1), run.level,
                                             run.physics, run.doors);
    EXPECT_EQ(target.has_value(), !blocker);
    if (target) EXPECT_EQ(target->kind, AuthoredTargetKind::Box);
  }
}

TEST(HouseholdRuntime,
     FixedBoundaryLookSequenceIsIndependentOfPresentationBatches) {
  std::vector<PhysicsBoxState> reference;
  for (int batches : {1, 4}) {
    HouseholdRun run;
    run.pickup();
    for (int step = 0; step < 180; ++step) {
      PlayerActionSnapshot input;
      input.look_delta_x = step < 90 ? .8 : -.6;
      run.batch(input);  // one accepted changing-look sample per boundary
      for (int batch = 1; batch < batches; ++batch) {
        run.batch();
        (void)run.household.presentation(run.doors.presentation());
      }
      run.household.beforeFixedStep(run.player.viewPose(1));
      run.physics.advanceWorld(dt);
      run.player.fixedStep(dt);
      const auto state = run.physics.boxState(0);
      ASSERT_EQ(run.household.heldBox(), 0U);
      if (batches == 1)
        reference.push_back(state);
      else {
        EXPECT_NEAR(state.center.x, reference[step].center.x, .001F);
        EXPECT_NEAR(state.center.y, reference[step].center.y, .001F);
        EXPECT_NEAR(state.center.z, reference[step].center.z, .001F);
        double dot = 0, aa = 0, bb = 0;
        for (int i = 0; i < 4; ++i) {
          dot += double(state.orientation[i]) * reference[step].orientation[i];
          aa += double(state.orientation[i]) * state.orientation[i];
          bb += double(reference[step].orientation[i]) *
                reference[step].orientation[i];
        }
        const double angle =
            2 *
            std::acos(std::clamp(std::abs(dot) / std::sqrt(aa * bb), 0., 1.));
        EXPECT_LE(angle * 180 / std::acos(-1.), .1);
      }
    }
  }
}

TEST(HouseholdFixture,
     OrdinaryDefinitionsPrepareAndRunWithoutFilenameDispatch) {
  const CaptionFont font("resources");
  for (const auto& name :
       {"household-interactions", "household-baseline", "household-capacity"}) {
    SCOPED_TRACE(name);
    const auto loaded =
        loadLevelDocument(std::filesystem::path("resources/levels") /
                          (std::string(name) + ".level.json"));
    ASSERT_TRUE(loaded);
    EXPECT_NO_THROW(validateHouseholdText(loaded.document->household, font));
    const auto assets = prepareSceneAssets("resources", *loaded.document);
    EXPECT_TRUE(assets.obstacle_material);
    HouseholdRun run(*loaded.document);
    EXPECT_EQ(run.physics.boxCount(), loaded.document->household.boxes.size());
    EXPECT_EQ(run.physics.actorCount(), 1U);
    EXPECT_EQ(run.physics.doorCount(), 1U);
    EXPECT_EQ(run.household.radioOn(0), false);
    const auto authored = run.level.household();
    for (int step = 0; step < 180; ++step) {
      run.batch({}, 1);
      EXPECT_EQ(run.physics.boxCount(), authored.boxes.size());
      const auto display = run.household.presentation(run.doors.presentation());
      EXPECT_EQ(display.size(), 6U + authored.boxes.size() + 2U);
    }
    EXPECT_EQ(run.level.household(), authored);
  }
}

TEST(HouseholdRuntime, FreshRunAndLaterAuthoringOwnIndependentState) {
  auto authored = scene(true);
  const auto saved_initial = authored;
  {
    HouseholdRun running(authored);
    running.pickup();
    running.batch({}, 4);
    const auto live = running.physics.boxState(0);

    // The editor retains its own document while Play consumes a saved immutable
    // scene. Later authoring cannot relocate a body or change live radio state.
    authored.household.boxes[0].center = {4, 2, -3};
    authored.household.documents[0].pages[0] = "Edited after Play";
    authored.household.radios[0].initially_on = false;
    samePose(live, running.physics.boxState(0));
    EXPECT_EQ(running.level.household(), saved_initial.household);
    EXPECT_EQ(running.household.heldBox(), 0U);
    EXPECT_TRUE(running.household.radioOn(0));
    ASSERT_TRUE(running.household.requestDrop());
    running.household.beforeFixedStep(running.player.viewPose(1));
    running.household.toggleRadio(0);
    EXPECT_FALSE(running.household.radioOn(0));
    running.household.openDocument(0);
    EXPECT_EQ(running.household.readingDocument(), 0U);
  }

  HouseholdRun fresh(saved_initial);
  const auto reset = fresh.physics.boxState(0);
  EXPECT_EQ(reset.center, saved_initial.household.boxes[0].center);
  EXPECT_EQ(reset.orientation, yawQuaternion(0));
  EXPECT_FLOAT_EQ(reset.linear_velocity.x, 0);
  EXPECT_FLOAT_EQ(reset.linear_velocity.y, 0);
  EXPECT_FLOAT_EQ(reset.linear_velocity.z, 0);
  EXPECT_FLOAT_EQ(reset.angular_velocity.x, 0);
  EXPECT_FLOAT_EQ(reset.angular_velocity.y, 0);
  EXPECT_FLOAT_EQ(reset.angular_velocity.z, 0);
  EXPECT_FALSE(fresh.household.heldBox());
  EXPECT_FALSE(fresh.household.readingDocument());
  EXPECT_TRUE(fresh.household.radioOn(0));
  EXPECT_EQ(fresh.audio.instance("receiver-loop"), 1U);
}
