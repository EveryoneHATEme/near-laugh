## 1. Toolchain Configuration

- [x] 1.1 Add portable `clang` and `clang++` selections to the existing `debug` configure preset and verify the preset remains valid with `cmake --list-presets`.
- [x] 1.2 Enable C and C++ at the root and add project-owned Clang compiler-identity failures for both languages; verify a fresh debug configuration reports both compiler IDs as Clang and the failure diagnostics explicitly name the required toolchain.
- [x] 1.3 Add the Windows-only MSVC-compatible target guard without changing warning frontend behavior, runtime libraries, or SDK paths; verify generated compiler metadata reports Clang's MSVC simulation target and the link step remains compatible with existing dependencies.

## 2. Developer Documentation

- [x] 2.1 Update `README.md` and `docs/DEVELOPMENT.md` to require Clang, explain the Windows MSVC-compatible ABI label, document `--fresh` migration from an existing non-Clang cache, and provide compiler-selection verification commands; verify every documented command and referenced path matches the final preset behavior.

## 3. Validation

- [x] 3.1 Configure the standard debug preset with `--fresh` and inspect the resulting CMake compiler metadata/cache to verify portable Clang C/C++ selection, GNU-style frontend use, and the MSVC-compatible Windows target without a repository hard-coded LLVM installation path.
- [x] 3.2 Build with `cmake --build --preset debug` and run `ctest --preset debug --output-on-failure`; verify all targets compile under Clang with warnings enabled and all deterministic tests pass.
- [x] 3.3 Run `ctest --preset vulkan-smoke --output-on-failure` on a presentation-capable Vulkan development environment and verify validation reports no new errors, or record why this environment-dependent validation could not be performed.
- [x] 3.4 Review the final `git diff` to verify changes are limited to the Clang toolchain policy, its documentation, and this change's artifacts, with no runtime or gameplay scope expansion.
