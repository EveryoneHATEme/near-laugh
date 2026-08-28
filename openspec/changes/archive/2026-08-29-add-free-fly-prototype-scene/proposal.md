## Why

The runtime can open a window, collect FPS input, and present a Vulkan triangle, but it cannot yet demonstrate the 3D rendering path needed by the game. A small navigable test scene is the next useful vertical slice because it exercises camera motion, perspective transforms, depth, and static geometry without prematurely introducing player physics or a general world framework.

## What Changes

- Add a runtime-owned free-fly camera controlled by the existing FPS action snapshot, with mouse look, horizontal and vertical translation, sprint acceleration, pitch limits, and frame-rate-independent motion.
- Capture the cursor for camera look during the prototype runtime and provide a simple way to release and recapture it.
- Replace the fixed clip-space triangle output with a small built-in 3D test scene containing a floor, enclosing geometry, and multiple depth-separated objects.
- Pass explicit camera view/projection data through the engine-owned frame request without exposing Vulkan types outside the render module.
- Add depth image allocation, depth testing, and swapchain-dependent depth-resource recreation to the Vulkan renderer.
- Keep the scene and camera purpose-built for this prototype; do not add player physics, collision, gravity, asset loading, ECS, or a general scene framework.

## Capabilities

### New Capabilities

- `free-fly-camera`: Runtime-owned perspective camera state and deterministic keyboard/mouse navigation for inspecting the prototype scene.
- `prototype-scene`: The built-in static 3D scene and its observable composition, rendering, and navigation behavior.

### Modified Capabilities

- `runtime-composition`: Coordinate camera updates and camera frame data in the existing runtime-owned main-thread loop.
- `vulkan-renderer`: Replace the triangle-only smoke output with camera-transformed static geometry and depth-buffered rendering, including depth-resource lifetime across swapchain recreation.

## Impact

The change affects the Engine frame loop, engine-owned frame request data, runtime input consumption, renderer resource ownership, graphics-pipeline configuration, shaders, smoke tests, deterministic camera/math tests, and runtime shader/resource packaging. It may add one small cross-platform mathematics dependency if the design review selects that over project-local vector and matrix types. Public `near_laugh::Application` and `RuntimeConfig` APIs remain unchanged, and GLFW/Vulkan dependency boundaries remain intact.
