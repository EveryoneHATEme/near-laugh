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

## Read Before Making Architectural Changes

Read:

* `docs/VISION.md`
* `docs/ARCHITECTURE.md`
* `docs/GAMEPLAY.md`
* `docs/RENDERING.md`
* `docs/DEVELOPMENT.md`

Relevant requirements are defined in `openspec/specs/`.

Active planned changes are defined in `openspec/changes/`.

When older documentation or implementation assumptions conflict with the
current project vision, do not silently preserve the older assumption.

Raise the conflict or update the affected documentation as part of the
relevant change.

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

* follow `docs/DEVELOPMENT.md`
* review `git diff`
* build the affected targets
* run affected tests
* check Vulkan validation where relevant
* verify the requested behavior rather than only compilation
* check whether documentation became inaccurate

Report any validation step that could not be performed.

Do not describe a change as robust, production-ready, complete, or
architecturally correct solely because the build and tests pass.
