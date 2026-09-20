## Why

Functional editor checks currently require repeated screenshots and coordinate selection, or feature-specific C++ tests. A coding agent needs to discover the current UI, send a new sequence of interactions at runtime, and verify observable results without images or a registered scenario for each feature.

## What Changes

- Add a local, opt-in automation editor built with a pinned Dear ImGui Test Engine. A small MCP process owns a temporary editor session and forwards bounded requests over inherited local pipes.
- Expose four tools: `ui_session`, `ui_observe`, `ui_execute`, and `app_inspect`. Execute ordered data-only actions and assertions against the real editor widgets through Test Engine, retaining the editor and unfinished edits between requests.
- Combine Test Engine item discovery with small UI-local metadata registrations for stable identities, widget capabilities, typed draft values and commit semantics. Read-only application snapshots distinguish document values from widget buffers and transient previews.
- Cover the standard widget families already used by the editor with shared adapters, including compound numeric/color controls, multiline text, menus, popups, lists and conditional sections. Report unsupported custom viewport operations explicitly.
- Enforce one controller, finite deadlines, fail-fast execution, input cleanup, fixture confinement, and test-only file/process policies. Ordinary game/editor builds contain no automation endpoint or Test Engine dependency.
- Document structured functional testing through `ui_test_runner` separately from authorized visual checks through `ui_driver`.

### Required acceptance

1. Add a control of a supported family to the real editor with ordinary adjacent metadata; after rebuilding the editor, discover it, change it and verify both UI and applied state through a newly supplied MCP batch. Neither MCP code nor the generic executor changes; no pre-registered scenario, image or desktop mouse operation is involved.
2. Across separate requests, observe an uncommitted numeric/text edit, commit through the widget's normal interaction, verify dirty/history state, and exercise Undo/Redo and temporary Save As/Open through real controls.
3. Demonstrate disabled, ambiguous, stale, missing and unsupported targets; a failing assertion; timeout/cancellation/disconnection; and an attempted file escape. Stop subsequent actions, report partial effects truthfully and release synthetic input.
4. Preserve ordinary editor behavior and dependency boundaries; verify the automated editor lifecycle under Vulkan validation separately from visual acceptance.

### Non-goals and follow-up boundary

No scenario catalogue as the agent interface, application-command shortcut, full DOM, reflection system, arbitrary code evaluation, shell/file service, plugin system, general scripting language or universal desktop automation. No mandatory headless renderer, screenshot/OCR/vision, visual-rendering certification, or ImGui/dependency upgrade. Version 1 targets the existing Windows development environment and a dedicated editor process; it does not attach to a user's editor. Viewport picking, surface placement, sculpting, camera gestures, gizmos, docking rearrangement, OS dialogs and launching/controlling the game process are outside v1. Standard controls configuring those modes remain observable; excluded actions never use a hidden mouse fallback. These are explicit exclusions, not completed coverage.

## Capabilities

### New Capabilities

- `semantic-ui-automation`: Session ownership, live semantic observation, dynamic real-widget execution, read-only application inspection, coverage boundaries and safe failure behavior.

### Modified Capabilities

- `development-toolchain`: Add an isolated opt-in automation build and reproducible dependency/validation workflow without changing the normal development targets.

## Impact

Implementation will touch editor composition/lifetime and GLFW input isolation, UI-local metadata, document file/process policy boundaries, CMake and test definitions, a small local MCP host, developer/architecture documentation and agent routing guidance. Runtime public APIs, level formats and normal gameplay requirements do not change. Current main persistence specs still describe v9 while the committed household implementation uses v10; this change neither reconciles that separate work nor hard-codes a document format in its protocol.

Dependency: retain ImGui `v1.92.9b-docking`; select Test Engine commit `2628e39cc0ea3a0a612d5d039543c9d4e873c720` (2026-09-15). Its engine license is Dear ImGui Test Engine License v1.04, **not MIT**; eligibility and redistribution notices must be recorded before integration. The MCP host uses the official Python MCP SDK, with all runtime dependencies pinned in `scripts/requirements-editor-ui.txt` and installed in a local virtual environment. It adds no network service or C++ runtime dependency. Build/link evidence, API limits and unresolved verification belong in `design.md`.

Implementation is in progress in the working tree. Current verification and remaining gates are recorded in `tasks.md` and `validation.md`; this change is not archived.
