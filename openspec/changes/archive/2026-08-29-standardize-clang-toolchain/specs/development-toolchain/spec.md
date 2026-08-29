## Purpose

Defines the reproducible Clang-based toolchain used by the standard development build while preserving compatibility with the project's Windows runtime and dependencies.

## ADDED Requirements

### Requirement: Standard development builds select Clang
The standard debug configure workflow SHALL select Clang for every C and C++ language enabled by the project or its fetched dependencies, SHALL resolve the compiler by a portable executable name rather than a machine-specific installation path, and SHALL identify the configured compiler as Clang.

#### Scenario: Fresh debug configuration with Clang available
- **WHEN** a developer configures the standard debug preset in a fresh build tree and the required Clang executables are discoverable
- **THEN** configuration succeeds with both the C and C++ compiler identities reported as Clang

#### Scenario: Existing environment prefers another compiler
- **WHEN** a developer configures the standard debug preset in a fresh build tree from an environment whose implicit compiler would otherwise be MSVC
- **THEN** the preset selects Clang instead of inheriting the environment's implicit compiler choice

### Requirement: Missing or incorrect compilers fail clearly
The standard debug configure workflow SHALL NOT silently fall back to a non-Clang compiler and SHALL fail with a diagnostic that identifies the Clang toolchain requirement when the required compiler cannot be selected or is not identified as Clang.

#### Scenario: Clang is not discoverable
- **WHEN** a developer configures the standard debug preset without the required Clang executables available to CMake
- **THEN** configuration fails and the diagnostic identifies that Clang is required

#### Scenario: Selected executable is not Clang
- **WHEN** the compiler selected for the standard debug workflow does not identify as Clang
- **THEN** configuration stops before project targets are built and reports the compiler-policy mismatch

### Requirement: Windows ABI compatibility is retained
On Windows, the Clang development toolchain SHALL target the MSVC-compatible Windows ABI and remain compatible with the Microsoft runtime, Windows SDK, Vulkan SDK, and fetched native dependencies; the standard workflow SHALL NOT change to a MinGW or libc++ platform target as part of this capability.

#### Scenario: Windows toolchain is configured
- **WHEN** the standard debug preset is configured on Windows
- **THEN** the selected Clang toolchain uses the MSVC-compatible Windows target and the project dependencies remain link-compatible

### Requirement: Development documentation reflects the required toolchain
The documented development workflow SHALL name Clang as a required tool, explain that the Windows build retains the MSVC-compatible ABI despite using the Clang compiler, and provide a way to verify the compiler selected in a fresh configuration.

#### Scenario: Developer follows setup documentation
- **WHEN** a developer prepares a new development environment using the repository documentation
- **THEN** the prerequisites and verification guidance are sufficient to configure the standard debug workflow with Clang without relying on an undocumented local compiler selection
