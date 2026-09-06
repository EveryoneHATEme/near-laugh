# Validation — 2026-09-06

## Automated results

- `cmake --preset debug`: passed with Clang 23.1.0, GNU frontend and Windows
  MSVC ABI. Configuration required the installed Ninja executable outside the
  filesystem sandbox.
- `cmake --build --preset debug`: passed for game, editor, tests, and packaged
  resources. Existing Vulkan aggregate-initializer warnings remain.
- `ctest --preset debug --output-on-failure`: 197/197 passed (190 unit tests,
  six boundary tests, one resource-layout process test).
- `ctest --preset vulkan-smoke --output-on-failure`: 5/5 passed, including
  expected failure paths for injected validation errors and partial editor
  construction. Normal paths completed without error-severity validation
  messages. Tests exercise all point/spot enable combinations, editor switch
  edits and initial-state preview, terrain replacement, resize, recovery, and
  minimize/restore.
- Both GLSL stages were rebuilt with `glslc --target-env=vulkan1.3` and passed
  `spirv-val --target-env vulkan1.3`. Tests inspect the packaged binaries'
  push-constant offsets against the 128-byte host layout.
- `git diff --check` and `openspec validate add-light-switch --strict`: passed.

Local detailed logs are `build/light-switch-debug-tests.log` and
`build/light-switch-vulkan-tests.log`.

## Manual status

Tasks 6.4 and 6.5 remain unchecked. The game launched, and a verified window
capture showed the pale plate and contrasting rocker on the central obstacle
(`build/switch-visible.png`). Reliable foreground ownership could not be
maintained: a subsequent attempt was refused by the desktop helper's focus
guard. No manual off/on, held-key, reach/miss/obstruction, flashlight,
recapture, restart, or interactive editor workflow result is claimed.
The validation game was closed through its own window handle.

The source and executable-adjacent level files retained matching SHA-256
`42DBC0E16404BF99E8AE1C586EEA567C7F37C82AAA86FA224B8D1AD0CF86C3E2`
before and after the desktop attempt. The game log contained two warnings
from external overlay layers advertising Vulkan 1.2, with no error-severity
validation messages.

Deterministic tests cover the interaction and editor behaviors above,
including real ImGui controls and version-2/version-3 authoring round trips.
They do not replace the requested manual checks. Finish tasks 6.4 and 6.5 in
an available desktop session before archiving this change.
