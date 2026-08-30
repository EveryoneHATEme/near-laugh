## 1. Prototype Lighting Data

- [x] 1.1 Add the concrete immutable environment-light value and accessor to `PrototypeLevel`, extend level validation for finite normalized direction and bounded non-negative intensities, and verify focused world tests accept the built-in light and reject invalid fixtures.

## 2. Lightable Scene Geometry

- [x] 2.1 Extend the prototype scene vertex data with a floating-point world-space normal, assign the six outward axis normals during box expansion, and verify focused scene tests cover unit length, face consistency, and outward orientation at every authored solid bound.
- [x] 2.2 Update the Vulkan vertex binding and attribute descriptions for the expanded format, add offset/stride assertions, and verify the vertex-layout tests match all position, color, and normal fields.

## 3. Lighting Pipeline

- [x] 3.1 Define the aligned standard-layout scene push-constant payload for the camera and immutable light, expose it to the required shader stages, and verify compile-time and unit checks cover offsets, total size, stage flags, and the Vulkan 128-byte minimum limit.
- [x] 3.2 Update the scene draw path to populate and push the level light with each camera frame while preserving one vertex-buffer bind and one draw call, and verify renderer/source boundary tests still enforce backend-neutral runtime and world interfaces.
- [x] 3.3 Update the prototype vertex and fragment GLSL for world-space normal forwarding plus bounded Lambert and ambient shading, regenerate both committed SPIR-V files with the Vulkan SDK compiler, and verify shader resource tests and a debug build consume the packaged outputs successfully.

## 4. Validation and Documentation

- [x] 4.1 Update rendering/development documentation to describe the lit prototype output and its deliberate exclusions, and verify repository searches no longer describe the current fragment path as unlit flat-color output.
- [x] 4.2 Configure and build the debug preset, run `ctest --preset debug --output-on-failure`, and verify all deterministic tests pass.
- [x] 4.3 Run `ctest --preset vulkan-smoke --output-on-failure`, launch the FPS when a desktop Vulkan session is available to inspect face orientation and readability, and verify no error-severity Vulkan validation messages or visual normal inversions occur.
- [x] 4.4 Review `git diff` for scope, CPU/GPU layout consistency, generated shader assets, and unrelated refactors, and verify the final change remains limited to the approved lighting milestone.
