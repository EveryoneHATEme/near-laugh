# Development

## Required Tools

- CMake
- Ninja
- C++ compiler
- Vulkan SDK
- Git

## Configure

cmake --preset debug

## Build

cmake --build --preset debug

## Test

ctest --preset debug --output-on-failure

## Run

./build/debug/bin/fps

## Debug Build Requirements

Development builds should enable:

- compiler warnings
- assertions
- Vulkan validation layers
- debug symbols

Warnings should not be silently ignored.

## Validation

Before completing an implementation task:

1. configure successfully;
2. build successfully;
3. run affected tests;
4. run the relevant executable when practical;
5. verify that Vulkan validation reports no new errors;
6. review the final diff.

A task is not complete solely because the code compiles.