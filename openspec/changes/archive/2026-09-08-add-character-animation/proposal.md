## Why

P07 needs visible, verifiable skeletal motion before route, collision and editor
work can be assessed. The supplied animation library already contains a rigged
neutral mannequin, allowing this prerequisite to be accepted independently.

## What Changes

- Prepare one packaged mannequin from the local Quaternius Standard download
  with only standing, walking and interaction clips, retaining provenance and
  reproducible preparation outside the runtime.
- Add a separate bounded animated GLB profile, deterministic clip sampling and
  short pose transitions. Keep existing static model loading unchanged.
- Present supplied character poses in the current material, point-light and
  shadow passes, with explicit immutable asset and changing frame ownership.
- Add an explicit development viewer for clip selection, pause, time inspection
  and one/four-instance visual and timing checks. It requires no actor level
  format, collision, narrative events or editor authoring.
- Treat turning as authored placement yaw in P07b; the supplied edition has no
  turn clip. Superhero retargeting, root-motion locomotion, detailed acting,
  morphs, facial animation and extra material features are outside this stage.

## Capabilities

### New Capabilities

- `character-animation`: Prepared animated asset profile, coherent sampled
  poses/transitions and reproducible neutral animation acceptance.

### Modified Capabilities

- `runtime-composition`: Backend-neutral supplied animated pose presentation.
- `vulkan-renderer`: Changing skinned geometry and its frame/resource lifetime.
- `interior-lighting`: Animated rendered geometry contributes coherent shadows.

## Impact

Affects character asset preparation/catalog data, CPU animation, renderer input,
selected resource packaging and game/editor rendering helpers. A small CPU-only
animation target is justified by runtime, viewer and editor consumers; no new
third-party dependency or general animation framework is needed. No level
version changes or main-spec edits occur during planning.

## Dependencies and Boundaries

P07a; starts after archived P04 and P10. Follow with
[P07b: scripted characters](../../add-scripted-characters/proposal.md), then
[P07c: authoring](../../add-character-authoring/proposal.md). Together these replace
the original oversized P07; T2 is accepted only after all three.

The initial representative is the embedded mannequin in
`build/p07-assets/source/Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb`.
The supplied Superhero bodies remain source candidates; matching joint names
do not prove compatible bind poses. See [asset inspection](asset-inspection.md).

## Acceptance Criteria

- Prepared assets reproduce from pinned local sources; a normal checkout/run
  needs only the committed derivatives and licenses.
- Idle, walk, interaction, loop boundaries and transitions have finite, correct
  poses; the imported bind pose is reproduced and malformed data fails safely.
- One and four independently posed mannequins retain matching color/shadows,
  material appearance and state through resize, minimize/restore and failures.
- Record real visual observations, release CPU/GPU/frame timings against the
  retained lighting baseline, and Vulkan validation through teardown.
