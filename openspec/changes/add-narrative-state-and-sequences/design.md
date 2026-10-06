## Context

See [proposal.md](proposal.md) for the outcome and prerequisites. P06 and the
complete P07 chain are archived; the baseline is level v10 with independent
light, door, household, character and audio owners. `Engine::tick` advances
physics at 1/60 s with a 100 ms contribution cap, hands character audio off
after the fixed-step batch, and samples ordinary interactions once per batch.
Audio uses uncapped active time. Reading and cursor release continue the world;
development suspension and minimization freeze it.

The current APIs primarily expose player toggles. Actor and cue instances are
observable, but cancellation by actor/source alone can stop a replacement
instance. Door obstruction stops motion until another explicit request; actors
instead retry their blocked route. The editor must retain its existing
independence from runtime simulation and physics.

## Goals / Non-Goals

**Goals:** Keep scene definitions immutable, put event state in one run-local
owner, reuse concrete subsystem rules, and make timing/ownership results
testable without a window or device. Complete ordinary authoring and saved-file
Play within this change.

**Non-Goals:** A scripting VM, graph editor, generic event bus, plugin API,
live simulation in the editor, checkpoint serialization, session menus,
prepared-state launches, story content, arbitrary object manipulation or a
refactor of the established physics/audio clocks.

## Decisions

### 1. Concrete definitions in the existing world boundary

Add a required `narrative` object to v11, containing `facts`, `regions` and
`events`. Initial limits are 32 Boolean facts, 32 axis-aligned regions and
64 events, with 1-32 ordered steps per event and at most eight predicates in
each condition list. These are the first supported authoring profile, not
claims about future story needs. All records use the existing durable-ID
grammar; references are typed. Regions use finite center/positive half-extents
and do not create physics bodies or renderable game geometry.

An event has one trigger, an AND-list of start guards, an optional nonempty
AND-list of cancellation predicates, a repeat mode (`once` or `rearm`), and
ordered steps. An absent cancellation list means no automatic cancellation.
There are four trigger kinds: scene entry, region entry, accepted player
interaction, and a false-to-true transition of a nonempty predicate list.
Scene entry is once-only. Empty start guards mean true. There are no expression
strings, nested expressions, jumps, parallel step branches or recursive calls.

Predicate kinds are Boolean fact, player inside/outside region, light enabled,
door endpoint/lock state, radio enabled, box held/free, document currently open,
actor action state, event terminal state, and elapsed active scene time reaching
a finite threshold. Door endpoint means actually reached and stationary,
not merely requested. Terminal-state predicates observe the latest event run;
a repeated run replaces its prior terminal state. Durations are finite,
nonnegative and at most 3600 seconds. A document-open occurrence does not claim
that its text was read or understood.

The finite step vocabulary is `set_fact`, `set_light`, `set_door_open`,
`set_door_locked`, `set_radio`, `play_cue`, `run_route`, `delay`, and `wait_until`.
The first five finish when their command is accepted (including already at the
desired state). `play_cue` waits for its own one-shot to finish; `run_route`
waits for its own route including the final interaction. `wait_until` uses
the same predicates. Radio supplies looping ambience; direct sequence playback
is limited to non-autoplay one-shots not reserved by actors or radios.

Alternative: a generic action registry and expression tree. Rejected because
the selected scenes need a finite vocabulary, and new action kinds should
remain explicit reviewed C++ changes.

### 2. One run-local progression owner, concrete dispatch

Place definitions/validation in `near_laugh_world` and the progression reducer
under `src/core/gameplay` in `near_laugh_runtime`. The reducer consumes plain
observations and command results; it does not poll input, read wall clocks,
hold physics/backend pointers or access files. `Engine` owns it after its
referenced definitions/controllers and supplies concrete command dispatch.
Use simple typed records/variants rather than a new polymorphic backend or
module. Tests can feed observations/results directly without creating Engine.

Per-event state holds a run generation, step, waiting reason, latest terminal
state and optional owned actor/cue instance. Only one run of an event may be
active. Facts, trigger latches, clocks and the bounded trace belong to this
owner; definitions never change during play. Rendering receives resolved
presentation from existing owners, not narrative commands.

Shutdown closes dispatch first, conditionally cancels owned work, then releases
the progression owner before its controllers/audio/definitions. Partial startup
has no callable half-initialized event runtime. Fresh launch restores authored
facts and empty event history; swapchain recovery preserves current state.
P09 will later define safe checkpoint reconstruction, not serialize these
implementation structures by default.

Alternative: callbacks/subscriptions on every controller. Rejected because one
concrete consumer can collect small result records at existing call sites.

### 3. Sample observations explicitly; never manufacture player actions

Accepted interaction occurrences carry a monotonic occurrence number, target
kind/ID, action kind and actual result. Publish door interaction/lock/knock,
switch activation, radio on/off and document-open results only after acceptance.
Publish pickup/drop/throw only at successful physical application, never at
queue insertion. Refusals, misses, holding a key and safety releases are not
accepted player-action triggers. Document navigation is not a new trigger kind.

Record sampled player region membership transitions after each accepted fixed
step, using the player's feet, half-open bounds and initialized occupancy at scene entry. Spawn
inside a region is not an entry crossing; use the scene-entry trigger when
that behavior is intended. Preserve the order of crossings inside a multi-step
batch, even if the player leaves again before rendering. A pass entirely through
a thin region between two samples does not count; there is no swept trigger
collision. Regions have no line-of-sight test. Start guards use the scheduler's current accepted snapshot;
an entry occurrence remains an occurrence even if its region is now outside.

Occurrences are consumed once at the next active scheduler boundary, with
bounded capacity derived from the six-step simulation cap, region limit and
current action owners. Overflow is an explicit diagnostic/failure, never a
silent dropped story action. Suspension discards unconsumed ordinary input
commands under current rules, but preserves already accepted occurrences and
running event state for one-time processing after resume.

Alternative: poll key state or diff only rendered positions. Rejected because
it invents actions after refusals and misses intermediate crossings.

### 4. Deterministic scheduler boundary and repeat policy

After the fixed-step batch and accepted ordinary interaction dispatch, collect
one immutable snapshot including accepted occurrences and current logical cue
status at an injected active-time sample. Before the single character audio
handoff, perform cancellation preflight; then hand off surviving character audio
and dispatch narrative continuations before resolving captions/presentation.
This prevents a canceled route emitting its pending interaction sound first.
The boundary performs:

1. Evaluate cancellation for all live runs and conditionally stop their owned
   work before any new step dispatch. Cancellation dominates continuation and
   a simultaneous start of that same event.
2. For surviving runs, classify existing owned waits from that immutable
   snapshot before any new starts. A matching completed instance is latched as
   success even if another event reuses the actor/source later this boundary;
   a mismatch/cancellation already present in the snapshot fails the wait.
   Then hand off audio only for surviving character actions and identify fresh
   triggers using the snapshot. Start guards and cancellation
   predicates must permit entry. A trigger rejected by guards, received while
   active, or suppressed by cancellation is consumed, not queued for later.
3. In durable event-ID order, start eligible runs and advance existing ones.
   Drain only their finite immediate steps. Query results for newly dispatched
   commands as needed; retained wait outcomes, snapshot conditions and start decisions do not recursively observe
   effects produced by this boundary. Busy commands are attempted at most once
   per event per boundary. Immediate state writes follow the same stable order.
4. Publish new facts/event states for the next snapshot and retain diagnostics.

Initialize condition-trigger latches from the authored initial snapshot: an
initially true conjunction does not fire; scene-entry is the startup trigger.
An event-terminal predicate is false before that event has run.

`once` consumes its one run when accepted, including a later canceled/failed
run. `rearm` requires a new trigger after the previous run becomes terminal;
for condition triggers, false must be observed after termination before a new
true. Region/interaction triggers require a later occurrence. A held condition,
an old occurrence or presentation recovery cannot restart a run. Event effects
may trigger another event only at a later boundary; there is no same-boundary
recursive chain. Simultaneous conflicting state writes are resolved in ID order
and traced; IDs therefore have an intentional arbitration role, array order does
not. Renaming can change this documented tie order.

Alternative: arbitrary numeric priorities or a general dependency scheduler.
Rejected until authored scenes demonstrate a need beyond stable ID ordering.

### 5. Preserve the two existing time domains

Scene elapsed time and delays use uncapped monotonic active time, excluding
explicit suspension/minimized time, with the same pause policy as audio. A
delay deadline begins when the step is reached. Cue completion uses the logical
coordinator; route/door completion uses the actual accepted owner result. No
delay backdates a new cue/route, and no elapsed interval fabricates movement.
Zero-step active frames may start/cancel events and expire waits; physical
requests move only during subsequent fixed steps.

Reading and cursor release keep world/event/audio time running. Pause freezes
all of them, retains the current document page, and consumes inactive inputs
without replay. Closing a scene stops dispatch and cancels owned transient work;
leaving a trigger region alone continues a run unless its cancellation predicate
explicitly requests cancellation. Leaving the application destroys run-local
progress; there is no implicit level transition or checkpoint here.

Determinism means equal initialized definitions, ordered observations/results
and active-time samples yield equal decisions independent of rendering or
collection order. Equal wall time under arbitrary frame stalls is not equal
simulation history: the existing 100 ms cap and batched actor/audio handshake
remain. Tests vary zero/one/multiple-step batches, preserve crossings and compare
causal order/results where observations are equivalent; they also assert the
defined differences for capped stalls instead of demanding identical timestamps.

Alternative: move all audio to fixed time or refactor marker scheduling now.
Rejected because it changes previously accepted timing beyond T4's requirement.

### 6. Explicit commands and narrowly owned cancellation

Add small concrete desired-state methods to light, door and radio owners;
keep player toggle/reach/side/input rules intact. Repeating an enabled-state
request must not toggle or restart audio. Authored door requests use no invented
player position, but still require a lockable stationary closed door for lock
changes, refuse opening while locked and retain collision sweeps.

Door open/close steps accept an endpoint request, not endpoint arrival. Use a
following `wait_until` to wait for the actual endpoint. An obstructed door stays
stopped after clearance; neither the scheduler nor wait step retries it. A later
explicit request can retry. Existing actor blocked states instead keep retrying
the route, as P07 defines. Radio commands use logical household ownership without
requiring the player to have empty hands or stop reading.

Store an actor/source ID plus instance only after a successful new start. Busy
or already-active foreign work is a wait, never transferred ownership. Add
conditional cancel operations accepting the expected instance. A replaced or
externally canceled instance before observed completion makes its wait fail
with a reason; it never adopts the replacement. A completion already classified
from the boundary snapshot survives subsequent same-boundary reuse. Cancellation
stops only matching owned cue/route instances.
All refused commands fail the event, cancel any owned transient work and
prevent later steps. Busy/blocked/unsatisfied-condition waits have no
hidden timeout; cancellation is the explicit escape. Check already-desired
state first. Light/radio set commands either accept/no-op or report invalid
target as failure. Door lock changes on open/moving/unlockable doors and open
requests on locked doors fail rather than waiting for that precondition; authors
must place a wait before the request when desired. Only actor/audio ownership
or foreground contention is a busy-start wait. Route blockage and predicate
waits remain explainable waits under their existing policies.

Set-state effects, accepted door motion targets and radio state persist after
event cancellation; no rollback is inferred. Only `play_cue`/`run_route` retain
cancelable work while waiting. Source reservation separates sequence sources
from actor/radio/autoplay ownership; two sequences may contend for one eligible
source, with stable dispatch order and no preemption. The old P04 fixture must
not share its privately driven sources with narrative events.

Alternative: blanket cancel by object ID or rollback all mutations. Rejected
because player/other-event changes can supersede the event and world changes
are not transactional.

### 7. Complete authoring with bounded diagnostics

Extend the existing flat object list/history with fact, region and event records.
Regions have numeric bounds, selectable editor-only wire boxes and placement
on the nearest supported upward surface using the current capture/cancel policy.
Facts/events are list-only records, without invented transforms. Properties
edit typed references, trigger/guard/cancel lists and ordered steps. Unknown
references remain visible; rename updates typed incoming references atomically,
delete preserves broken links, and undo/redo restores the whole edit. Duplicates
receive a new ID and preserve outgoing references. Validation rejects self event
references and unsupported combinations, including scene-entry with rearm.

Preview is schematic and silent; Play uses the ordinary validated saved file
in a separate process. Add semantic metadata and independent document inspection
for every new standard control, including draft/applied values, selection,
step ordering and validation. No bespoke automation action per feature.

Development diagnostics expose current facts, each event run/step/wait reason
and a last-256-transition trace with IDs, instance, reason, active time and
dropped-record count. Record transitions, not every repeated wait poll. Use a
bounded read-only development snapshot/trace and the existing development
pause entry; expose it in the explicit T4 development runner and its retained
report, not as implementation labels in the player's game flow. Full editor
runtime inspection and prepared launches remain P11.

Alternative: put a live runtime in the editor. Rejected to retain ownership
boundaries and ordinary saved-file behavior.

## Risks / Trade-offs

- Confusing requested and completed actions -> separate set requests from
  actual-state waits, and report door obstruction without automatic retry.
- Cancellation races with replacement -> expected-instance checks inside the
  concrete owner, with replacement/cancel tests, not caller-only assumptions.
- Frame grouping changes available observations -> state the determinism
  boundary above and retain both synthetic traces and integrated stall cases.
- A condition can wait forever -> expose the unmet predicate/busy owner and
  provide explicit cancellation; do not invent timeouts or force success.
- Cycles across separate events -> one active run per event, fresh-trigger
  rearming and finite per-boundary work; traces expose repeated authored cycles.
- Expanding authoring effort -> keep one typed linear editor and test semantic
  controls independently from region picking, placement and visual acceptance.

## Migration Plan

1. Introduce v11 and exact v10 reading alongside all existing v2-v9 mappings.
   Older shapes reject narrative fields and normalize to empty narrative arrays;
   opening remains clean and does not rewrite files. Explicit Save emits v11.
2. Preserve every existing field/order and retained historical compatibility
   fixture. Update current packaged scenes and current-format generators to
   v11 with empty narrative arrays; provide an idempotent v10-to-v11 conversion
   that preserves existing v11 data. Add separate neutral T4 scene resources.
3. Integrate concrete owner commands, progression, then editor authoring. Empty
   narrative levels must retain the prior gameplay/input/audio behavior.
4. An older executable cannot read v11. Rollback uses an untouched original or
   Save As copy; do not offer a downgrade that silently discards narrative data.
   Keep main specs unchanged until completion/synchronization of this change.

## Validation Strategy

- **Automated:** codec/migration/negative validation; reducer traces; concrete
  door/light/radio and actor/cue ownership; accepted interaction occurrences;
  zero-step/multi-step crossings; pause/reading/stalls; editor commands/history,
  real-ImGui controls and semantic draft/applied assertions. Run affected targets
  incrementally, then full Debug and automation suites per DEVELOPMENT.md.
- **GPU:** ordinary game/editor smoke plus the neutral T4 fixture, minimize/
  restore and swapchain recovery, partial startup and shutdown after owned
  cue/route cancellation, with zero unexpected Vulkan validation errors.
- **Authoring/semantic:** prepare a second variation with expected saved/applied
  states, delegate its MCP/official SDK scenario, retain A/B/C evidence separately.
  Broken-link repair, cancel/undo and saved-file Play remain explicit checks;
  semantic success does not certify viewport operations or game launch.
- **Manual:** separately authorize and delegate appearance/placement checks;
  record ordinary scene play, cancellation before and during owned actions,
  door/actor obstruction, reading, audible/muted/no-device behavior. Human
  listening is separate from automated cue/caption state and screenshot evidence.
- **Performance:** in Release on the same supported machine/settings, compare
  the same neutral scene with events disabled and at the supported event count;
  retain three paired runs, warm-up and CPU/GPU/frame p50/p95/p99 using existing
  timing scopes and the accepted T1 gates. Exercise zero and capacity definitions
  without inventing additional actors/lights beyond their limits. Failed gates
  require investigation, not new thresholds or speculative infrastructure.

T4 acceptance needs the ordinary neutral sequence plus the independent authored
variation and retained evidence/limitations. Automated success alone does not
close manual acceptance. No story, checkpoint, P11 or P12 completion is required.
