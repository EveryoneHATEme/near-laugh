## Context

The root project currently enables C++ and later acquires a C compiler when GLFW is configured. The `debug` preset fixes the generator and build type but leaves both compiler choices implicit. The existing `build/debug` cache happens to use `clang`/`clang++` 18 with the GNU-style frontend, the MSVC-compatible Windows target, LLVM binutils, and `lld-link`; that local cache is not a repository guarantee.

Compiler selection must happen before CMake enables a language, so it belongs in a preset rather than in target-level CMake code. The project also needs an explicit policy check because a cached or command-line override can supersede the intended selection. See `proposal.md` for motivation and `specs/development-toolchain/spec.md` for observable requirements.

## Goals / Non-Goals

**Goals:**

- Make the existing `debug` workflow deterministic on a fresh configuration.
- Keep compiler locations portable across developer machines.
- Validate both the root C++ code and fetched C dependencies against the same Clang policy.
- Preserve the current Windows ABI and dependency compatibility.

**Non-Goals:**

- Supporting parallel MSVC and Clang development presets.
- Replacing the Microsoft CRT, STL, Windows SDK, or ABI with MinGW or libc++.
- Adding a general toolchain abstraction or cross-compilation framework.
- Pinning a specific Clang release without a demonstrated language or compiler requirement.

## Decisions

### Select portable compiler names in the existing preset

Add `CMAKE_C_COMPILER=clang` and `CMAKE_CXX_COMPILER=clang++` to the existing `debug` configure preset. Executable names allow CMake to resolve the installation from the development environment while avoiding the current machine's `D:/LLVM` path.

An additional `debug-clang` preset was considered, but rejected because the documented `debug` workflow would remain environment-dependent and the duplicate preset would add maintenance without a supported second compiler policy. Setting `CC` and `CXX` only in shell setup was also rejected because it preserves the undocumented environment dependency.

### Enable and validate both project languages at the root

Declare both C and C++ in the root `project()` call, then stop configuration unless both `CMAKE_C_COMPILER_ID` and `CMAKE_CXX_COMPILER_ID` are `Clang`. Enabling C at the root makes the policy check occur before GLFW enables C internally and gives failures one project-owned diagnostic.

Relying only on the preset values was considered, but a reused cache or explicit override could still select another compiler. A separate CMake toolchain file was rejected because two compiler executable names and a small policy check do not justify another configuration layer.

### Preserve the MSVC-compatible target on Windows

On Windows, require the Clang compiler simulation/target information to indicate MSVC compatibility. Continue using the `clang`/`clang++` GNU-style frontend so the existing non-MSVC warning flags remain applicable. Do not inject a MinGW target, change the C++ standard library, or hard-code SDK/library directories.

`clang-cl` was considered, but it would change the frontend variant and warning-option path without improving ABI compatibility; the current `clang++` installation already targets `x86_64-pc-windows-msvc`. Explicitly forcing a target triple was also rejected because the native LLVM installation already supplies the correct host target and the policy check can reject incompatible installations.

### Treat compiler changes as a fresh-configuration migration

Document Clang as a prerequisite and instruct developers migrating an existing non-Clang `build/debug` cache to run the preset with CMake's `--fresh` option. Verification will inspect the generated compiler metadata/cache rather than infer the compiler from IDE labels such as the MSVC-compatible target name.

## Risks / Trade-offs

- [Clang executable names are absent from `PATH`] -> CMake fails during compiler discovery; documentation will name the prerequisite and show compiler/version checks.
- [An LLVM installation defaults to a non-MSVC Windows target] -> the Windows compatibility guard rejects it before targets build and explains the required ABI.
- [Existing MSVC build cache conflicts with the new preset] -> document and validate a `--fresh` reconfiguration path.
- [Clang upgrades introduce new warnings] -> keep warnings enabled and treat resulting diagnostics as maintenance work rather than suppressing them globally.
- [Enabling C at the root performs detection earlier] -> accept the small configure-time cost so the fetched GLFW C sources cannot escape the compiler policy.

## Migration Plan

1. Add portable C and C++ compiler selections to the existing debug preset.
2. Enable C at the root and add Clang identity plus Windows ABI compatibility checks before dependency configuration.
3. Update prerequisites, migration guidance, and compiler verification documentation.
4. Reconfigure the debug build with `--fresh`, verify the compiler identities and Windows target metadata, then build and run deterministic tests.
5. Run the Vulkan smoke preset where a presentation-capable desktop and validation layers are available.

Rollback consists of reverting the preset, policy checks, and documentation together, then performing another fresh configuration so the cache can select the prior environment-default compiler.
