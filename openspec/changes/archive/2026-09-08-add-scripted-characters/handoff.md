# P07b to P07c handoff

Status: P07b accepted on 2026-09-08 with implementation, automated verification
and all nine passing Release samples retained. The user explicitly approved
finalizing tasks 5.3 and 5.7 with physical listening and hardware latency
unverified, as recorded in validation.md. All 25 tasks are checked. This
handoff was archived on 2026-09-08, then all seven delta specs were synced and
verified against main specs in the order requested by the user. T2 still
requires P07c authoring acceptance.

## Delivered contracts

- `LevelCharacters` contains up to four actors, 32 marks and 16 routes. Marks
  own the only authored feet/yaw placement. Routes belong to one actor, reference
  1–32 ordered marks and support null or `interact` final clips. Initial route,
  footstep and interaction links are nullable; source links are exclusive.
- The canonical file is v9. Exact v2–v8 inputs preserve prior fields, normalize
  empty characters and never rewrite on Open. Save As retains an original for
  older executables; removing actors from v9 does not downgrade it.
- Shared validation checks references, catalog limits, support/static clearance
  and initial actor/player-entry/door overlap. Endpoint marks do not certify
  route traversability. Ground feet remain the visible/audio anchor on slopes;
  a private support-derived offset positions the unchanged upright capsule.
- `prepareCharacterAssets` shares selected immutable assets and supplies the
  authored actor order. Zero actors need no mannequin. `CharacterPlayback::drive`
  separates calibrated clip time from blend elapsed time without losing an
  interrupted transition source. Render poses borrow joint palettes and use
  current accepted world placement.
- `CharacterController` exposes actor-owned start/cancel and durable action,
  route/mark, obstruction, contention, distance and contact results. Active same
  starts are idempotent; another route is busy; explicit restart uses current
  placement. Initial routes start once per new runtime, not by filename.
- The fixed-step owner advances world, player, actors in durable-ID order and
  doors, then hands accepted source positions/contact events to the existing
  cue coordinator. Actors wait automatically; obstructed doors need another
  interaction. Capsule tops are not player support or interaction targets.
- Footsteps use 0.12-second mono effects and calibrated accepted contacts.
  Interact holds at its marker while foreground audio is busy, then owns one
  one-second essential cue/caption. Cancellation cannot steal unrelated audio.
  Device loss/mute preserve logical action and caption state.
- The editor retains all character fields across unrelated edits/history/save,
  displays frozen initial idle poses, and prepares selected character/audio
  assets before child creation. Failed replacement keeps coherent prior GPU
  resources and palette storage. Invalid references remain repairable and have
  diagnostic anchors. Dedicated character list/property/placement controls are
  not included in this stage.

## P07c dependency review

P07c's existing proposal, design and tasks match the delivered v9 and selected
resource boundaries. It can extend the existing document selection/commands,
source rename references, picking/overlays and snapshot preview. It must keep
the editor free of PhysicsWorld/Engine dependencies: schematic route inspection
does not establish physical traversal, which is checked through saved-file Play.

P07c should replace the compatibility-only diagnostic fallback with its planned
record selection and repair UI. It should expose the authored ground anchor and
distinguish visual bounds from the actual offset capsule on slopes. It must stop
preview snapshots on the specified edits, selection/history, minimize and Play
transitions, and coordinate them with existing audio audition.

The next stage needs no new file version, general animation graph, runtime
editor, source-asset conversion or save-game serialization. Its second scene
must be built through editor operations and played independently, with real
obstruction/release, sound/caption and shadow evidence. T2 and dependent P06/P05
readiness remain open until that authoring acceptance and the full P07 chain.

## Evidence and reproduction

See [validation.md](validation.md), [performance.md](performance.md), [evidence](evidence), and
[DEVELOPMENT](../../../../docs/DEVELOPMENT.md#p07b-scripted-characters) for commands,
captures, raw timing samples, failed/interrupted runs and measurement limits.
Both the one-second caption/effect decision and slope correction were approved
before this continuation. Neither needs another approval. The user has also
accepted the documented unavailable listening/hardware-latency evidence.
The archive and subsequent main-spec sync are finished. No commit was made.
