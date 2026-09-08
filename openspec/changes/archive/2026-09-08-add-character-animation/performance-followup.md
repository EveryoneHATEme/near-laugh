# Indexed character presentation — implemented and measured 2026-09-08

The user explicitly authorized indexed rendering on 2026-09-08 after the
technical review below. Design decision 3 now records this bounded revision,
and tasks 5.1–5.4 track implementation and renewed validation. The measured
four-character failure in [validation.md](validation.md) remains retained.
All nine new Release runs now pass the unchanged acceptance gates. Final
functional/visual evidence and measurements are in
[indexed-validation.md](indexed-validation.md); the accepted handoff is ready
for the separate archive workflow.

## Measured reason

All three four-character runs fail GPU p95 and frame p50/p95/p99. Compared
with the light-only setup, approximately 81% of the increase in mean GPU time
falls inside the shadow scope (5.57 ms of 6.85 ms, calculated from raw samples).
That scope does not distinguish vertex processing, rasterization, fragments,
memory bandwidth or synchronization. CPU deformation and upload also add work;
upload p95 reaches 2.17 ms. Sustained acquire waiting accompanies roughly 30 Hz
frame cadence. The current data does not establish the exact scheduling cause.

The original implementation expanded every triangle index into a full 40-byte
vertex, then submitted that stream to color and 24 point-shadow faces.
For the prepared asset, 8,546 unique vertices become 41,232 expanded vertices.

| Four-instance storage | Expanded baseline | Indexed revision |
| --- | ---: | ---: |
| Changing vertex payload per frame | 6,597,120 bytes | 1,367,360 bytes |
| Shared immutable uint32 index payload for this asset | None | 164,928 bytes |

The changing payload reduction is 79.3%. This is a data-size calculation,
not a measured frame-time improvement or a guarantee of passing the gates.

## Bounded design revision

1. Keep CPU pose evaluation and skinning, one deformed vertex per source vertex,
   the existing vertex layout and the two fence-protected vertex slots.
2. Keep a shared immutable index stream per unique selected asset. Preserve
   source triangle order and each primitive's index/material range.
3. Use indexed draws with checked first-index, index-count and vertex-offset
   ranges in the existing color and shadow pipelines. Every pass still uses
   the same evaluated pose. Borrowed frame and instance selection APIs stay intact.
4. Keep four instances, 40,000 source vertices and 200,000 aggregate indices,
   all eight lights/four casters, shadow resolution/filtering and the packaged
   visual profile. This requires no new shader layout or animation backend.
5. Extend the existing RAII resource owner for immutable indices; cover partial
   allocation, candidate failure/retention, slot reuse and final teardown.

Design decision 3 and implementation/validation tasks were updated before
editing the renderer. Existing functional requirements describe geometry,
appearance and ownership rather than non-indexed draw syntax; review their
consistency without changing acceptance thresholds. The original failed
measurement task remains completed evidence, with new comparison work tracked
separately so the failure is not overwritten.

Validate observable triangle/material equivalence, one/four/empty behavior,
off-screen shadows, recovery and editor rollback. Rebuild affected targets and
run affected deterministic and Vulkan checks. Repeat the same nine Release
samples with the final camera/placements and unchanged gates, retaining new raw
data alongside the present failures. Accept and archive P07a only if the full
result supports doing so; otherwise use the new measurements for the next review.

## Technical review before authorization — 2026-09-08 continuation

Independent code and timing reviews support this bounded experiment. No conflict
was found with the supplied-pose, geometry/material, shadow or ownership
requirements. This section records the review before user authorization:
design decision 3 and implementation were still pending confirmation then.
No renderer code, measurement inputs, acceptance gates or task status changed
during that review. The subsequent authorized implementation and fresh evidence
are recorded in [indexed-validation.md](indexed-validation.md).

The implementation can stay in `character_presentation.hpp/.cpp` and
`character_resources.hpp/.cpp`, with their deterministic and Vulkan tests.
Runtime color, editor color and every shadow face already invoke the same
`CharacterResources::draw`. The existing shaders do not use vertex IDs;
preserving triangle order and all vertex attributes needs no shader/API change.

Index offsets identify each distinct asset's shared immutable index stream;
vertex offsets identify each instance's independent deformed source vertices.
Repeated asset owners share index/material data. Distinct assets require distinct
ranges even if their skeleton tags coincide. Check primitive coverage, local
indices, byte products and signed vertex-offset conversion before Vulkan use.
The 200,000 aggregate index limit still counts all selected instances, even
when sharing reduces the stored index payload.

Regression coverage should reconstruct triangles and compare positions, normals,
UVs and material colors against the tiny fixture's independent expectations.
Include reordered pose requests, four shared-asset instances and a mixed
distinct-asset case. Extend partial-construction and editor-retention tests to
the new index buffer/memory and initialization failures. Preserve empty selection,
complete-request validation, two fenced vertex slots, recovery and final teardown.
Current implementation descriptions in `docs/ARCHITECTURE.md` and
`docs/RENDERING.md` must be updated with an implemented revision.

The timing audit verified all nine retained CSV hashes and the recorded current
input hashes, including the Release executable. The four-character sample
intervals remain above 30.9 ms; the failed gates are not a summary-rounding issue.
Recalculated mean GPU/shadow increases are 6.84947/5.57442 ms (81.385% inside
the shadow scope), and the proposed payload reduction is 79.2734%.

One measurement limitation is now explicit in [validation.md](validation.md):
the instrumented renderer waits on the acquired-image semaphore at
`ALL_COMMANDS`, while normal rendering uses `COLOR_ATTACHMENT_OUTPUT`. This
intentionally keeps presentation-image availability outside the GPU timestamps,
but also delays shadow work in the measured path. Consequently, its cadence
does not establish non-instrumented cadence or the exact FIFO scheduling cause.
Keep the same timing path for the proposed before/after comparison; changing
instrumentation would require separately identified evidence and review.
