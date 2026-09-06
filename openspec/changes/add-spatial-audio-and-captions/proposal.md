## Why

The player understands the danger through familiar sounds becoming wrong:
the radio, unanswered kettle, footsteps, and telephone conversation. The
runtime currently has neither audio nor game text, so this central experience
cannot be authored or evaluated.

## What Changes

- Add explicitly owned audio playback for packaged dialogue, localized
  one-shots, moving sources, and ambient loops. Define listener updates,
  start/stop behavior, gain control, device failure reporting, and teardown.
- Author the small amount of room/connection information needed for this
  apartment. Door state and room separation affect sound transmission so a
  voice behind the room door differs from a voice in the same room.
- Provide readable Russian game text, packaged font resources, subtitle
  placement, and essential sound captions with speaker/source wording limited
  to what the player can know. Essential clues remain understandable muted.
- Coordinate cue timing, overlap priority, and interruption. Pause/minimize
  behavior must preserve the relationship between audible content and text;
  presentation recovery must not replay a completed one-shot.
- Add sound placement, clip/caption references, room/door links, validation,
  undo/redo, and an explicit editor audition workflow with separate ownership.
- Keep this a concrete audio integration; select the dependency and supported
  formats during design. Do not add a middleware abstraction, acoustic
  simulation framework, procedural audio graph, or dialogue-choice editor.
- Any required serialized additions use an explicit version transition with
  the compatibility policy of the then-current level format.

## Capabilities

### New Capabilities

- `spatial-audio`: Owned playback, authored sources, bounded room/door
  transmission, cue lifetime, and audition.
- `game-text-presentation`: Russian text, dialogue subtitles, essential sound
  captions, readable layout, and cue synchronization.

### Modified Capabilities

- `level-persistence`: Persist sound/room definitions and validated references.
- `level-object-placement`: Place and edit audio sources and room connections.
- `level-editor`: Inspect caption/link errors and explicitly audition content.
- `runtime-composition`: Own audio/text presentation and coordinate its
  lifecycle, listener, timing, and minimized-window behavior.
- `vulkan-renderer`: Present game text while preserving scene resource and
  presentation-recovery guarantees.

## Impact

Introduces an audio dependency behind a meaningful playback/lifetime boundary
and packaged audio/font resources. Affects runtime timing, world validation,
game text rendering, editor audition, and packaging rules. Document required
formats, authored cue behavior, captions, and audio validation.

## Dependencies and Boundaries

P04; requires [P03](../archive/2026-09-06-add-interactive-doors/proposal.md).
P03 supplies authoritative door states. P05 later decides story sequencing;
this change validates sound/text behavior through a bounded cue fixture.
Full session menus/settings belong to P12.

## Acceptance Criteria

- Hear a radio, footsteps, and a telephone from distinct authored positions;
  closing/opening a connecting door changes transmission as authored.
- A fixture plays the completed phone conversation followed by the misleading
  invitation. Its captions preserve both the ordering and the contradiction.
- Essential captions remain legible at supported window sizes without naming
  an unrevealed threat; overlapping ambience does not hide the key clue.
- Missing clip/font and invalid room/door references give actionable errors.
  Pause/minimize, resume, and shutdown leave no stale voices or repeated cues.
- Run timing/layout/reference/lifetime tests and relevant Vulkan smoke;
  manually listen and complete the cue fixture with audio muted.
