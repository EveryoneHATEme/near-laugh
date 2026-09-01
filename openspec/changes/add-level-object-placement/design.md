## Context

See `proposal.md` for motivation. This change assumes the persistence and editor-foundation changes are implemented and archived. The editor owns a valid `LevelDocument`, renders it with a free camera, and tracks path and dirty state, but its presentation is read-only.

The supported level remains intentionally flat: a vector of axis-aligned solids, one spawn, two lights, and one static prop placement. Terrain mutation is deferred to `add-terrain-sculpting`.

## Goals / Non-Goals

**Goals:**

- Add precise selection and useful placement without adding persistent entity identifiers or a hierarchy.
- Make every committed edit deterministic, undoable, validated, and reflected in preview and dirty state.
- Keep manipulation code testable without GLFW, ImGui, or Vulkan.

**Non-Goals:**

- ImGuizmo, arbitrary rotation, multi-selection, snapping grids, prefabs, asset catalogs, or component composition.
- Adding/removing the fixed spawn, lights, prop, or terrain.
- Game physics preview or play-in-editor.

## Decisions

### Use editor-only transient object identifiers

Represent selection with an `EditorObjectId` allocated by the document session. Solids receive transient IDs parallel to their vector entries; spawn, light slots, and prop use fixed editor IDs. IDs are never serialized and therefore do not change the level format or game runtime.

Commands that add, duplicate, remove, undo, or redo carry the relevant transient ID so selection remains stable when vector indices shift. Opening another document recreates IDs deterministically from document order and clears selection/history.

Persistent UUIDs were rejected because no runtime behavior or cross-document reference requires them.

### Keep edit operations in a UI-independent document controller

Add an editor document controller with explicit commands for property replacement, terrain-anchored positioning, solid insertion, duplication, and removal. Each command validates field-level preconditions, applies one mutation, refreshes full level diagnostics, updates preview-dirty flags, and records history.

ImGui panels translate completed widget edits into commands; they do not mutate `LevelDocument` fields directly. Continuous numeric drags capture the before value on activation and commit one command on deactivation, preventing one history entry per rendered frame.

### Perform analytic CPU picking against authored proxies

Build a world-space ray from the pointer position and editor camera. Intersect solids as AABBs, the static prop through its authored yawed box proxy, and editor-only finite marker spheres for lights and spawn. Compare positive ray distances and choose the nearest hit. Terrain intersection is computed separately from the exact heightfield triangles for direct placement but terrain is not selectable in this change.

This matches the level's bounded collision descriptions and avoids GPU ID buffers, readback synchronization, or mesh-level picking.

### Use simple editor overlay geometry

Extend the concrete editor renderer with one small line/overlay path for selected bounds, light/spawn markers, and placement feedback. Overlay values are derived each frame from editor state and never enter the game frame contract or serialized level.

No general debug-draw service is introduced. The overlay supports only the shapes required by this editor change.

### Define deterministic terrain anchoring per object type

The placement ray uses the nearest heightfield-triangle intersection. Solids set horizontal center to the hit and vertical center to terrain height plus half extent. The spawn sets its foot position to the hit. Lights set their position directly to the hit plus their existing vertical offset relative to their prior terrain anchor. The prop sets translation to the hit while retaining yaw, scale, and proxy values.

Numeric property edits remain available for exact positioning and for placements intentionally above terrain.

### Implement a 128-entry command history with revision identity

Use concrete command variants containing minimal before/after data and selection states. Each committed command receives a unique before/after document revision; undo restores the before revision, redo restores the after revision, and a new edit after undo discards the redo branch. Dirty state is `current_revision != saved_revision`; successful save updates only `saved_revision`.

This remains correct when undo returns to the last saved state and when edits branch after undo. Full document snapshots were rejected because commands are simple and later terrain strokes can store sparse sample changes.

### Permit invalid intermediate documents but gate persistence

Field parsing rejects non-finite or structurally meaningless values before mutation. Cross-object or gameplay validation runs after a valid field commit and may leave the document invalid so the user can repair it. Preview continues from finite data; save remains disabled until all shared diagnostics clear.

## Risks / Trade-offs

- **[Transient IDs complicate history around removal]** -> Commands own removed values, insertion positions, and IDs so undo restores the exact session identity.
- **[CPU picking differs from rendered mesh at edges]** -> Use the same authored boxes, prop proxy, terrain triangulation, and camera matrix conventions as scene generation.
- **[Full validation after each command may cost more as solids grow]** -> The format caps solids at 240 and terrain at 9,409 samples; measure before introducing incremental validation.
- **[No general gizmo makes rotation less tactile]** -> Provide numeric yaw and terrain placement first; add a concrete handle later only if workflow testing demonstrates the need.
- **[Invalid previews can produce confusing geometry]** -> Reject non-finite/non-positive primitives at field commit and reserve invalid intermediate state for safe cross-object constraints.

## Migration Plan

1. Implement and archive persistence and editor-foundation changes first.
2. Add editor object IDs, command controller, history, and deterministic unit tests without UI/render integration.
3. Add analytic picking and terrain anchoring tests using known camera rays and level geometry.
4. Add ImGui object list/property controls and concrete selection overlays.
5. Exercise add/duplicate/remove/edit/save/undo workflows and run Vulkan validation plus regression tests for `fps`.

Rollback removes editor mutation, selection, overlays, and history while leaving the read-only editor and persisted level intact.

