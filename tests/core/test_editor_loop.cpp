#include <gtest/gtest.h>

#include "editor/editor_loop.hpp"

TEST(EditorLoop, CoversNormalCloseZeroExtentAndRestorePaths) {
  EXPECT_EQ(decideEditorLoopAction(false, {1600, 900}),
            EditorLoopAction::Render);
  EXPECT_EQ(decideEditorLoopAction(true, {1600, 900}), EditorLoopAction::Exit);
  EXPECT_EQ(decideEditorLoopAction(false, {0, 0}),
            EditorLoopAction::WaitForEvents);
  EXPECT_EQ(decideEditorLoopAction(false, {1600, 900}),
            EditorLoopAction::Render);
}

TEST(EditorLoop, ExplicitlyConsumesEveryRendererOutcome) {
  EXPECT_TRUE(editorContinuesAfter(FrameOutcome::Rendered));
  EXPECT_TRUE(editorContinuesAfter(FrameOutcome::Skipped));
  EXPECT_TRUE(editorContinuesAfter(FrameOutcome::Recovered));
}
