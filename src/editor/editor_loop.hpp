#ifndef EDITOR_EDITOR_LOOP_HPP
#define EDITOR_EDITOR_LOOP_HPP

#include "core/frame.hpp"

enum class EditorLoopAction { Exit, WaitForEvents, Render };

[[nodiscard]] constexpr EditorLoopAction decideEditorLoopAction(
    bool exit_requested, FramebufferExtent framebuffer) noexcept {
  if (exit_requested) {
    return EditorLoopAction::Exit;
  }
  if (framebuffer.isZero()) {
    return EditorLoopAction::WaitForEvents;
  }
  return EditorLoopAction::Render;
}

[[nodiscard]] constexpr bool editorContinuesAfter(
    FrameOutcome outcome) noexcept {
  switch (outcome) {
    case FrameOutcome::Rendered:
    case FrameOutcome::Skipped:
    case FrameOutcome::Recovered:
      return true;
  }
  return false;
}

#endif
