## Why

The prototype's primary action currently exercises rifle, raycast, damage, and target-feedback systems, while the intended experience now needs darkness navigation instead. Replacing shooting with a controllable flashlight also establishes the first dynamic spot-light path without tying the renderer's light description to the player or to flashlight gameplay.

## What Changes

- Add a source-agnostic spot-light description with world-space position, direction, range, cone, color, and intensity that can be produced by the player flashlight now and by another gameplay object in a future requirement.
- Add one camera-mounted flashlight, initially off, that toggles once per captured left-click press and follows the interpolated first-person view.
- Preserve left click as the cursor-recapture action while the cursor is released; the recapture click does not toggle the flashlight and must be released before a later toggle.
- Add the active spot light to the existing textured Lambert lighting while retaining both immutable level-authored point lights and the near-black ambient contribution.
- **BREAKING** Remove prototype rifle firing, recoil, static shooting raycasts, target health/damage, hit highlighting, and destroyed-target dimming from runtime behavior and render requests.
- Keep the three target plates as inert textured and collidable prototype geometry; deleting or redesigning the range scenery is outside this change.
- Exclude a visible flashlight model, batteries, flicker, audio, volumetric beams, spot-light shadows, and multiple simultaneous dynamic spot lights.

## Capabilities

### New Capabilities

- `spot-lighting`: Defines a gameplay-source-independent, finite cone light and its contribution to the opaque prototype scene.
- `player-flashlight`: Defines the one local player's camera-mounted, left-click-toggled use of the spot-light capability.

### Modified Capabilities

- `hitscan-weapon`: Removes the automatic rifle, authoritative hitscan shot, and recoil requirements.
- `shooting-targets`: Removes mutable target health, damage, hit feedback, and destroyed-state presentation requirements while leaving the plates as inert scene geometry.
- `physics-simulation`: Removes the shooting-only backend-neutral static ray query from the physics boundary.
- `prototype-scene`: Retains three inert textured and collidable plates but removes their gameplay target descriptions.
- `scene-texturing`: Removes target highlight/dimming presentation and updates the textured appearance contract for point-plus-spot lighting.
- `runtime-composition`: Replaces fixed-step shooting-range coordination and target presentation with immediate flashlight toggle coordination and source-agnostic spot-light frame data.
- `scene-lighting`: Extends the existing immutable point-light environment with one optional dynamic finite cone contribution.
- `vulkan-renderer`: Replaces per-frame target presentation masks with backend-neutral spot-light data while preserving the single opaque scene draw.

## Impact

- Runtime composition and input edge handling in `Engine` change from rifle/target coordination to flashlight state coordination.
- Renderer-facing frame and push-constant layouts change to carry one generic spot light instead of target presentation masks.
- The prototype fragment shader gains bounded cone and distance attenuation in addition to the existing two point lights.
- Rifle, shooting-range, and shooting-target runtime sources and their focused tests leave the build; affected input, frame-layout, shader-contract, renderer-boundary, and Vulkan smoke tests change.
- Existing Vulkan, GLFW, Jolt, GLM, texture, point-light, and descriptor dependencies remain unchanged.
