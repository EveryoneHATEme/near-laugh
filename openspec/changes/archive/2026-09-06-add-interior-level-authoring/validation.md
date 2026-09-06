# P01 validation record

Date: 2026-09-06. Host: Windows, Clang 23.1.0, Debug preset.

## Automated evidence

- `cmake --preset debug` and `cmake --build --preset debug` passed.
- `ctest --preset debug --output-on-failure` passed **224/224** checks:
  216 unit, six boundary, and two process checks (33.15 seconds).
- `ctest --preset vulkan-smoke --output-on-failure` passed all seven cases:
  retained prototype, both apartment entries, deliberate error injection,
  lifecycle failures, editor replacement/empty geometry, and editor startup
  failure. Vulkan validation was enabled on **AMD Radeon(TM) Graphics**.
  The only error-severity message was the expected injected error. An external
  `VK_LAYER_OW_OVERLAY` layer emitted an API-version warning. The smoke log is
  retained locally at `build/p01-vulkan-validation.log`. The final run took
  19.19 seconds and followed the desktop-discovered UI correction.
- Actual Jolt traversal passed from both entries through Lena's room,
  corridor, kitchen, stairs, and lower landing, with both stair directions,
  ordinary walking, and standing stance. Upper/lower settling, wall blocking,
  slope contact, terrain intrusion, rotated proxy clearance, and construction
  failure recovery are covered by deterministic tests.
- Real ImGui tests exercised New Interior, terrain-tool reset, dirty Play
  cancellation, Save As, a single consumed launch request, and matching
  upper-floor preview/commit. Existing capture, numeric-drag, history, and
  switch UI checks pass.
- A real native child received paths containing spaces, Cyrillic, and
  host-valid shell-significant characters literally. Tests observed zero and
  nonzero exits, duplicate-launch refusal, missing/non-executable failures,
  and child completion after the process owner was destroyed.
- Version-2/3 compatibility uses the retained actual v3 schema, removing the
  switch field for v2. Opening remains clean and read-only; explicit saves
  normalize to v4. The packaged prototype retains its authored values.
- `openspec validate add-interior-level-authoring --strict` passed.

## Desktop acceptance

Performed through visible Windows editor/game windows using mouse and keyboard
input. The editor and default game were launched with
`D:\programming\near-laugh\build` as their working directory.

| Exercise | Observed outcome | Local evidence under `build/` |
| --- | --- | --- |
| New Interior and structural authoring | Unsaved, dirty interior with no terrain; duplicated the floor to a top height of 3 m and authored a wall. | `p01-two-floor-entry.png` |
| Entries on both floors | `default` at Y=0 and `entry-1` at Y=3 remained valid; the upper-floor surface candidate reported Y=3 and clicking committed that height. | `p01-upper-placement-preview.png`, `p01-manual.level.json` |
| Invalid unselected entry | Setting `entry-1` to unsupported Y=4 blocked Play while `default` was selected and identified the affected entry. Repair restored validity. | `p01-invalid-unselected.png` |
| Wall-mounted switch | Preview identified the +Z wall at Y=1.394; click placed the plate at Z=-3.829, with its back 1 mm outside the wall at Z=-3.85. | `p01-switch-preview.png`, `p01-dirty-play.png` |
| Dirty Play cancellation | Cancel closed the decision without saving or starting a game; the interior stayed dirty. | `p01-cancel-dirty.png` |
| Save As, reopen, chosen start | Save and Play wrote the separate authored file and opened a game on its upper floor. Reopening the file restored a clean, valid two-floor interior and the wall switch. | `p01-manual-upper-start.png`, `p01-reopened.png` |
| Independent child lifetime | Closing the editor left its game running; the game then closed normally. | `p01-child-after-editor-close.png` |
| Apartment route | Started in Lena's room, walked through the corridor and kitchen, descended the complete stairs to the landing, ascended again, and returned to Lena's room. | `p01-apartment-start.png`, `p01-kitchen.png`, `p01-descended-landing.png`, `p01-room-chair-return.png` |
| Lower-landing route | Started on the lower landing, ascended to the kitchen and Lena's room, then returned down the complete stairs to the landing. Both runs used ordinary walking without jump or crouch. | `p01-lower-start.png`, `p01-lower-to-kitchen.png`, `p01-lower-to-lena.png`, `p01-lower-round-trip.png` |
| Process exit and editor responsiveness | The editor remained usable and reported game exit code 0 with the selected file and `lower-landing` ID. | `p01-editor-child-exit.png` |
| Retained default invocation | Launching the game without options from the separate working directory displayed the packaged terrain prototype. Source and packaged prototype hashes stayed identical before and after all authoring and play runs. | `p01-retained-prototype.png` |

Prototype SHA-256 before and after:
`032C98DD3A20F78DC24427F24BE27D1B39CB87D1990769A17EE1DB094A2E5512`.
The images and scratch authored file are local acceptance artifacts in the
ignored build directory, not additional packaged game content.

Desktop inspection exposed a newly added switch taking the previous selection's
initial floor offset. The UI now refreshes the selected value before initializing
placement settings. A real ImGui regression places a newly added switch on a
floor and checks the required 1.4 m height. That test and the final full suites
passed after the correction. Task 10.3 has no remaining Windows desktop checks.

## Scope and limits

The scene is temporary structural content for M1. Later roadmap milestones,
doors, narrative progression, audio, checkpoints, and expanded lighting remain
outside P01. The native POSIX branch has not been compiled or exercised on
Linux/macOS in this Windows session. No readiness protocol or protection
against an external writer racing between preflight and child load is claimed.
