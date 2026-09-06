#include <gtest/gtest.h>

#include "core/input/player_input.hpp"
#include "core/platform/input.hpp"

TEST(InputAccumulator, TracksEngineOwnedKeysAndButtons) {
  InputAccumulator input;
  input.setKey(PhysicalKey::W, true);
  input.setMouseButton(PhysicalMouseButton::Left, true);
  EXPECT_TRUE(input.snapshot().isKeyDown(PhysicalKey::W));
  EXPECT_TRUE(input.snapshot().isMouseButtonDown(PhysicalMouseButton::Left));
  input.setKey(PhysicalKey::W, false);
  input.setMouseButton(PhysicalMouseButton::Left, false);
  EXPECT_FALSE(input.snapshot().isKeyDown(PhysicalKey::W));
  EXPECT_FALSE(input.snapshot().isMouseButtonDown(PhysicalMouseButton::Left));
}

TEST(PlayerInputMapper, MapsEveryRequiredDefaultControl) {
  InputAccumulator input;
  for (const PhysicalKey key :
       {PhysicalKey::W, PhysicalKey::A, PhysicalKey::S, PhysicalKey::D,
        PhysicalKey::Space, PhysicalKey::LeftShift, PhysicalKey::LeftControl,
        PhysicalKey::Escape, PhysicalKey::E}) {
    input.setKey(key, true);
  }
  input.setMouseButton(PhysicalMouseButton::Left, true);
  input.setMouseButton(PhysicalMouseButton::Right, true);
  input.addCursorPosition(10.0, 20.0);
  input.addCursorPosition(13.0, 24.0);

  const PlayerActionSnapshot actions =
      PlayerInputMapper{}.map(input.snapshot());
  EXPECT_TRUE(actions.move_forward);
  EXPECT_TRUE(actions.move_backward);
  EXPECT_TRUE(actions.move_left);
  EXPECT_TRUE(actions.move_right);
  EXPECT_TRUE(actions.jump);
  EXPECT_TRUE(actions.sprint);
  EXPECT_TRUE(actions.crouch);
  EXPECT_TRUE(actions.menu);
  EXPECT_TRUE(actions.interact);
  EXPECT_TRUE(actions.primary_action);
  EXPECT_TRUE(actions.secondary_action);
  EXPECT_DOUBLE_EQ(actions.look_delta_x, 3.0);
  EXPECT_DOUBLE_EQ(actions.look_delta_y, 4.0);
}

TEST(PlayerInputMapper, NewBatchClearsLookAndPreservesHeldActions) {
  InputAccumulator input;
  input.setKey(PhysicalKey::W, true);
  input.setMouseButton(PhysicalMouseButton::Left, true);
  input.addCursorPosition(1.0, 1.0);
  input.addCursorPosition(2.0, 3.0);
  input.beginEventBatch();

  const PlayerActionSnapshot actions =
      PlayerInputMapper{}.map(input.snapshot());
  EXPECT_TRUE(actions.move_forward);
  EXPECT_TRUE(actions.primary_action);
  EXPECT_DOUBLE_EQ(actions.look_delta_x, 0.0);
  EXPECT_DOUBLE_EQ(actions.look_delta_y, 0.0);
}

TEST(InputAccumulator, ResetsCursorDeltaPerEventBatch) {
  InputAccumulator input;
  input.addCursorPosition(10.0, 20.0);
  input.addCursorPosition(13.5, 18.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 3.5);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, -2.0);

  input.beginEventBatch();
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 0.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, 0.0);
  input.addCursorPosition(14.0, 19.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 0.5);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, 1.0);
}

TEST(InputAccumulator, WaitedBatchIsSampledBeforeTheNextPollBatch) {
  InputAccumulator input;
  PlayerInputMapper mapper;
  input.setKey(PhysicalKey::W, true);
  input.addCursorPosition(10.0, 20.0);

  input.beginEventBatch();
  input.addCursorPosition(13.0, 18.0);
  const PlayerActionSnapshot waited_actions = mapper.map(input.snapshot());
  EXPECT_TRUE(waited_actions.move_forward);
  EXPECT_DOUBLE_EQ(waited_actions.look_delta_x, 3.0);
  EXPECT_DOUBLE_EQ(waited_actions.look_delta_y, -2.0);

  input.beginEventBatch();
  const PlayerActionSnapshot next_poll_actions = mapper.map(input.snapshot());
  EXPECT_TRUE(next_poll_actions.move_forward);
  EXPECT_DOUBLE_EQ(next_poll_actions.look_delta_x, 0.0);
  EXPECT_DOUBLE_EQ(next_poll_actions.look_delta_y, 0.0);
}

TEST(InputAccumulator, CaptureResetDiscardsCursorJump) {
  InputAccumulator input;
  input.addCursorPosition(1.0, 2.0);
  input.resetCursorTracking();
  input.addCursorPosition(100.0, 200.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 0.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, 0.0);
}

TEST(PlayerInputMapper, InteractionIsIndependentOfMouseActions) {
  InputAccumulator input;
  input.setKey(PhysicalKey::E, true);
  const auto actions = PlayerInputMapper{}.map(input.snapshot());
  EXPECT_TRUE(actions.interact);
  EXPECT_FALSE(actions.primary_action);
  EXPECT_FALSE(actions.secondary_action);
  input.setKey(PhysicalKey::E, false);
  input.setMouseButton(PhysicalMouseButton::Left, true);
  EXPECT_FALSE(PlayerInputMapper{}.map(input.snapshot()).interact);
  EXPECT_TRUE(PlayerInputMapper{}.map(input.snapshot()).primary_action);
}
