# P07 source inspection — 2026-09-08

This is a read-only inspection of local source files, not evidence of successful
runtime import, retargeting, animation quality or visual acceptance. No source
archives, extracted files, aliases or models were modified during planning.

## Inputs and provenance

Use `build/p07-assets/README.md`, `manifest.json` and `inventory.json` as the
local acquisition record. The included license files identify both Standard
packages as CC0 1.0. Implementation must retain those notices and a compact
source/derivative manifest under versioned resources; `build/` is ignored.

| Input | SHA-256 |
| --- | --- |
| Universal Animation Library[Standard].zip (manifest) | `cc73fc4e495b82958207316596317a3f40b9fa38065bde1027937452da537724` |
| Universal Base Characters[Standard].zip (manifest) | `fdbf1804c90dfc1ea03e992bff7da2dfd1a79318e13270a660180f9308455f40` |
| UAL1_Standard.glb (computed in this inspection) | `69591853d817488edaa8fd9bf8fc1d821eaeaf789f8627b3cd23b41c4ed67997` |

Only the GLB hash above was recomputed here; ZIP CRC/hash validation is reported
by the supplied manifest, not repeated by this planning pass.

## Observed content

| Asset | Meshes / primitives / materials | Joints | Vertices / indices | Animation |
| --- | --- | --- | --- | --- |
| UAL1_Standard.glb | 1 / 2 / 2 | 65 | 8,546 / 41,232 | 43 clips |
| Superhero_Male_FullBody.gltf | 3 / 3 / 3 | 65 | 8,483 / 42,954 | none |
| Superhero_Female_FullBody.gltf | 3 / 3 / 3 | 65 | 8,844 / 45,180 | none |

The 7,618,436-byte animation GLB includes its own mannequin, two constant
base-color double-sided materials, no images and an Armature root. Its selected
mesh is skinned; positions/normals/UVs/weights are float and joints are unsigned
bytes. All three selected clips have 195 LINEAR channels (translation, rotation
and scale for every joint).

| Source clip | Duration | Total input keys across channels |
| --- | --- | --- |
| Idle_Loop | 2.5 s | 14,820 |
| Walk_Loop | 1.3333333333333333 s | 7,995 |
| Interact | 2.0 s | 11,895 |

Decoded root translation, rotation and scale are constant throughout each
selected clip. The root rotation includes the existing -90-degree X conversion.
The largest selected scale-key deviation from one is below 0.0000005.
The static POSITION accessor envelope is approximately Y=0.00046..1.82918 m;
this is a bind-mesh observation, not a bound on all animated poses.

The two Superhero bodies have different pelvis translations, bone rotations
and proportions from the animation mannequin. For example pelvis translation
is (0, 0.05010, 0.91670) in the library, (0, 0.04300, 0.94910) in the male
body, and (0, 0.04570, 0.93180) in the female body. Names alone cannot justify
copying absolute animation channels onto those skins. They also use external
textures, multiple meshes, extra UV/color channels, normal/roughness inputs
and double-sided materials outside the static importer profile.

## Planning consequence

Use the already animated mannequin to validate P07's supported pipeline.
Prepare only Idle_Loop, Walk_Loop and Interact as catalog clips `idle`, `walk`
and `interact`; normalize materials to the game's diffuse profile and remove
unused channels/data explicitly. Preserve local downloads for later character
art preparation. Neither all 43 clips nor combat actions become requirements.

Turning uses a bounded actor yaw change while standing, with its visible
foot-pivot limitation recorded. A dedicated turn animation or transfer onto
Superhero bodies is a subsequent asset/acting task with its own validation.
