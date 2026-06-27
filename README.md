# near-laugh

A tiny SDL3 GPU first-person shooter prototype.

## Controls

- `WASD` moves
- Mouse looks
- Left click fires a hitscan shot
- `Esc` quits

## Shaders

Edit the GLSL files in `resources/shaders`; CMake automatically rebuilds the
matching SPIR-V files into `build/resources/shaders` when `glslangValidator` or
`glslc` is available on PATH.

The current milestone is intentionally small: a static arena, simple player
collision, colored box targets, and a center crosshair.
