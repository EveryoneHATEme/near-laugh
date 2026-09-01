# Agent Instructions

## Project

This repository contains a purpose-built C++/Vulkan engine
for a single-player first-person shooter.

It is explicitly NOT a general-purpose game engine.

## Read Before Making Architectural Changes

Read:

- `docs/VISION.md`
- `docs/ARCHITECTURE.md`
- `docs/GAMEPLAY.md`
- `docs/RENDERING.md`
- `docs/DEVELOPMENT.md`

Relevant requirements are defined in `openspec/specs/`.

Active planned changes are defined in `openspec/changes/`.

## Scope

Do not generalize a feature for hypothetical future games.

Do not introduce architecture for unsupported platforms,
rendering APIs, game genres, multiplayer, scripting,
plugins, or editors.

Prefer concrete FPS-oriented code over generic frameworks.

## Engineering Policy

Prefer:

simple code over clever code
explicit ownership over implicit ownership
RAII over manual lifetime management
composition over unnecessary inheritance
measured optimization over speculative optimization
small APIs over extensible APIs
game requirements over engine purity

## Architecture Changes

Do not introduce any of the following as part of an unrelated task:

ECS
job system
render graph
RHI abstraction
plugin architecture
scripting language
custom allocator framework
bindless renderer
asynchronous compute architecture

Any such change requires an explicit OpenSpec proposal.

## OpenSpec Workflow

For non-trivial features or architectural changes:

inspect existing specs
→ explore
→ create/update OpenSpec change
→ review proposal/design/specs
→ implement tasks
→ build/test/validate
→ archive after completion

Trivial local fixes do not require a new OpenSpec change.

## Implementation Rules

Stay within the requested change.

Do not refactor unrelated code.

Do not create abstractions with only one hypothetical implementation
unless they establish a meaningful architectural boundary.

Do not silently expand scope.

When requirements are ambiguous, prefer the solution with
less machinery and fewer concepts.

## Completion

Before reporting a task as complete:

follow `docs/DEVELOPMENT.md`
review `git diff`
check affected tests
check Vulkan validation where relevant

Report any validation step that could not be performed.