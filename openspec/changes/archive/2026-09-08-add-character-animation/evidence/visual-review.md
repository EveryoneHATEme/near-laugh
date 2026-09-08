# Retained character image review — 2026-09-08

Independent inspection of all 26 PNGs listed in
[captures.json](captures/captures.json), all eight desktop screenshots in
`viewer-controls/`, and the preparation script's
[orthographic mesh reference](../../../../../resources/characters/evidence/pose-reference.png).
The images were opened individually with `view_image`. This review did not run
the viewer, build code, or infer appearance from passing tests.

## Mesh, clips, and lighting

| Evidence | Actual observation |
| --- | --- |
| [bind](captures/bind.png) and the independent reference's bind column | Upright body, sideways extended arms, attached hands and feet, orange body pieces and purple joints agree in overall proportions. The shadow has the corresponding extended arms rather than an upright capsule silhouette. |
| `idle-0/25/50/75/99.png` | The wide stance and lowered arms remain recognizable throughout. Changes in hands, shoulders and body outline are small compared with walking. No collapsed limb, detached body part or return to the sideways-arm bind pose is visible. |
| `walk-0/25/50/75/99.png` and the reference's three walking columns | The leading leg alternates between the start and half-cycle images. Quarter-cycle images show a bent swing leg with its foot lifted. Arm positions and the receiver shadow change with the legs. The frontal Vulkan view compresses fore/aft motion; the reference's side row makes the leading heel, lifted toe and folded swing leg easier to distinguish. |
| `interact-0/25/50/75/99.png`, [desktop reach](viewer-controls/interact-reach.png), and the reference's last column | The mannequin's left arm extends forward, appears foreshortened from the front, and returns downward near the end. The other arm and wide stance remain recognizable. The reaching hand is near the reference pose in the desktop image labelled `interact 0.70 s`; the reference is sampled at 0.733333333 s, so these are nearby poses, not identical samples. |
| [four independent](captures/four-independent.png), [editor retained four](captures/editor-retained-four.png), and [side view with flashlight](viewer-controls/four-side-flashlight.png) | Four bodies are present with different poses and orientations. Rear bodies are partly occluded by the nearer bodies in the frontal view. The side view exposes the forward arm extension and forward/backward leg separation. Orange body surfaces and purple neck, waist and limb joints remain distinct at the different yaws. The editor image also retains the separate door and its shadow. |

The curved shoulders, head and limbs have coherent light-to-dark surface
variation in the frontal and side views. No conspicuous inverted patch, exploded
triangle, missing section or stretched skin spike is visible in these images.
The independent reference uses flat triangle lighting, while Vulkan uses the
surface normals and scene lights; matching pixel colors or shading is not an
appropriate comparison. These observations do not independently prove every
normal's numerical value or unit length.

The body has consistent proportions relative to its bind/reference geometry.
The screenshots have no calibrated ruler, so absolute metre scale rests on the
separate [calibration record](../../../../../resources/characters/README.md), not a
measurement made from these perspective images. Likewise, the documented
2.654 cm source sole dip is retained; these images do not establish precise
floor clearance or acceptance for future floor/stair locomotion. The desktop
control overlay crosses some feet, so the unobstructed smoke captures are the
better evidence for lower-leg and foot outlines.

## Loop and transition boundaries

The smoke fixture samples phases `0`, `0.25`, `0.5`, `0.75`, and `0.999999`.
The filenames ending in `-99` therefore mean **0.999999 of clip duration**, not
phase 0.99. Comparing `idle-99` with `idle-0` and `walk-99` with `walk-0` shows
the same recognizable stance, body location and shadow silhouette, without a
visible bind reset. Small image differences remain: the retained readback log
reports 226 changed pixels for idle and eight for walk. The interaction returns
to a lowered-arm pose near its end; it is a clamped one-shot, not a loop.

[Before interruption](captures/transition-before-interruption.png) shows the
captured walk-to-interact blend after 0.06 s, following a seek to walk 0.4 s.
[Interrupted middle](captures/transition-interrupted-middle.png) shows 0.075 s
into the subsequent blend toward idle. The latter has a wider stance and a
correspondingly different leg shadow; both have lowered arms, intact limb
proportions and no sideways-arm bind pose. The smoke's retained successful
readback check separately establishes exact image equality immediately before
and after the interruption command. There is no separate PNG of the completed
0.15 s blend in this retained set.

[Recovered frozen](captures/recovered-frozen.png) visually matches the interrupted
middle image, including its shadow. Their RGB hashes in `captures.json` are
identical. The desktop `walk-seek.png` and `walk-still-paused.png` both show
`walk 0.10 s paused`, the same pose and the same shadow. Their PNG SHA-256 values
were independently rechecked during this review and are both
`f1eb0d703473085715a5025d460dca3c1d47659c591b87fda07352311769a588`.
The restart screenshot reads `walk 0.00 s paused`; the final side screenshot
reads `interact 1.49 s playing` and shows the arm lowered again.

These are inspected stationary samples and boundary comparisons. They do not
constitute a continuous-motion recording or a visual judgment of temporal
smoothness between every sample. Exact endpoint/time-batching guarantees belong
to the behavioral evidence recorded separately in `validation.md`.

## Shadow observations and limitations

The [light-only receiver](captures/offscreen-light-only.png) has no mannequin
silhouette. [Idle](captures/offscreen-idle.png) adds a large body outline with
separated arm regions; [interaction](captures/offscreen-interact.png) moves one
arm silhouette conspicuously outward. No orange/purple mannequin geometry is
visible in these receiver views. The readback record supplies the additional
geometry visibility check and changed-pixel counts; the images themselves show
that the receiver silhouettes differ from each other and the baseline.

Frontal bind, walking and interaction images also show arm/leg changes in the
same scene as the visible mesh. In the control images, disabling the point
light leaves a dim receiver, while the flashlight creates a localized bright
patch. The side-view flashlight image illuminates the bodies without erasing
their point-light shadow silhouettes.

Shadow edges are visibly stepped/coarse in the enlarged off-screen silhouettes.
Small hand details merge, and foot/contact edges are not precise at this shadow
resolution. The evidence supports matching posed silhouettes, with the existing
point-shadow resolution/contact limitation explicitly retained. It does not
establish final character-art quality, route foot placement, collision, final
P07a performance acceptance, or full T2 acceptance.
