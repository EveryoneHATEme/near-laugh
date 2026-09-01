## Context

See `proposal.md` for motivation. `PrototypeLevel` currently constructs every terrain sample and authored object in C++, then exposes immutable references to renderer and physics. Validation is distributed across Boolean helpers, resource resolution knows only shaders, textures, and one GLB, and no representation can be saved by a future authoring tool.

The current renderer and physics already benefit from consuming the same level value. This design preserves that startup boundary and changes only how the value is produced.

## Goals / Non-Goals

**Goals:**

- Establish an explicit editable level value, shared validation diagnostics, and an immutable runtime handoff.
- Make one canonical level document readable in source control and stable under an unedited load/save round trip.
- Preserve current prototype content and runtime behavior exactly after migration.
- Keep resource identifiers implicit and fixed rather than storing paths or creating an asset registry.

**Non-Goals:**

- Runtime save/load, hot reload, streaming, or mutation.
- Multiple terrains, variable terrain resolution, arbitrary models, materials, light counts, or collision shapes.
- A general serialization framework used outside FPS level data.

## Decisions

### Use a strict JSON level document with schema version 1

Store the packaged level at `resources/levels/prototype.level.json`. Its root contains `version`, `terrain`, `solids`, `player_spawn`, `environment_light`, and `static_prop` in that canonical order. Vector-like values use named `x`, `y`, and `z` members where field meaning benefits from labels; colors use fixed-size arrays; terrain heights use a row-major numeric array matching the existing sample-index convention.

JSON is selected because it is inspectable, diffable, and supported by mature bounded parsers. A custom text grammar would create parser maintenance unrelated to the game, while a binary format would obstruct review and early authoring. Pin `nlohmann/json` to an exact release through CMake and link it privately to `near_laugh_world`; JSON types must not appear in world, runtime, renderer, or physics interfaces.

Unknown members and unsupported versions are errors. Strict rejection prevents an older editor from silently discarding fields written by a newer one.

### Separate editable data from validated runtime data

Introduce a concrete `LevelDocument` aggregate in `near_laugh_world` containing the serializable values. It may temporarily be invalid so later editor changes can present and repair errors. Introduce a validated immutable `PrototypeLevel` construction path that accepts a `LevelDocument` only after shared validation succeeds.

`loadLevelDocument(path)` parses syntax and returns document data or structured diagnostics. `validateLevelDocument(document)` returns all structural and gameplay diagnostics rather than stopping at the first Boolean failure. `loadPrototypeLevel(path)` composes parsing, validation, and immutable construction for the game. `saveLevelDocument(path, document)` validates before writing and never writes an invalid document.

This is a concrete authoring/runtime boundary, not a generic validation or serialization framework. Renderer and physics retain their existing `const PrototypeLevel&` consumption.

### Keep limits and resource choices in the game-owned schema

Version 1 fixes the terrain at 97 by 97 samples, caps solids at 240, retains exactly two point lights, and retains exactly one placement of the one packaged chair model. The level stores no model, texture, shader, or collision-asset path. Surface and solid kinds serialize as explicit lowercase strings rather than ordinal enum values.

The fixed profile matches current rendering and physics limits and gives the editor useful data without promising arbitrary scenes. A future concrete game requirement may revise the format version through its own proposal.

### Emit canonical JSON explicitly

Write members in the documented order with two-space indentation, locale-independent finite decimal output, LF line endings, and one trailing newline. Preserve solid vector order and terrain row-major order. Parse into project-owned types and emit from those types; do not preserve input whitespace or member ordering.

Write to a sibling temporary file, flush and close it, then replace the destination so a failed write does not truncate the last valid document. On Windows, replacement behavior must be implemented with explicit filesystem error reporting and covered by focused tests.

### Centralize field-aware diagnostics

Use a small level-specific diagnostic value containing a category, a document path such as `solids[4].half_extent.x`, and a message. Parsing adapters translate library exceptions into this form at the codec boundary. Validation reports enough location detail for later editor highlighting, including terrain sample or cell coordinates for slope failures.

Existing Boolean validation helpers may remain as thin compatibility wrappers during migration, but the diagnostic validator becomes authoritative.

### Resolve and load the level before dependent subsystems

Extend runtime resource resolution with the fixed relative path `levels/prototype.level.json`. The engine loads and validates it after window creation and before physics or renderer construction, then passes the same immutable level to both. Resource copying includes the `levels` directory beside the existing shaders, textures, and models.

## Risks / Trade-offs

- **[Large height array makes JSON noisy]** -> Keep the fixed row-major representation and rely on the editor for normal terrain changes; source-control visibility is still more useful than an opaque binary at this scale.
- **[Floating-point text may not round-trip]** -> Use the standard maximum round-trip precision for `float` and test boundary, negative-zero, and locale cases.
- **[Strict unknown-field rejection makes forward compatibility explicit]** -> Require a versioned migration instead of silently accepting data loss.
- **[Replacing the constructor can change prototype content accidentally]** -> Generate the initial document from the current constants once, add semantic equality tests, and retain scene/render/physics tests during migration.
- **[External JSON code expands dependencies]** -> Pin one release, keep it private to the codec translation unit, and expose only project-owned data and diagnostics.

## Migration Plan

1. Add document types, structured validation, JSON codec, and focused round-trip/error tests without changing runtime construction.
2. Create `prototype.level.json` from the current constructor values and prove it is semantically equal to the existing level.
3. Add runtime resource resolution and packaging for the level file.
4. Switch engine composition to the validated loaded level, then remove hard-coded authoring values while retaining calculation and validation helpers.
5. Run deterministic tests, the FPS executable, and Vulkan validation; inspect the scene and collision for parity.

Rollback restores hard-coded `PrototypeLevel` construction and removes only the level codec, packaged file, parser dependency, and resource-resolution additions.

