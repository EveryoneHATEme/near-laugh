# P02 implementation validation

Recorded on 2026-09-06. This record distinguishes verified import/resource
behavior from the remaining manual acceptance and spec handoff.

## Selected asset preparation

From the repository root:

```sh
python scripts/prepare_apartment_assets.py --source house_interior_pack
```

The preparation was also repeated with `--resources` pointing to a newly
created temporary directory. All six generated resource files and
`models/apartment_assets.sources.json` matched the packaged copies byte for
byte. All six original source SHA-256 hashes still matched the manifest after
preparation. The script checks preserved geometry/index/UV buffer bytes and
base-color PNG bytes, refuses output inside the source pack, and rejects a
changed source hierarchy or primitive profile. The raw pack is not a CMake
input or executable resource.

| Prepared model | Material | Base color / sampling |
| --- | --- | --- |
| `apartment_chair.glb` | OPAQUE | Embedded 128 x 128 PNG, repeat, nearest/nearest mip |
| `apartment_table.glb` | OPAQUE | Embedded 128 x 128 PNG, repeat, nearest/nearest mip |
| `apartment_phone.glb` | MASK, cutoff 0.5 | Embedded 128 x 128 PNG, repeat, nearest/nearest mip |
| `apartment_radio.glb` | OPAQUE | Embedded 128 x 128 PNG, repeat, nearest/nearest mip |

Each prepared model has one root, mesh, primitive and material. Materials use
metallic factor 0, roughness factor 1 and no roughness/emission/normal/occlusion
textures. The two additional selected outputs are
`apartment_wood_floor.png` and `apartment_wallpaper.png`, both 128 x 128. Exact
source paths/hashes and provenance are in
`resources/models/apartment_assets.sources.json` and `APARTMENT_ASSETS.md`.

## CPU verification

The latest complete debug CTest log at the time of this audit records **44/44
passing** tests in the following groups:

```sh
ctest --preset debug --output-on-failure -R "^(RuntimeResources|StaticModelLoader|SceneAssets|TextureAssets|SceneMaterial|SceneVertex|ImmutableMeshBuffer|ScenePipeline|SceneLighting|PrototypeScene|ChangingGeometry)\."
```

These results cover all four prepared models and the legacy chair; catalog
bounds; finite positions/normals and retained UVs; independent repeated
placements sharing one model material; empty and selected-only resource sets;
absolute roots from another working directory; missing selected files;
unsupported hierarchy, extensions, BLEND, shading, samplers and external
images; corrupt PNG data, oversized dimensions and overflowing buffer/accessor
ranges; independent structural assignments; and bounded changing door boxes.

Coverage tests verify that MASK compares sampled alpha times factor alpha at
the cutoff, while OPAQUE ignores alpha. These CPU tests do **not** measure
rendered depth holes. The 128-byte lighting/frame push constant contract is
unchanged and its layout tests pass.

The source boundary script also passed when run directly:

```sh
cmake -DSOURCE_ROOT=D:/programming/near-laugh -DBUILD_ROOT=D:/programming/near-laugh/build/debug -P tests/boundary/check_sources.cmake
```

It retains backend/parser dependency boundaries. Incidental exact material
bind counts and old chair member names were removed; the Vulkan lifecycle and
editor smoke now check actual ownership and frame draw events instead.

## Packaged shaders

Both commands were rerun, followed by successful validation of both outputs:

```sh
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/prototype_scene_vertex.glsl -o resources/shaders/prototype_scene_vertex.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 --target-spv=spv1.5 resources/shaders/prototype_scene_fragment.glsl -o resources/shaders/prototype_scene_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_fragment.spv
```

The first GPU smoke exposed the SDK default SPIR-V 1.6 lowering of `discard`
to `DemoteToHelperInvocation`, which required an unenabled optional device
feature (`VUID-VkShaderModuleCreateInfo-pCode-08740`). The fragment command above
now explicitly targets SPIR-V 1.5. `spirv-dis` confirms only the `Shader`
capability and `OpKill`; `spirv-val --target-env vulkan1.3` passes. The runtime
device baseline was not expanded.

The vertex binary remains byte-identical because its source interface did not
change. The fragment stage samples a per-material 2D image, reads a separate
32-byte factor/cutoff uniform, and discards MASK fragments before output.

## Combined GPU and visual acceptance

The final combined debug run passed **281/281** CTest cases in 37.34 seconds;
the game/editor build passed. After correcting only two stale editor summary
labels, the full build and all 10 EditorUi tests passed again. Fresh saved-file
preflight re-reads and validates the launch path immediately before importing
its assets; a queued request cannot validate a later unsaved model selection.
The regression rejects both a same-frame editor change and external file edits.

All **7/7 Vulkan smoke cases passed** in 21.80 seconds on AMD Radeon(TM)
Graphics with validation enabled and no unexpected error-severity messages.
The suite includes furnished placements, changing door/feedback boxes across
both fenced slots, static material/mesh retention through recovery, empty
changing frames, partial material/buffer failures, and white/cutout full mip
uploads. Editor smoke covers transactional replacement and actual scene-before-
ImGui command ordering. Logs are build/p02-p03-debug-tests.log,
p02-p03-vulkan-tests.log and p02-p03-editor-ui-tests.log.

Real desktop screenshots confirmed wood floor, wallpaper, repeated chair/table
transforms, the OPAQUE radio and phone cord cutout with the flashlight. A
temporary copy added inspection feet (1.75, 3, -2.7), yaw -74 degrees; a downward
view exposes the phone on the table. p02-furniture-close.png and
p02-furniture-receding.png record close and receding views. The cord has open
holes showing table grain, with no filled rectangle; remaining radio texels
remain opaque. Point/spot enable combinations are also exercised by smoke.

For a depth-sensitive comparison, a second temporary level listed the phone
model before the table, making the table batch draw after the phone. Identical
camera/light input produced p02-depth-holes.png. This screenshot and
p02-furniture-close.png are byte-identical PNGs, SHA-256:

```text
C06EB7D4EFAE84651C248674E6E1AFC43671D5D3025F554BC69B9D0F9AA1AA55
```

Thus the contrasting table remains visible through the cord's cutouts even
when rendered later; the covered phone remains in front. This supplies actual
GPU color/depth evidence beyond the CPU alpha helper. Original packaged levels,
models and source pack were unchanged by the inspection copies.

At the earlier automated/desktop audit, task 7.5 remained open only for the full manual furnished route from both named
starts. Both complete routes pass the standing-player/door deterministic test;
desktop input covered the doorway, kitchen and upward stairs in separate runs,
but no reliable complete manual route record was obtained. See P03 validation
for two-sided door, player-stop, light and restart observations and limits.

## Documentation and spec handoff

ARCHITECTURE, GAMEPLAY, RENDERING, DEVELOPMENT, ROADMAP and selected-asset
preparation/provenance docs describe the final composed v6 implementation.
Task 7.6 is closed: after P03 synchronization and archive, all thirteen P02
capability deltas were merged and verified against the main specs. Stale
Purpose descriptions were updated for the final authored scene, materials,
placements, physics and rendering profile. P02 passed strict validation and
was archived; roadmap and dependency links now point to both archives.

Final git diff review and git diff --check passed. The raw house_interior_pack
and unrelated plot.md remain untouched and untracked. There is no generalized
asset registry, hierarchy editor, PBR pipeline, runtime prop mutation or new
Vulkan device feature requirement.

## User acceptance on 2026-09-06

After the remaining manual checks were explicitly listed in the conversation,
the user confirmed that they had checked the changes, were satisfied, and
requested closure of P02 and P03. This closes manual acceptance task 7.5.
This is user-reported acceptance; no new agent-observed screenshots or
per-scenario measurements are claimed. The earlier audit limitations above
remain a historical record of the agent-run checks.

## Archive verification on 2026-09-06

All 23 main specs passed strict validation after the ordered sync. Closure
changes only documentation, task records and specifications; runtime code,
assets and build configuration are unchanged. The recorded build, CPU tests
and Vulkan smoke results above were not rerun for this documentation handoff.
