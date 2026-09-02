# near-laugh

A purpose-built C++/Vulkan engine for a single-player
first-person shooter.

## Documentation

Vision: `docs/VISION.md`
Gameplay assumptions: `docs/GAMEPLAY.md`
Architecture: `docs/ARCHITECTURE.md`
Rendering: `docs/RENDERING.md`
Development: `docs/DEVELOPMENT.md`

## Runtime boundaries

The `fps` executable links the backend-neutral `near_laugh_runtime` facade.
Runtime composition owns `Platform -> Window -> RuntimeResources ->
PrototypeLevel -> PhysicsWorld
-> PlayerController -> PlayerFlashlight -> Renderer` in that
order and destroys them in reverse.
`near_laugh_platform` confines GLFW and physical keyboard/mouse state;
`near_laugh_world` owns the bounded level document, private JSON codec, shared
validation, and immutable prototype terrain, solids, spawn, prop, and two
authored point lights; `near_laugh_physics`
confines Jolt Physics; and `near_laugh_render` confines Vulkan. The runtime
maps physical state to the fixed controls for the one local FPS player.
Polling and blocking waits each open one input batch; the Engine samples a
waited batch before another poll can clear its cursor delta, while held actions
remain active. Each renderer request returns rendered, skipped, or recovered,
and the Engine exhaustively consumes that outcome while retaining loop and
application-lifetime control. The Engine also owns a bounded 60 Hz simulation
accumulator, player/flashlight input, interpolation, cursor capture transitions,
and framebuffer aspect. Physics retains Jolt body data privately. Rendering
receives only a column-major view-projection matrix and one optional
source-independent spot-light frame; it does not interpret player, flashlight,
input, simulation state, or elapsed time.

## Prototype scene controls

The executable starts with the cursor captured and shows one built-in textured
3D room with a floor, boundaries, several obstacles, a low step, and a
crouch-only passage, plus three inert textured and collidable plates. Every immutable
axis-aligned solid carries one fixed floor, boundary, obstacle, or
shooting-target surface role; rendering and static collision derive from those
same solids. The fixed packaged asset
`resources/levels/prototype.level.json` supplies the 97-by-97 terrain, no more
than 240 solids, spawn, exactly two point lights and ambient intensity, and one
chair placement with its box proxy. It contains no resource paths; the fixed
chair GLB, shaders, and four textures remain separately implicit runtime
resources. Startup validates the document once, and the game does not mutate,
save, or hot-reload it while running.
One restrained cool point light surrounds the spawn and one stronger warm
point light marks the destination. Their finite, non-overlapping radii leave
an intentionally dark transition between them over a near-black ambient floor.

- Mouse: look
- W/A/S/D: move horizontally relative to the current view
- Space: jump while grounded
- Left Control: hold to crouch
- Left Shift: sprint
- Escape: release the cursor
- Left mouse button: toggle the flashlight while captured; recapture while released

Movement is constrained by static Jolt collision and includes gravity, wall
sliding, the authored 0.30 m step, bounded air control, and blocked standing
under low clearance. This prototype does not yet contain dynamic rigid bodies,
moving platforms, doors, projectiles, or physics-driven objects. The initially
disabled flashlight follows the interpolated camera and adds one finite-range
spot light with smooth cone falloff. Its render-frame type is independent of
the player, so another concrete object can supply the same one dynamic slot.
There is no weapon, damage, target state, visible flashlight model, battery,
shadow map, volumetric beam, audio, particles, or general light registry.

## Build

The project requires CMake, Ninja, Clang (available as `clang` and `clang++`),
and a Vulkan 1.3 SDK. The debug preset selects Clang for both C and C++ by
portable executable name. On Windows, Clang continues to use the
MSVC-compatible ABI, Microsoft runtime, and Windows SDK; an MSVC compatibility
label in CMake's compiler metadata describes that target, not the selected
compiler.

Configure, build, and run the deterministic tests with:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

If `build/debug` was configured previously with a non-Clang compiler, replace
its cached compiler selection with a fresh configuration:

```sh
cmake --preset debug --fresh
```

The configure output must identify both compilers as Clang. The generated
`build/debug/CMakeCache.txt` records the portable selections, while the
compiler metadata under `build/debug/CMakeFiles` records the resolved paths
and identities. See `docs/DEVELOPMENT.md` for focused PowerShell and POSIX
verification commands.

`prototype_scene_vertex.spv` and `prototype_scene_fragment.spv` are copied to
`build/debug/bin/resources/shaders`. The four fixed textures
`prototype_floor.png`, `prototype_boundary.png`, `prototype_obstacle.png`, and
`prototype_shooting_target.png` are copied to
`build/debug/bin/resources/textures`; `prototype.level.json` is copied to
`build/debug/bin/resources/levels`; and `prototype_chair.glb` is copied to
`build/debug/bin/resources/models`. The launcher
uses the host's native process facility to discover the actual executable and
passes its adjacent resource root explicitly to the runtime. Asset loading is
therefore independent of both the current working directory and the spelling
of the invocation. Swapchain creation also validates color-attachment usage
and selects a supported composite-alpha mode from the queried surface
capabilities before calling Vulkan. One swapchain-independent, four-layer sRGB
texture array owns a complete GPU-generated mip chain, repeat/linear sampler,
and one immutable combined image-sampler descriptor. It remains one opaque
scene draw and is not a general material, asset streaming, descriptor-indexing,
or bindless system. A separate swapchain-independent RAII owner uploads the two
validated level lights once to an 80-byte uniform buffer and exposes one
immutable set-1 descriptor; the texture remains set 0. Opaque visibility uses
one device-local depth attachment per swapchain image. A 128-byte per-draw
push constant carries the camera plus the optional source-independent spot
light without mutable descriptor updates. There are no shadows, multiple
dynamic lights, file-loaded lights, fog, HDR, exposure adaptation, or general
lighting framework in this milestone. See
`docs/DEVELOPMENT.md` for run commands and the optional Vulkan smoke preset.
