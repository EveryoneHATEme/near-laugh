## Context

See `proposal.md` for motivation. This change assumes `remove-legacy-fps-assumptions`, persistence, editor foundation, and object placement are implemented and archived. The active editor document is version 2 and already has transient state, bounded command history, shared validation, terrain picking support for placement, and a generated heightfield preview.

The terrain is a fixed 97-by-97 row-major height array with 0.5-metre spacing. Its solids retain only the floor, boundary, and obstacle surface roles. Runtime rendering and Jolt collision remain immutable after `near_laugh` startup; only the editor mutates document samples.

## Goals / Non-Goals

**Goals:**

- Make brush results deterministic from explicit stamps and independent of render timing.
- Rebuild preview geometry safely while preserving simple Vulkan ownership.
- Store one compact history command per continuous stroke and surface spatial validation failures.

**Non-Goals:**

- Terrain topology, dimensions, origin, or spacing changes.
- GPU compute brushes, multithreaded mesh generation, dynamic game collision, or runtime deformation.
- Paint layers, procedural tools, erosion, holes, caves, or overhangs.

## Decisions

### Reuse exact heightfield-triangle picking

Use the same cell diagonal and interpolation convention as `prototypeTerrainHeightAt` and render mesh generation. A pointer ray tests the bounded grid and returns the nearest positive triangle hit. The brush footprint is a world-space circle projected onto the heightfield and rendered through the editor's concrete overlay path.

Brute-force testing of 18,432 triangles is acceptable for this fixed terrain and keeps code obvious. Optimization requires measurement.

### Convert pointer motion into explicit deterministic stamps

A stroke records its brush settings at press time. Apply the first valid hit immediately, then place further stamps along the world-space pointer path at a fixed spacing of half the terrain sample spacing. Carry unused path distance between input updates. This yields the same stamp series for the same sampled world path rather than tying brush strength to frame duration.

Path distance uses X/Z, and only pointer movement supplies new path samples;
terrain rising under a stationary pointer does not produce extra stamps.
Misses break the sampled path. Input capture, navigation, minimize, and document
lifecycle requests finish the current stroke without discarding its edits.

Each stamp visits the smallest row/column rectangle covering its radius, then evaluates samples in row-major order. Distance is measured in the X/Z plane. The falloff control blends between a constant interior weight and smoothstep attenuation toward the radius; raise/lower applies signed `strength * weight` metres.

For `t = distance / radius`, interior weight is
`1 - falloff * t*t*(3 - 2*t)`. At or beyond the radius the weight is zero.

### Smooth from an immutable pre-stamp neighborhood

For each smooth stamp, first copy the affected rectangle plus its one-sample neighborhood. Compute each affected sample's 3-by-3 weighted average from that snapshot and blend the original toward it by `smooth_strength * distance_weight`. Applying results only after all values are computed prevents iteration order from biasing the surface.

The fixed neighborhood uses separable `[1, 2, 1]` weights with total weight 16.

### Coalesce preview rebuilding once per editor frame

Brush stamps update CPU height samples immediately and mark terrain preview data dirty. Before the next draw, regenerate the full bounded terrain vertex stream and replace the editor terrain buffer after the relevant frame fence makes the old buffer safe to release. Multiple input updates in one frame coalesce into one rebuild.

The terrain is small enough that a full CPU rebuild is the simplest correct starting point. Do not change the immutable game mesh buffer or add a generic dynamic-mesh subsystem.

The existing editor world buffer also contains the small solid vertex stream;
rebuilding that buffer preserves those values and avoids another mesh owner.
Both in-flight frame fences must complete before replacing the shared buffer.
Chair, lighting, texture, and pipeline resources survive terrain-only changes.

### Record sparse stroke history

At stroke start, allocate a map from sample index to its first observed value. Every modifying stamp records an index only on first touch. At stroke end, collect final values for those indices in sorted order; omit entries whose before and after values are equal. One terrain-stroke command stores these pairs plus selection/tool state needed by history.

Undo and redo write the stored sample set, rebuild the preview, refresh validation, and use the existing document revision model. A no-op stroke creates no command or revision.

### Extend validation diagnostics with terrain locations

Run full shared validation after stroke completion, undo, and redo, not after every stamp. Terrain slope diagnostics carry cell coordinates and triangle half; spawn support/clearance diagnostics identify the spawn. The editor overlay highlights invalid cells and the validation panel lists the same diagnostic values.

The preview may show a finite but invalid surface during a stroke. Saving remains gated, and invalid numeric results are prevented at stamp application.

## Risks / Trade-offs

- **[Repeated full mesh uploads can stutter during sculpting]** -> Coalesce to one rebuild per frame and measure this fixed 9,409-sample case before adding partial GPU updates.
- **[Input samples can describe different pointer paths at very low frame rates]** -> Resample each observed world-space segment at fixed distance; deterministic replay is defined by the resulting ordered path samples.
- **[Strong brushes create unsavable slopes easily]** -> Show invalid cells immediately after stroke completion and preserve undo as one action.
- **[Sparse history can still approach full terrain size]** -> The terrain and history are bounded; 128 full-terrain before/after pairs remain within a predictable, modest memory ceiling.
- **[Smoothing near terrain borders differs from interior]** -> Clamp the 3-by-3 neighborhood to valid sample coordinates and cover border cases with deterministic tests.

## Migration Plan

1. Complete and archive `remove-legacy-fps-assumptions`, retaining version 2, the three-role surface set, player-oriented terminology, and the `near_laugh` game target; then implement and archive the other prerequisite changes in order.
2. Add pure brush kernels, path stamping, and sparse stroke commands with deterministic tests.
3. Add terrain targeting and brush-footprint overlay behavior.
4. Add coalesced editor terrain preview rebuilding and spatial validation overlays.
5. Exercise long strokes, undo/redo, invalid-slope repair, save/reload, resize, and Vulkan validation; confirm the game runtime remains immutable.

Rollback removes terrain tools, commands, overlays, and mutable editor preview rebuilding while preserving object placement, editor foundation, and the level format.
