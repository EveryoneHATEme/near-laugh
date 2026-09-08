# Approved slope placement clarification (2026-09-08)

Status: approved by the user with "yes, proceed". The decision is incorporated
in the proposal/design/specs and task 2.3. Shared validation and runtime collision
offsets are implemented; uphill/downhill/flat-transition and overhead regressions
pass, and task 2.3 is checked. Do not request this approval again. The diagnosis
and original revision plan below are retained as historical context.

## Reproduced mismatch

The shared validator accepts actor marks on terrain up to the existing
50-degree walking limit. Marks lie at the terrain height, and the runtime
requirement measures accepted feet arrival within 0.02 m of that position.
The catalog gives an upright capsule of radius 0.25 m and height 1.85 m,
with its bottom at the same feet anchor.

On a plane inclined by angle theta, a nonpenetrating upright capsule's bottom
is r * (1 / cos(theta) - 1) above the surface directly beneath its center.
The offset is approximately 0.1311 m at 49 degrees and 0.1389 m at 50 degrees.
Treating that capsule bottom as the visible/authored foot anchor cannot also
satisfy the existing 0.02 m arrival criterion.

`ActorPhysics.SupportedTerrainSlopePreservesAuthoredFeetArrivalTolerance`
creates validated 49-degree terrain and requests one metre of horizontal
travel. It reaches Z=1 but settles at Y=1.281426 instead of the mark's
Y=1.150368. The 0.131058 m error fails the arrival assertion. Raw results are
in `build/scripted-characters-actor-slope.log`; the earlier initial-contact
failure remains in `build/scripted-characters-actor-collision.log`.

Shared placement validation already allows bounded bottom-cap separation on
terrain (`spawnClear` in `prototype_level.cpp`); the runtime movement contract
has not defined how that separation relates to authored feet.

## Approved revision (implemented)

Keep the authored mark, visible model origin and source placement anchored to
the accepted ground surface. Represent the collision capsule placement with a
separate, support-derived vertical offset inside physics:

- Keep the catalog radius, height, supported slope range and 0.02 m route
  arrival tolerance.
- Derive the offset from accepted structural/terrain support, with zero offset
  on flat ground. Collision clearance and continuous sweeps use the resulting
  actual capsule pose, including changes in the offset.
- Apply the same rule during initial placement validation, including overhead,
  player-entry, actor and door clearance. Reject invalid placement rather than
  lifting through a ceiling or snapping across a blocker.
- Publish ground feet and yaw for route decisions, rendering and source motion.
  Preserve the existing model/foot and proxy/limb limitations; this adds no
  inverse kinematics or animation clips.

Design decision 3, placement/arrival requirements, physics requirements, shared
validation and task 2.3 now use this distinction. Regressions cover initialization,
uphill/downhill movement, flat transitions, ceilings, supported arrival and swept
participant clearance. These bounded cases do not prove every possible terrain
configuration traversable.

An alternative is to restrict actor terrain slopes or permit a larger vertical
arrival error and floating visible feet. Those change the specified player-slope
support or presentation behavior; neither has been assumed or implemented.
