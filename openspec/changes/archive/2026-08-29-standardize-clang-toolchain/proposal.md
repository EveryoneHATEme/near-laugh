## Why

The documented `debug` preset leaves compiler selection to the invoking environment, so a fresh build may use MSVC even though the current development build uses Clang. Making the compiler choice explicit gives the project one reproducible, consistently diagnosed development toolchain.

## What Changes

- Make the documented CMake debug workflow select Clang for both C and C++ compilation without embedding machine-specific installation paths.
- Fail configuration clearly when the selected compiler is not Clang instead of silently using a different compiler.
- Retain the Windows MSVC-compatible ABI, Microsoft runtime, Windows SDK, and LLVM linker behavior; this change does not introduce a MinGW or libc++ target.
- Document the Clang requirement and how to verify the selected compiler.
- **BREAKING**: the standard debug workflow will no longer configure on a development machine that has only MSVC and no discoverable Clang installation.

## Capabilities

### New Capabilities

- `development-toolchain`: Defines deterministic Clang selection, clear configuration failure, and documentation requirements for the standard development build.

### Modified Capabilities

None.

## Impact

- Affects the root CMake presets and compiler-policy checks, plus the README and development documentation.
- Requires Clang to be discoverable by executable name when creating a fresh build tree.
- Does not change runtime APIs, gameplay, rendering behavior, shipped assets, or third-party dependency versions.
