## Context

See `proposal.md` for motivation. The current implementation exposes the obsolete direction through the `fps` CMake target and binary, `FpsInputMapper`/`FpsActionSnapshot`, the `fps-input` capability, version-1 level enum values and JSON tokens named `shooting_target`, a fourth packaged texture layer, and validation that requires exactly three target plates. The same language is repeated across durable architecture documents, current main specs, tests, and two unimplemented editor changes.

The levels and resources are prototype fixtures rather than shipped user content. The cleanup can therefore make a deliberate compatibility break, but it must leave runtime/editor resource layouts, generated geometry, texture-layer indexing, validation, and test expectations internally consistent.

## Goals / Non-Goals

**Goals:**

- Establish one neutral naming vocabulary for the game application and one-player input contract.
- Remove target-specific content and validation rather than disguising it behind a neutral rename.
- Make the level-format break explicit and deterministic.
- Keep current first-person movement, flashlight, rendering, physics, and editor behavior unchanged except where they depended on the removed plates or texture.
- Make current documentation distinguish durable architecture from prototype implementation facts.

**Non-Goals:**

- Redesign player movement or decide which prototype movement features belong in the final game.
- Replace the removed plates with new scenery, materials, or gameplay objects.
- Generalize input, materials, level content, or editor architecture.
- Rename the concrete internal `Engine` composition owner solely because of its type name.
- Rewrite archived OpenSpec changes.

## Decisions

### Use `near_laugh` for the game target and player-oriented input names

Rename the CMake target and produced executable from `fps` to `near_laugh`, set the visible application title to `near-laugh`, and update executable-relative resource rules and test invocations atomically. Rename the input source/header and public/internal types to `PlayerInputMapper` and `PlayerActionSnapshot`; the replacement OpenSpec capability is `player-input`.

No alias target, forwarding header, typedef, or duplicate executable is retained. Compatibility names would preserve exactly the obsolete concepts this change is meant to remove. First-person look remains factual terminology because camera perspective does not imply shooter architecture.

### Remove target content instead of renaming it

Delete the shooting-target solid kind and surface enum values, their codec tokens, target-count validation, the packaged texture, and the three plate entries in the prototype level. The remaining fixed surface set is floor, boundary, and obstacle, stored in a three-layer texture array in that stable order.

Renaming the values to `accent` or `marker` was rejected because there is no authored-game requirement for the plates. Keeping them under a neutral name would preserve obsolete content and tests without purpose.

### Advance the test-level format directly to version 2

The codec accepts version 2 only. The packaged level and test fixtures are updated to version 2, while version 1 and `shooting_target` tokens fail through the normal unsupported-version/value diagnostics. There is no migration utility, dual parser, fallback mapping, or alias.

Changing the accepted schema while retaining version 1 was rejected because it would make the version marker misleading. Production save compatibility is a separate future requirement and must not be inferred from these test assets.

### Apply the cleanup from dependencies outward

Update level data types and codec, then runtime/editor resource contracts and rendering layer counts, then the packaged fixture and tests. Rename the input contract before updating runtime consumers, and rename the executable only after target references and launch tests can move together. This order keeps intermediate compiler failures localized even though the final change lands atomically.

### Separate durable documentation from prototype inventory

`ARCHITECTURE.md` will describe the actual `near_laugh_*` module dependency and ownership boundaries, with the concrete internal `Engine` named only as the runtime composition owner. `RENDERING.md` will describe the direct Vulkan boundary and current renderer, removing weapon rendering and speculative feature lists that have no active requirement. `DEVELOPMENT.md` will retain exact commands and current fixture details without defining the product through absent shooter features.

Main spec Purpose text and requirement wording will use game/runtime/player terminology. Explicit shooter terms remain only where they explain a rejected legacy value or the intentional absence of combat architecture. The active object-placement and terrain-sculpting artifacts are updated to version 2 and the three-role surface set before either change is applied. Archived artifacts remain untouched.

## Risks / Trade-offs

- **[Renaming the executable can leave stale build or test references]** -> Search CMake, scripts, tests, docs, copied-resource rules, process probes, and Vulkan application metadata; validate a fresh build rather than relying only on an incremental tree.
- **[Removing one texture layer can desynchronize CPU and shader assumptions]** -> Define the three-role ordering once in the existing bounded contract and verify generated vertex layers, descriptor image layers, resource manifests, and smoke rendering together.
- **[Version-1 files fail immediately]** -> This is intentional; provide an actionable unsupported-version diagnostic and update every repository-owned fixture to version 2 without adding migration code.
- **[Documentation cleanup can erase useful current-state detail]** -> Keep concise implementation facts in `DEVELOPMENT.md` and behavioral guarantees in OpenSpec while removing duplication from architecture documents.
- **[Concurrent editor plans can reintroduce removed vocabulary]** -> Revise both active changes before implementation and make this cleanup a prerequisite in their planning text and tasks.

## Migration Plan

1. Update the level contract to version 2, remove target enums/content/resources, and adjust rendering and editor resource layouts.
2. Replace the FPS-named input capability and code contract, then rename the game target and visible application metadata.
3. Update repository fixtures, tests, current docs, main-spec Purpose text, and active change artifacts.
4. Configure from a fresh build tree, build all affected targets, run deterministic tests and Vulkan smoke validation, validate all active OpenSpec changes, and audit non-archived text for unintended legacy assumptions.

Rollback is a source-level revert of the whole change. No level-data rollback or migration support is provided because repository test fixtures are the only affected documents.
