# Selected apartment assets

The chair, table, telephone and radio are game derivatives of the user's
`house_interior_pack`, by DissonantVoid. The source pack is available at
https://dissonantvoid.itch.io/psx-retro-interior-pack. The supplied source pack
is kept locally in the ignored root directory `house_interior_pack/` and is
not copied by the build or included in Git. Only the selected game resources
belong in the repository:

- `models/apartment_chair.glb`, `apartment_table.glb`, `apartment_phone.glb`
  and `apartment_radio.glb`, with their base-color PNGs embedded;
- `textures/apartment_wood_floor.png` and `apartment_wallpaper.png`.

These paths are relative to `resources/`. Building, testing and running the
game/editor use the prepared files and do not require the source pack.

`apartment_assets.sources.json` records exact source files and SHA-256 hashes.
Retain the source pack's acquisition/license records with the project. These
derivatives do not grant permission to redistribute the raw source pack.

The source pack is needed only to regenerate the prepared files. From the
repository root, reproduce the six selected resources with:

```sh
python scripts/prepare_apartment_assets.py --source house_interior_pack
```

The preparation script preserves geometry, UVs and base-color PNG bytes. It
removes exporter extras and unused roughness data, normalizes the material to
the game's base-color diffuse profile, and retains repeat/nearest sampling with
nearest mip selection. The telephone's alpha-bearing cord changes from BLEND
to MASK with cutoff 0.5; the other props remain OPAQUE. Wood-floor and wallpaper
base colors are extracted for solid/terrain materials, without importing their
source plane geometry. No PBR, emission or blended transparency is implied.
