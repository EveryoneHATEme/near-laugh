# Agent Instructions

## Project

This repository contains a purpose-built C++/Vulkan runtime for a
single-player first-person narrative horror game.

The target experience emphasizes authored environments, exploration,
interaction, atmosphere, lighting, spatial audio, environmental
storytelling, and scripted events.

It is explicitly NOT a general-purpose game engine.

Combat is not a baseline assumption.

Do not infer weapons, damage, enemies, combat AI, or other shooter systems
from legacy code, names, tests, documents, or the fact that the game uses
a first-person perspective.

## Context and Reading Policy

Load the smallest context sufficient for a correct change. This policy limits
unnecessary reading, not required instructions, dependencies, or verification.

### Task scope and discovery
- Start with the requested outcome, `git status --short`, and likely affected
  paths. Preserve pre-existing user changes.
- Check applicable nested agent instructions before editing their files.
- Discover paths with scoped `rg --files` or `git ls-files`; locate symbols with
  scoped `rg -n`; then read the relevant definitions, callers, and tests.
- Prefer focused excerpts for large files. Expand to complete functions or
  related files whenever needed to understand behavior, invariants, or lifetime.
- Do not survey the whole repository for a local task. Widen discovery only to
  answer a concrete unresolved question; stop exploring once the edit and its
  verification are sufficiently understood.

### Documentation routing
- Architecture or ownership changes: `docs/ARCHITECTURE.md`; also consult
  `docs/VISION.md` when product scope is involved.
- Gameplay changes: relevant parts of `docs/GAMEPLAY.md` and `docs/VISION.md`.
- Rendering changes: `docs/RENDERING.md` and relevant architectural constraints.
- Build and validation: applicable sections of `docs/DEVELOPMENT.md`.
- Read current specs for affected behavior. Documentation links elsewhere in
  this file are references to relevant sections, not a mandatory full-doc sweep.
- Flag conflicts with current product direction; resolve affected documentation
  within the agreed scope rather than silently retaining obsolete assumptions.

### OpenSpec
- Trivial local fixes do not need a new change. Keep required spec updates when
  behavior changes; context economy is not permission to bypass the workflow.
- Work on the selected change, not every entry in `openspec/changes/`.
- When using an OpenSpec skill, read its required inputs, including every path
  returned in `contextFiles`. Do not load unrelated changes, specs, or skills.
- Read archived changes only for a concrete history or regression question.
- Keep progress in the selected change's task checklist; do not restate the
  proposal, design, or task list after each implementation step.
- Propose splitting oversized work into coherent changes before implementation;
  never silently omit requirements or declare partial tasks complete.

### Tool output
- Do not dump directory trees, whole large files, asset data, or build logs.
  Query the needed paths, symbols, metadata, or fields instead.
- Avoid build trees, fetched dependencies, and generated assets in routine
  source searches; inspect them when they are directly relevant to the issue.
- Save verbose command output to a temporary log. Report the command, exit
  status, summary, and relevant diagnostics; preserve the real exit status.
- Treat truncated search or log output as incomplete. Narrow the query or read
  another relevant range instead of assuming unseen output is unimportant.
- Re-read unchanged material only when it is missing from usable context or a
  new question requires a different part; do not repeat orientation rituals.

### Verification and handoff
- Run affected checks during iteration; run broader integration and Vulkan
  validation when the change requires them. Never skip required validation
  merely to save context, and report checks that could not be performed.
- Delegate only bounded, independent questions when subagents are available;
  request findings and file references rather than raw file dumps.
- Keep routine updates brief. Final output should cover changes, checks and
  results, and unresolved risks without repeating the full diff.
- At a handoff, retain the goal, selected change, relevant paths, decisions,
  completed checks, and next action. Do not copy the transcript or source files.

## Subagent Delegation

Use available subagents proactively under the triggers below. Do not wait for
another explicit user request. This dispatch policy applies to the main agent;
subagents must complete their assigned scope without spawning further agents.
Respect user constraints, applicable instructions, and tool/permission limits.

### When to delegate
- Before substantial exploration or implementation, identify a bounded question
  or work unit to delegate. Start with one subagent; keep at most two active.
- Delegate before doing the same work locally when any of these applies:
  - Finding the relevant behavior or cause requires investigating separate
    subsystems, and the useful result is a compact code map or diagnosis.
  - A verbose build/test run or a long log needs focused diagnosis.
  - An implementation unit has explicit file ownership, agreed interfaces,
    and no unresolved dependency on concurrent edits.
  - A non-trivial change to resource lifetime, Vulkan synchronization, shared
    interfaces, persistence, or another critical invariant needs independent
    review before completion.
- A bounded investigation may be delegated for context isolation even when
  implementation must wait for its result. Parallelism is not required.
- Stay local for straightforward edits, a targeted lookup, or tightly coupled
  work that cannot be bounded safely. Do not invent work to satisfy a quota.
- Announce the delegated scope briefly. For substantial work kept local, give
  the concrete reason once. If subagent tools are unavailable, report that and
  continue locally; never pretend that delegation occurred.

### Assignment and return contract
- Give each subagent one outcome, relevant paths/symbols, known facts and
  constraints, the selected OpenSpec change/task when applicable, write
  permissions, and a clear stopping condition. Do not paste the transcript.
- Default to no source edits. For implementation, assign an explicit file set;
  for builds/tests, identify allowed output paths and shared resource limits.
- Require a compact return: findings or changes; evidence with file/symbol
  references; exact checks and outcomes; uncertainties/blockers; next action.
  Aim for 300 words, but never omit a critical finding to meet this target.
- Keep raw searches, source dumps, and full logs out of the parent response.
  Preserve necessary logs as artifacts and return their paths and diagnostics.
- Subagents must follow applicable repository instructions and required skill
  inputs. Report missing access or insufficient evidence rather than guessing.

### Coordination and acceptance
- Do not repeat a delegated investigation while it is running. Continue other
  independent work, or wait when its answer is required for the next decision.
- Treat the working tree as shared unless isolation has actually been verified.
  Never allow overlapping concurrent writes, including the main agent's edits.
  Coordinate build directories, generated files, GPU runs, and other shared
  resources. Preserve pre-existing user changes.
- The main agent owns cross-cutting decisions, the active OpenSpec workflow,
  and its task checklist. Workers report task status; they do not independently
  rewrite the plan, mark tasks complete, or start separate OpenSpec workflows.
- Reuse the same subagent for follow-ups on the same scope. Recheck evidence
  when relevant files change. Close its thread after accepting the result.
- Review the actual diff and verify critical claims. Run the required final
  checks on the integrated state; a subagent's "done" is not acceptance.
  Wait for required results before claiming completion. Report failed,
  unavailable, or skipped checks, and unresolved risks explicitly.

## UI Validation Routing

The main agent owns test selection, implementation, and OpenSpec acceptance.
Delegate test execution without waiting for an explicit user request:

- Existing automated UI tests -> ui_test_runner.
- Screenshot-based UI interaction -> ui_driver.

Prepare the build, fixtures, scenario, and expected results before any
foreground desktop interaction. Delegate a complete bounded scenario,
not individual clicks.

Only one agent may control a given GUI session at a time.
Do not repeat the delegated scenario in the main agent without a specific
evidence gap or a relevant code change.

If the assigned model or GUI tools are unavailable, report the blocker.
Do not silently fall back to foreground testing with the main agent.

Do not seize the user's active desktop without explicit authorization
for that run. Prefer an isolated test environment.

Automated success is not visual acceptance. Report unavailable checks.

## Product Scope

Do not generalize a feature for hypothetical future games.

Prefer concrete game-specific code over generic frameworks.

The runtime may be deliberately specialized for:

* one local player
* first-person exploration
* authored levels
* world interaction
* scripted events
* lighting and atmosphere
* spatial audio
* game-specific characters or threats
* game-specific save/progression state

Do not introduce architecture for unsupported:

* platforms
* rendering APIs
* game genres
* multiplayer
* plugin ecosystems
* general-purpose scripting
* general-purpose editors

The existing standalone authoring tooling may evolve to support this game.

Do not turn it into a general-purpose engine editor without an explicit
requirement.

## Legacy FPS Assumptions

Earlier versions of the project targeted a first-person shooter.

That direction is obsolete.

Do not introduce or preserve architecture merely because it supports:

* hitscan weapons
* projectiles
* ammunition
* reloads
* recoil
* weapon switching
* enemy health
* generic damage systems
* combat AI
* shooter inventories
* FPS-style movement mechanics

unless an active game requirement explicitly needs that feature.

If old FPS terminology appears in documentation or code, treat it as a
candidate for cleanup rather than evidence of current product intent.

## Engineering Policy

Prefer:

* simple code over clever code
* explicit ownership over implicit ownership
* RAII over manual lifetime management
* composition over unnecessary inheritance
* measured optimization over speculative optimization
* small APIs over extensible APIs
* behavioral guarantees over implementation-shape guarantees
* game requirements over engine purity
* authored solutions over generic systems when both solve the same need
* fewer concepts over additional abstraction layers

A small amount of duplication is acceptable when removing it would require
a premature framework.

An abstraction should earn its existence by simplifying a real problem.

## Architecture Changes

Do not introduce any of the following as part of an unrelated task:

* generic ECS
* job system
* render graph
* RHI abstraction
* plugin architecture
* general-purpose scripting language
* custom allocator framework
* bindless renderer architecture
* asynchronous compute architecture
* generic behavior-tree framework
* generic inventory/equipment framework
* general-purpose event bus

Any such change requires a concrete game or technical requirement and an
explicit architectural review or OpenSpec proposal.

The existence of a common engine pattern is not sufficient justification.

## Gameplay Architecture

First-person perspective does not imply first-person-shooter architecture.

When implementing gameplay, start from the concrete player experience.

For the current project, likely concerns include:

* exploration
* interaction
* authored events
* doors and props
* environmental state
* lighting changes
* spatial audio
* progression
* save/load
* characters or threats where required

Do not invent systems from this list before they are needed.

When a feature is small, implement the concrete feature first.

Generalize only after multiple real use cases demonstrate a stable common
abstraction.

## OpenSpec Workflow

For non-trivial features or architectural changes:

inspect existing specs
→ explore the current implementation
→ create or update an OpenSpec change when required
→ review proposal/design/specs
→ implement tasks
→ build/test/validate
→ archive after completion

Trivial local fixes do not require a new OpenSpec change.

Do not create a specification merely to formalize an implementation detail
that can be expressed clearly in code and tests.

OpenSpec records requirements and decisions.

It is not evidence that an architectural decision is correct merely
because the implementation conforms to the specification.

Real gameplay requirements may justify revisiting an existing spec.

## Implementation Rules

Stay within the requested change.

Do not refactor unrelated code.

Do not silently expand scope.

Do not create abstractions with only one hypothetical implementation
unless they establish a meaningful architectural boundary.

Do not add extension points without an existing caller.

Do not design APIs for hypothetical future features.

When requirements are ambiguous, prefer the solution with less machinery
and fewer concepts.

Preserve external-library boundaries where they provide real ownership,
lifetime, dependency, or testing value.

Do not add wrappers solely to make third-party code invisible.

## Testing Policy

Prefer tests that verify observable behavior, invariants, ownership
boundaries, failure handling, or deterministic simulation.

Source-level checks may be used for narrow architectural boundaries that
are difficult to verify otherwise.

Do not use source-text or regular-expression checks to freeze incidental
implementation details such as:

* exact function names
* exact call counts
* source ordering that has no semantic requirement
* a specific implementation syntax
* details that could change during a behavior-preserving refactor

A test suite passing does not prove that the architecture is appropriate.

Tests should protect behavior and intentional constraints while allowing
reasonable refactoring.

When fixing a defect, add a regression test when practical and valuable.

## Documentation

Keep documentation focused on durable requirements and important
architectural decisions.

Do not duplicate the same rule across multiple documents unless each copy
serves a clear purpose.

Separate:

* product requirements
* current implementation details
* temporary milestone constraints

Do not turn temporary prototype behavior into a permanent product
requirement without explicit justification.

When a change makes documentation inaccurate, update the relevant
documentation in the same change.

## Performance

Do not optimize from intuition alone.

Before introducing substantial performance complexity, identify the
measured problem.

Prefer a clear implementation until profiling demonstrates that a more
complex solution is justified.

Performance-sensitive changes should preserve debuggability whenever
practical.

## Completion

Before reporting a task as complete:

* follow the applicable validation sections of `docs/DEVELOPMENT.md`
* review `git diff`
* build the affected targets
* run affected tests
* check Vulkan validation where relevant
* verify the requested behavior rather than only compilation
* check whether documentation became inaccurate

Report any validation step that could not be performed.

Do not describe a change as robust, production-ready, complete, or
architecturally correct solely because the build and tests pass.
