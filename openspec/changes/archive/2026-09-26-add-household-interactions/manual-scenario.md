# P06/T3 desktop acceptance scenario

Status: historical scenario; further manual execution stopped at the user's
request on 2026-09-11. The authorization from 2026-09-10 no longer authorizes
continuing these steps. Functional verification now uses
`scripts/check_household.ps1`; see [coverage and results](validation.md).
The steps below document the earlier plan and retained observations, not work
to resume. On 2026-09-26 the user accepted P06/T3 using the retained evidence
with subjective feel and physical listening explicitly unverified. That decision
does not mark the unperformed steps below as passed; see `validation.md`.

## 1. Neutral interaction scene

Launch `build/debug/bin/near_laugh.exe --level
resources/levels/household-interactions.level.json` from the repository root.
Start at the authored `explore` entry. There are four boxes, a table with a
three-page Russian letter and a radio, a thin wall, a door and a walking actor.

1. Pick up the floor box with E. Look around slowly and briskly; observe actual
   lag and rotation. Move it into the table, 0.02 m wall and accepted door leaf.
   Confirm visible contact, no snap through blockers, and bounded release when
   the target becomes too distant. Capture close color/shadow views.
2. Drop with E, then reacquire and throw with right mouse. Observe gravity,
   rotation and rest; repeat near a wall. Press E and right mouse together and
   confirm drop. Confirm flashlight remains independent while carrying, and
   that reading/radio/door/lock actions require empty hands.
3. Observe the actor stop at its route box, clear the box and observe resumed
   motion. Obstruct the door with a box, clear it and confirm a new press is
   required. Observe player contacts and inability to stand/jump from boxes.
4. With empty hands read the letter. Check Russian/Ё glyphs, A/D limits,
   E/Escape close, retained crouch, no player movement/look and continued
   box/actor/audio motion. Hold movement while closing; release before moving.
5. Toggle the radio; verify hint, indicator and caption agree in the same
   frame. Toggle off and restart, then repeat muted with M. Record visual
   caption behavior separately from audible output; do not infer listening.
6. Pause/resume with P while holding and while reading. Minimize/restore and
   resize with a page open. Check no catch-up or repeated page/escape action.
   Release cursor while holding, minimize and recapture; verify safety drop.
7. Restart the ordinary executable and confirm authored initial state. Retain
   pass/fail observations and solicit human hold/throw feel and audio acceptance.

## 2. Independent authoring through the UI

Launch `build/debug/bin/level_editor.exe`, use **New interior**, and work only
through the editor interface. Do not open the generated household fixture as
the starting point or write JSON/scripts to substitute for authoring.

1. Add and place two boxes on clear floor surfaces, separated from the entry.
   Edit one yaw, duplicate/delete and undo/redo; confirm stable selection.
2. Add a document with Russian title `Вторая комната` and two distinct pages
   containing Ё/ё and Latin. Commit edits, inspect the readable preview, change
   pages and undo/redo. Place the panel where the player can reach its surface.
3. Add an `apartment-radio` prop with no collision proxies, a captioned spatial
   looping ambience cue (`radio` clip and caption), a non-autoplay source, and
   a radio control referencing that prop/source. Choose initial off. Rename a
   linked prop/source, test reference repair and undo; clear all diagnostics.
4. Save As a new file in the run directory, close/reopen it, and inspect all
   authored values. Use ordinary **Play**, then demonstrate pickup/drop/throw,
   the two readable pages and radio on/off. Retain the actual UI-authored file.
5. Return to the editor, edit an initial box pose and use Save-and-Play for a
   fresh run. Report saved-file preflight errors or unavailable native dialogs
   distinctly; do not replace this workflow with scripted scene generation.

## 3. GPU and measurement order

After source builds and headless checks, run the full `vulkan-smoke` preset
sequentially and inspect retained household readbacks. Then run Debug and
Release `measure_household.ps1 -Check`, followed by the three paired Release
measurement runs with no concurrent build/test/game/GPU work. Fullscreen
1920x1080/60 Hz is required. Preserve failures and raw evidence; passing code
checks, screenshot automation or timing gates alone does not establish T3.
