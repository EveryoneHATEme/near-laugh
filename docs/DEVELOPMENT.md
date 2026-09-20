# Development

## Required Tools

- CMake
- Ninja
- Clang C and C++ compilers available as `clang` and `clang++`
- Vulkan SDK 1.3 or newer
- Git

## Configure, Build, and Test

The standard debug workflow is:

```sh
clang --version
clang++ --version
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Use CMake's fresh mode when the build tree contains a stale compiler or target
configuration:

```sh
cmake --preset debug --fresh
```

The preset selects the portable Clang executable names before either language
is enabled. On Windows these frontends target the MSVC-compatible ABI and use
the Microsoft runtime and Windows SDK; `MSVC` in CMake's simulation metadata
describes the ABI, not the selected compiler.

To inspect compiler selection in PowerShell:

```powershell
Select-String -Path build/debug/CMakeCache.txt -Pattern '^CMAKE_(C|CXX)_COMPILER:'
Get-ChildItem build/debug/CMakeFiles/*/CMakeCCompiler.cmake,
              build/debug/CMakeFiles/*/CMakeCXXCompiler.cmake |
    Select-String '^set\(CMAKE_(C|CXX)_COMPILER |COMPILER_ID|COMPILER_FRONTEND_VARIANT|SIMULATE_ID'
```

Both compiler IDs and frontend variants must report Clang and GNU
respectively. Windows additionally reports MSVC simulation IDs.

## Run

### Optional semantic editor automation (Windows)

This development profile uses Windows x64, Python 3.12 and the existing
C++/Vulkan tools. Install the official MCP SDK and its fully pinned dependency
closure into a local venv; no global Python/Codex configuration is changed:

```powershell
python -m venv build/editor-ui-venv
& ./build/editor-ui-venv/Scripts/python.exe -m pip install --only-binary=:all: -r scripts/requirements-editor-ui.txt
& ./build/editor-ui-venv/Scripts/python.exe -m pip check
```

The lock selects official `mcp==2.2.0` and every transitive runtime dependency.
The SDK's [low-level Server](https://py.sdk.modelcontextprotocol.io/advanced/low-level-server/)
publishes the existing JSON Schemas directly; the SDK owns MCP lifecycle and
dispatch. The bounded byte transport supplies the project's pre-parse limits.
Python only owns the test process and forwards requests; C++ resolves targets
and executes each whole batch. There are no feature-specific MCP tools.

Enable it explicitly; ordinary configurations neither fetch nor link Test
Engine:

```powershell
cmake --preset debug -DNEAR_LAUGH_UI_AUTOMATION=ON
cmake --build --preset debug --target level_editor level_editor_automation editor_automation_tests -j 4
& ./build/editor-ui-venv/Scripts/python.exe -B -m unittest discover -s tests/tools -p "test_editor_ui_*.py"
ctest --test-dir build/debug -L editor-automation --output-on-failure
```

Those tests use in-memory ImGui and process/pipe fixtures without opening an
editor window. Run normal checks too. The dedicated executable still requires
a real authorized desktop and Vulkan device; semantic success is separate
from presentation/visual acceptance. The selected change's
[validation record](../openspec/changes/add-semantic-imgui-automation/validation.md)
records the actual checks and outstanding hardware gates.

ImGui stays at `v1.92.9b-docking`. Test Engine is pinned to
`2628e39cc0ea3a0a612d5d039543c9d4e873c720` under its v1.04 license, which is
not MIT. The operator must establish an applicable license basis; this project's
record is linked above. Preserve the full copied
`bin/licenses/imgui-test-engine.txt` when distributing the optional executable.

Start the stdio host only after authorizing that test environment for the run:

```powershell
& ./build/editor-ui-venv/Scripts/python.exe -B scripts/editor_ui_mcp.py --environment authorized-test-desktop
```

The environment identifier records operator authorization; it does not create
or isolate a desktop. Omitting it makes session startup fail. Opening the host
alone launches no window. `ui_session` start accepts only `apartment-stairs` or
`household-interactions`, copies the fixture to an owned temporary directory,
and returns the session identity and reserved file slots. The executable,
resource root and startup argument are fixed. No existing editor is attached.

For Codex, merge the table from
[editor-ui-mcp.example.toml](../.codex/editor-ui-mcp.example.toml) into the local
`.codex/config.toml`, replacing `<REPO>` with `(Get-Location).Path` expressed
with forward slashes and `<AUTHORIZED_TEST_PROFILE>` with the authorized
environment ID. Run from the repository root. Do not overwrite an existing
configuration; retain its other tables. This local file is ignored by Git.
If startup is not authorized, omit both `--environment` and its value; discovery
and closed-session status still work. The profile string does not create an
isolated desktop or itself grant authorization.

Verified against the current official
[Codex MCP documentation](https://learn.chatgpt.com/docs/extend/mcp?surface=cli)
on 2026-09-18: trusted projects support `.codex/config.toml`; stdio settings
include `command`, `args`, `cwd`, `startup_timeout_sec`, `tool_timeout_sec` and
`enabled_tools`. The template allows 75 seconds per call for the 60-second
maximum batch plus bounded cleanup/transport overhead. Restart the Codex
session after editing; `codex mcp list` and `/mcp` report connection status.
No agent model settings or global user config are changed.

| Tool | Purpose |
| --- | --- |
| `ui_session` | Start, status, cancel an active request, close |
| `ui_observe` | Passive current UI records, exact scope/filter, bounded pages |
| `ui_execute` | Up to 64 data-only actions/assertions against current semantic targets |
| `app_inspect` | Explicit read-only document, selection, history, object, diagnostic and preview fields |

Requests and responses have strict schemas in `scripts/editor_ui_protocol.py`;
the C++ build generates its contract from that source. Start with scope `root`,
then use returned scope/control refs or exact `{scope,key,owner}` selectors.
Use `owner: "selection"` only when resolving against the current selection.
The observation includes supported actions, capacities, commit methods, typed
drafts and active raw text. An incomplete numeric buffer can be known text
while its previously copied numeric draft remains known. `app_inspect` reports applied state,
which may differ from an uncommitted draft. Closed/clipped/unregistered content
is explicit coverage, not proof of absence.

Execution request IDs are increasing positive decimal strings. Identical
retries return retained results; conflicting or expired IDs never replay
mutations. A failure stops the suffix and retains prior real effects. Results
distinguish `passed`, `failed`, `not_run` and `unknown`: lost execution evidence
cannot establish that a step did not run. Cleanup is `released`,
`process_terminated` or `unverified`; failed cleanup requires a new session.
No assertion on a truncated/unavailable value proves equality or inequality.
There is no automatic retry/reconnect for a lost response and no exactly-once
claim. Early cancellation is remembered by session/request ID (16 distinct
pending IDs maximum); repeated cancellation shares capacity. Overflow closes
the channel. Cancelled queued work cannot start later; cancelling startup also
closes the instance that startup acquired.

Type only returned temporary paths into the real Open/Save As UI. Restricted
saving validates/serializes before exclusive writes. It cannot overwrite an
unexpected competing file, but an I/O failure may leave partial bytes in an
owned temporary output and keeps the document unsaved. Ordinary editor saves
still use atomic replacement. Viewport picking, placement, sculpting,
navigation, gizmos, docking, OS dialogs and Play process creation are excluded.

`ui_execute` defaults to strict operation: it will not reveal a clipped target
or recover window focus implicitly. Opt in with
`"policy": {"auto_scroll": true, "auto_focus": true}` when those conveniences
are appropriate. Every step returns its `assistance` evidence. Even an assisted
packet cannot dismiss an obstructing popup, expand hidden ancestors, or
move/resize windows. Use explicit `open`, `close`, `focus` and `scroll` steps.

Numeric `edit` enters text; `commit` or `key` supplies a separate confirmation
or Escape gesture. A real drag uses `{"op":"drag","target":{"ref":"..."},
"direction":"increase","fraction":0.2,"frames":8}`. The fraction is relative
to the observed widget width, not an exact numeric assignment. Assert the
resulting document/preview value independently. Viewport and gizmo dragging
remain excluded. RGB component adapters explicitly reject other color modes.

Observation copies current Test Engine facts and UI metadata without invoking
`ItemReadAs*`. `engine.frame/status_frame` show source freshness; `input.text`,
`input_validation.status` and `value.draft` distinguish an incomplete numeric
buffer from its retained number. A fresh frame can advance application time.
Use `next_cursor` for immutable continuation pages, and increase depth or query
an observed child scope on `depth_limit`. Nested row refs are conservatively
invalidated when their array changes; re-observe or use a current selector.
Oversized labels expose `label_availability: truncated`; inactive input also
reports truncation. Prefixes cannot pass full-value assertions. Immediate
preview/placement handlers carry adjacent commit-policy metadata; Enter/Tab
remain input gestures, independently of when the handler applies a value.

For a runtime JSON-lines client with a new transcript file:

```powershell
& ./build/editor-ui-venv/Scripts/python.exe -B scripts/editor_ui_client.py --environment authorized-test-desktop --start apartment-stairs --transcript build/editor-ui-run.jsonl
```

After startup, input lines have `{ "tool": "ui_observe", "arguments": ... }`
using the returned session ID. The client supports arbitrary schema-valid
batches, not named scenarios. Both channels enforce 1 MiB messages, UTF-8,
bounded nesting and a five-second partial-line deadline. Operation defaults
are five seconds (maximum 30); batch defaults are 20 seconds (maximum 60).
Cancellation cleanup allows two seconds before owned-child termination.
Normal close also verifies the child's final exit and Vulkan diagnostics after
destruction. The host writes its bounded exit/diagnostic audit to stderr.
`diagnostic_path` identifies the retained per-session stderr file (16 MiB cap,
explicit truncation marker). Large responses instead identify a JSON result
artifact (1 MiB, up to 32 retained per host) and return compact failure evidence.
These local files survive close; remove them after reviewing evidence. Older
result artifact paths can expire. Ordinary execution returns checks, revisions,
partial changes and errors; inspect/observe are separate explicit requests.
The SDK client transcript records connection facts and actual tool payloads;
its output IDs correlate input lines and are separate from SDK wire IDs.

SDK acceptance of the real editor (requires authorization for this run):

```powershell
& ./build/editor-ui-venv/Scripts/python.exe -B tests/automation/real_editor_sdk.py --environment authorized-test-desktop --transcript build/editor-ui-sdk-integration.jsonl
```

Use a new transcript filename on subsequent runs. The driver tests initialization,
tool schemas/calls, a real UI edit and applied document value, retained duplicate
results, bad input, mid-batch failure, timeout, explicit and SDK cancellation,
incompatible hello and owned-editor death. The latter two use a test-only
fault wrapper around the real executable, with no production tool/config hook.
It retains complete tool exchanges, stderr and a `.summary.json`; normal close
requires zero Vulkan validation errors after destruction. Mock tests do not
replace this chain, and missing desktop/GPU access is **blocked**.

If an installed overlay breaks Vulkan loading, rerun the driver with
`--disable-implicit-layers` and a new transcript name. This applies
`VK_LOADER_LAYERS_DISABLE=~implicit~` only to that test host and its child;
explicit Khronos validation remains enabled and checked. The optional `env`
table in the Codex template provides the same child-local setting. This follows
the [Khronos loader layer filtering contract](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md#layer-filtering).
Retain the failed run as environment evidence rather than hiding its errors.

Prepared acceptance drivers under `tests/automation/real_editor_*.py` exercise
the real MCP/editor path. Each requires prior desktop authorization, an
`--environment` identifier and a new `--transcript` path. For example:

```powershell
& ./build/editor-ui-venv/Scripts/python.exe -B tests/automation/real_editor_acceptance.py --environment authorized-test-desktop --transcript build/editor-acceptance.jsonl
```

`real_editor_lifecycle.py` resizes/minimizes only its owned editor window;
`real_editor_failures.py` checks rejected actions and stopped suffixes;
`real_editor_widgets.py` checks the remaining workspace families;
`real_editor_shutdown.py --mode construction|file-exit|disconnect|host-death`
selects a prepared shutdown check (pass one mode). These are external test
clients, not host/executor scenario handlers. The separately built
`editor_automation_input_probe` runs as CTest `editor_automation_input_isolation`
under the `vulkan-smoke` label. It calls the owned GLFW callbacks in-process,
without OS keyboard/mouse injection, and checks camera/document isolation and
the session-local clipboard. Native WM-message/gamepad hardware and visual
appearance require separate evidence.

For a supported new control, keep its ordinary editor handler and add adjacent
metadata with an existing read-only binding. Document Summary's `Zero ambient`
button is the verified example:

```cpp
EditorWidgetMetadata::next("zero-ambient", "ambient_intensity", "", "document");
if (EditorWidgets::Button("Zero ambient"))
  static_cast<void>(editor_document.setAmbient(0.F));
```

After rebuilding `level_editor` and `level_editor_automation`, discover its
returned ref with `ui_observe`, send `activate`, and assert the document's
`ambient_intensity` with `app_inspect` or a typed batch assertion. The ordinary
Undo command restores the previous value. No host schema or executor handler
changes are needed. The prepared `real_editor_extension.py` driver runs this
example with the same `--environment` and `--transcript` arguments above.

### Ordinary launch

On Windows:

```powershell
.\build\debug\bin\near_laugh.exe
.\build\debug\bin\near_laugh.exe --level .\resources\levels\apartment-stairs.level.json --entry lower-landing
.\build\debug\bin\level_editor.exe
.\build\debug\bin\level_editor.exe .\resources\levels\prototype.level.json
```

On other supported desktop environments, use:

```sh
./build/debug/bin/near_laugh
./build/debug/bin/near_laugh --level ./resources/levels/apartment-stairs.level.json --entry lower-landing
./build/debug/bin/level_editor
./build/debug/bin/level_editor ./resources/levels/prototype.level.json
```

Both launchers derive the resource root from the actual executable path, not
the working directory or `argv[0]`.
The game accepts `--level <path>` and `--entry <id>` independently. Defaults
are the packaged prototype and the selected level's authored default entry.
Relative level arguments resolve against the invoking directory once; quote
paths containing spaces. Unknown, repeated, missing, and empty options fail
before application startup. Native Unicode paths are preserved. Other assets
always come from the executable's resource root.

## Current Packaged Resources

The build copies the following layout beside `near_laugh`, `level_editor`, and
the relevant smoke/process executables:

```text
resources/
  levels/prototype.level.json
  levels/apartment-stairs.level.json
  levels/audio-captions.level.json
  levels/household-interactions.level.json
  levels/household-baseline.level.json
  levels/household-capacity.level.json
  audio/*.wav
  captions/*.captions
  fonts/NotoSans-Regular.ttf
  fonts/OFL.txt
  characters/test-mannequin.glb
  characters/catalog.json
  characters/LICENSE-Quaternius.txt
  models/prototype_chair.glb
  shaders/prototype_scene_vertex.spv
  shaders/prototype_scene_fragment.spv
  shaders/point_shadow_vertex.spv
  shaders/point_shadow_fragment.spv
  shaders/caption_vertex.spv
  shaders/caption_fragment.spv
  textures/prototype_floor.png
  textures/prototype_boundary.png
  textures/prototype_obstacle.png
```

Levels write format version 10 and read exact versions 2–9 without modifying
the source. The profile contains optional 97-by-97 terrain, 1–240 solids,
1–16 entries/default, 0–8 lights/ambient, 0–128 props, 0–16 switches,
and 0–32 doors, plus audio (up to 128 cues, 64 sources, 32 rooms and 64 connections).
The required `characters` object contains arrays of up to four actors, 32 marks
and 16 routes. Earlier packaged scenes retain empty character arrays; the
one/four-actor scripted-character fixtures exercise authored initial routes.
The editor renders initial idle poses and supports character list/property
commands, surface placement and explicit silent clip/schematic route inspection.
The required `household` object contains up to 16 physical boxes, 32 readable
documents and eight radio controls. All earlier packaged scenes retain empty
household collections. The neutral household scenes contain 4, 0 and 16 boxes
respectively; baseline/capacity retain the same document, radio, actor and lights.
Prop/model/material and clip/caption IDs are logical names, never paths.
Props have finite translation/yaw, positive uniform scale and 0–8 local boxes.
Legacy chair/texture roles normalize to explicit legacy identities; v5 doors
survive migration. Versions 2–6 map to empty audio. New fields in old versions,
unknown fields and v1 fail. Legacy lights map to `point-light-0/1` without
shadows, and the singleton switch maps to `light-switch-0`; its initial
enable moves to the linked light. v7 audio is preserved. Exact v8 inputs preserve
all prior values and normalize to empty characters, as do v2–v7 after their
existing migrations. Versions 2–8 reject character fields. Exact v9 retains
all authored character data and order; versions 2–9 add empty household arrays
and reject household fields in their original shapes. Older builds cannot
read v10: use Save As or retain
the original before conversion when it is still needed by an older build.

The selected apartment derivatives are `models/apartment_chair.glb`,
`apartment_table.glb`, `apartment_phone.glb`, `apartment_radio.glb`, plus
`textures/apartment_wood_floor.png` and `apartment_wallpaper.png`.
See [asset preparation/provenance](../resources/models/APARTMENT_ASSETS.md).
The raw `house_interior_pack/` is local source material, excluded from Git by
the root `.gitignore` and not copied by builds. A checkout needs only the
prepared files in `resources/`; the full pack is needed only for regeneration.
Only models/materials referenced by the chosen level are loaded.

Structural materials are independent of collision kind; terrain has one
whole-surface material. Generated geometry keeps world-scaled UVs, while props
keep authored UVs. Apartment textures use nearest sampling; legacy textures
retain linear sampling. Phone cord alpha uses MASK cutoff 0.5; radio is OPAQUE.
All surviving fragments use the bounded authored point lights/flashlight. There is
no PBR, emission, blended transparency or texture-paint workflow.

## Current Game Controls

The prototype starts with the cursor captured:

- mouse: look;
- W/A/S/D: move relative to view;
- Left Shift: sprint;
- Left Control: crouch;
- Space: jump while grounded;
- E: operate the nearest door, switch, document, radio or box within 2 metres;
  while holding a box, drop it;
- R: lock/unlock a fully closed, stationary door from its authored bolt side;
- right mouse: knock once on the nearest door, or throw the held box;
- A/D while reading: previous/next page, one change per press;
- E/Escape while reading: close the document and retain cursor capture;
- Escape outside reading: release the cursor;
- left mouse button: toggle the flashlight while captured, or recapture the
  cursor while released.

Reading keeps the current stance and suppresses movement commands, look,
flashlight and object actions while gravity, collision, doors, actors, boxes
and audio continue. Held controls require release after mode transitions.
Pickup/drop/throw apply once before the next world step; a zero-step frame
retains the pending request. Cursor release owes a drop at that boundary.
Minimize or explicit suspension cancels pending commands but retains an existing
hold and any safety drop already owed until the next active step.

Ordinary launches with any nonempty household collection enable **P** for
development suspension and **M** for mute, except when using the explicit P04
audio or P07 character fixture controls. These are development controls, not
the future session menu. Mute keeps the cue clock and captions running;
suspension freezes world, audio and feedback time together.

A recapture press is suppressed until release so it does not also toggle the
flashlight. Movement uses fixed-step gravity and static/accepted door collision, slides along
walls, traverses the authored low step, and checks standing clearance beneath
the low passage. These are current prototype behaviors, not permanent product
requirements.

The packaged switch is the pale plate on the central obstacle facing spawn.
Walk forward from spawn to reach it. It controls Point light 1, initially on,
and adds no collision body. Terrain, solids, all prop boxes, door leaves,
actor proxies and household boxes block interaction. E/R/knock require a release before the first press and between
presses; holding it through a miss, cursor transition, or minimization cannot
trigger a later toggle. The light state persists through presentation recovery
and resets on restart without modifying the level file. Flashlight controls
and ambient remain independent.

## Current Editor Behavior

The standalone editor uses right mouse for scene navigation, Escape to release
navigation, mouse movement to look, W/A/S/D for horizontal movement,
Space/Left Control for vertical movement, and Left Shift to sprint. UI capture
suppresses conflicting camera input.

File > Open and File > Save As use explicit path-entry dialogs. Opening is
transactional; Save and Save As use the shared validated deterministic codec;
and dirty New, Open, Close, or Exit requests require Save, Discard, or Cancel.
File > New Interior creates a valid starter floor, default entry, two lights
and ambient 0.12, with no terrain, props, doors or switch. It begins
dirty and unsaved; Save uses Save As until a path has been chosen. Legacy files
open clean with a migration notice. Safely decoded gameplay-invalid files
remain editable with diagnostics; malformed or unsafe files preserve the
current document.

The Objects panel and left-click viewport picking share one selection. Solids
can be added, duplicated, deleted, and edited. Entries can also be added,
duplicated, renamed, and moved. IDs match `[a-z][a-z0-9-]{0,63}`; new entries use
the first unused `entry-N`. Make default changes the authored startup entry.
Renaming it updates the reference in one undoable edit. Choose another default
before deleting the default entry; the last entry cannot be deleted. Lights
and switches support add/duplicate/delete with durable IDs and limits of 8/16.
Props and doors support independent add/duplicate/delete, durable
IDs, finite property edits and undo/redo. Removing the last prop is valid.
Changing a prop model preserves its boxes; Reset model collision boxes is
explicit. Yellow render bounds and cyan proxy bounds distinguish appearance
from collision. Missing model references retain red selectable markers.
Ambient is editable in [0, 0.20], including zero. Terrain layout remains
read-only; terrain material is separate.

**Add point light** creates an enabled, unshadowed source. Light properties
include ID, position, RGB, intensity, radius, Initially on and Casts shadows.
The summary shows the four-caster budget, which includes disabled sources.
Duplicating a caster may exceed that budget; the repairable document remains
editable, with Save/Play blocked and a clearly stale coherent preview.
Renaming a light updates every incoming switch link in one undo step.
Deleting it keeps broken links visible for repair.

**Add light switch** creates a plate near the first entry at standing
interaction height, linked to the first light, or an explicit broken link in
a lightless document. Select a plate in Objects or the viewport. Properties
expose ID, Position, Yaw and Linked light by durable ID. Duplication preserves
the link. Selected lights show ranges/incoming links; selected plates show
their outgoing link. Numeric edits and surface mounting share undo/redo and
dirty state. Preview uses light initial values, independent of relinking or
removing switches. The editor does not run E interaction.

Properties expose solid geometry/tint/kind/material, entry ID/pose, lights,
prop identity/model/transform/box list, and door ID, bottom hinge, closed yaw,
leaf dimensions, opening angle/speed, lock side and initial state. Drag numeric
values or Ctrl-click to type. Finishing a field commits one undo step. Unsafe
fields retain their old values; safe cross-field errors remain repairable and
block Save/Play. Door overlays show hinge, opening arc and lock side.

**Objects > Household** provides **Add box**, **Add document** and **Add radio
control**, with separate list selection and 16/32/8 limits. Box properties edit
ID, initial center and yaw; documents edit ID, position, yaw, title and ordered
pages. Enter adds a page line; Ctrl+Enter or finishing the field commits the
whole draft. Previous/Next page navigates; Add/Remove page changes the list in
one undo step. Limits count Unicode scalars, not UTF-8 bytes: 80 for title,
480 per page and at most 16 pages. The readable preview uses runtime wrapping
at 800x600; glyph/fit errors retain a clearly stale last valid preview. Empty
text and broken links remain repairable, while unsafe field values are rejected.

Radio properties expose ID, prop/source links and **Initially on**. Use
**Select radio prop** to edit or place its static model. Each radio exclusively
owns one `apartment-radio` prop without collision boxes and one non-autoplay,
captioned spatial ambience loop, unowned by actors or other radios. Prop/source
rename updates all incoming radio links in one undo step; deletion leaves
broken references. Duplicate keeps outgoing links, including visible ownership
conflicts, and undo restores the prior state. Generated IDs skip retained
broken links instead of reconnecting them accidentally. The editor previews
authored household poses and radio state without running hold/throw or playback.

Enable **Place on surface** with an object selected. **Scene surfaces** uses
the nearest structural face or terrain triangle and displays the target,
face, elevation, and normal. It excludes the moved solid. **Terrain only**
retains terrain placement and is unavailable when terrain is absent.
On top surfaces, solids rest their bottom at the hit, entries place their
feet, and props place their translation anchor. Doors place their bottom hinge
with the visible floor-clearance offset (default 0.02 m). Lights and switches use the visible
height offset (initially 2 m and 1.4 m, or the previous terrain offset).
Vertical faces support solids, lights with a visible outward offset, and
switches with their back 1 mm outside the wall and front aligned outward.
Boxes and document panels rest on upward structural/terrain surfaces, keeping
their fixed thickness and yaw. Their placement does not target prop proxies.
Radio controls have no independent placement; select their linked prop.
Entries, props, doors, household boxes and documents cannot be wall-mounted.
Undersides block placement;
the editor never searches through an unsuitable nearer face. Escape, a miss,
UI capture, or navigation cancels/suppresses placement without an edit.
Yellow bounds identify selected geometry; sphere markers identify lights and
entries. Editor overlays remain visible through scene geometry.

Editor shortcuts (suppressed during camera navigation, active field editing,
or modal dialogs):

- Ctrl+Z: undo; Ctrl+Y or Ctrl+Shift+Z: redo.
- Ctrl+D: duplicate the selected supported object. Actor duplication includes
  an independent initial mark and clears route/sound links; route duplication
  preserves its owner and ordered links. Named records receive new durable IDs.
- Delete: remove the selected supported object, except the default/last entry.
  Incoming links remain visible for repair or undo.
- Ctrl+S: save the current valid document.

History retains up to 128 committed edits and clears on document replacement.
Undoing back to the saved state clears dirty state; editing after undo drops
the redo branch. Finite edits that violate level constraints, such as entry
overlap, remain visible and editable with validation diagnostics. Save is
disabled until correction or undo restores validity. Unsaved-close dialogs
still offer Discard and Cancel when the document is invalid.

Under Properties > Terrain, enable **Sculpt terrain** and select Raise, Lower,
or Smooth. Left-drag the scene to apply the brush; Escape returns to object
selection. Object placement and sculpting are mutually exclusive. UI controls
and camera navigation suppress brush input. The yellow outer ring shows the
radius; inner rings show falloff strength and follow the heightfield. Arcs
outside the terrain are omitted.

- Radius: 0.5 to 8 metres.
- Raise/lower strength: 0.01 to 1 metre per stamp.
- Smooth strength: 0 to 1; zero leaves the terrain and history unchanged.
- Falloff: 0 to 1, blending constant interior influence toward smoothstep
  attenuation. Samples exactly on the radius remain unchanged.

Drag controls or Ctrl-click to type values. Non-finite or out-of-range typed
commits retain the previous valid value and report the field. A stroke captures
its settings on press and stamps at 0.25-metre intervals along the observed X/Z
pointer path. Holding still adds no stamps. A terrain miss breaks the path;
returning to terrain starts a new segment within the same gesture. Smoothing
uses a pre-stamp 3-by-3 neighborhood with [1, 2, 1] weights in each axis and
clamped border coordinates.

Each modifying gesture shares the 128-entry history with object edits. Undo
restores the whole stroke, selection, and brush settings. Mesh preview updates
during the stroke, and full validation runs when it ends or is undone/redone.
Entering UI, starting navigation, minimizing, or requesting close ends the
current stroke. Invalid slope triangles appear in red; Validation lists their
zero-based cell X/Z and triangle 1 or 2. Repair with lower/smooth strokes or
undo. Terrain strokes revalidate all entries. If a stroke leaves an entry
unsupported, place it back on a suitable surface or correct its numeric pose.
Clear upper-floor entries remain supported by their authored structural floors.
Terrain tools and footprints clear when switching to an interior.

Brushes edit only the fixed 97-by-97 height samples. They do not change layout,
material assignments, props/doors, or runtime collision and cannot author holes, caves,
overhangs, voxel terrain, paint, procedural terrain, or erosion.

In **Playtest**, choose **Start entry** and **Play**. The selection is editor
state and does not change the authored default or dirty state. Play finishes
pending edits and validates all entries. Dirty work requires **Save and Play**
or **Cancel**; unsaved work then uses Save As. A fresh read must match the
prepared editor document before launch, including a second freshness check after
selected resource preparation. Required selected assets are decoded before
creating the child; a missing/unsupported asset launches nothing. If the disk file changed externally,
explicitly Save or Open it and try again. Errors and canceled dialogs launch
nothing and leave no deferred request.

Saved-file Play also checks every document's glyph coverage and complete layout
with the currently selected trusted font, plus linked radio audio/captions,
before creating a child. Text diagnostics identify the document and page.

The editor starts the sibling game with the saved absolute path and chosen
entry. One game child can run at a time. Authoring remains available; process
creation and eventual exit status appear in Playtest. Closing the editor
leaves the game running independently. Saved edits do not change that run.
Keep authored files at their own paths; playing them does not require changing
the packaged prototype. Shared resource copying runs once per build for the
executables in `build/<preset>/bin`.

The M1 blockout has upper floors at Y=3 and the lower landing at Y=0. From
`apartment`, leave Lena's room through the east doorway into the corridor;
the kitchen is east of the corridor at Z=-2.3. Follow the corridor north to
the rear stairs at Z=-6 and descend to Z=-16.2. `lower-landing` faces back
upstairs. Fourteen 0.6 m treads give fifteen 0.2 m rises in a 1.8 m-wide stair.
Open Lena's room door with E before crossing, using the doorway near Z=3.5.
The hinge is (-1.12, 3.02, 3.07), width 1.26 m, closed yaw -90 degrees and
opening angle -90 degrees. Roughly 4 cm jamb clearance accommodates the
conservative angular query; the leaf opens west into the room. Positive local Z
puts its bolt on the room side. E reverses mid-swing; blocked movement stops
until another E press. Refusal/knock geometry lasts 0.3 seconds, without sound.

The temporary switch post is at (-0.65, 3.7, 4.05); its plate is
(-0.721, 4.3, 4.05). Open the door from the apartment start before approaching
the switch-view position: standing in the swing arc correctly stops its motion.
From room feet around (-2.3, 3, 4.05), aim down slightly
toward the plate: the closed leaf blocks it, and opening permits interaction.
The kitchen table is at (2.6, 3, -4); phone and radio sit on top. Cross the
kitchen doorway at Z=-2.3, then turn toward Z=-1.9 to pass the nearer chair.
Both authored starts support the full ordinary-walking route after opening.
These are temporary acceptance positions, not narrative/progression content.

## P04 audio and caption fixture

Run `build/debug/bin/audio_captions_fixture.exe` from any working directory.
Add `--silent` to exercise the same sequence without opening an output device.
The executable explicitly selects the packaged `audio-captions.level.json`:
radio and localized ring, moving corridor footsteps, a complete Russian phone
call, two seconds of silence, then the contradictory invitation behind the door.
The ordinary game does not activate this sequence from the level filename.

Fixture-only controls (while the cursor is captured): F5 cancels all instances
and restarts, M toggles mute, P suspends/resumes world and cue time together. Restart retains mute
and pause settings. Existing E/R door controls and camera navigation remain
available; Escape releases the cursor without pausing audio. Minimize suspends
before waiting, and restore preserves cue offsets. There is no save-game or
P05 narrative state in this fixture.

In the editor, use **Audio authoring** to add/list cues, sources, rooms and
connections; **Properties** edits the selected record. Source placement uses
**Place on surface**, height above the floor and wall offset. Room wireframes
are selectable on their edges. Selecting a connection draws its room/door links.
Broken references remain visible for repair and block Save/Play. Rename and
reference changes share one undo step; deletion does not cascade.

Select a source and use **Audio audition → Start audition**. Stop, Mute and Pause
operate on a validated snapshot; camera movement updates the listener without
dirtying the file. The panel shows source/listener regions, effective gain,
Russian captions and device warnings. Any edit, undo/redo, replacement, minimize
or Play request stops audition. Restore and selection do not restart it.

The shared packaging target copies `audio/`, `captions/`, `fonts/` and compiled
caption shaders beside game, editor, demo and smoke binaries. See
[audio preparation, hashes and provenance](../resources/audio/README.md) and
[trusted font provenance/license](../resources/fonts/README.md). Regenerate the
fixture level with `python scripts/prepare_audio_level.py`; audio generation has
separate explicit eSpeak NG/FFmpeg requirements documented with the assets.

Recompile and validate caption shaders after editing them:

```sh
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/caption_vertex.glsl -o resources/shaders/caption_vertex.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 resources/shaders/caption_fragment.glsl -o resources/shaders/caption_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/caption_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/caption_fragment.spv
```

Deterministic tests render real offline PCM and run the complete sequence in
audible, muted and silent modes. Vulkan smoke adds changing captions, empty
frames, attachment-format changes, atlas retention, allocation failures,
runtime minimize/restore/close, and editor audition/failed Play preflight.
Both validation sinks remain alive through final GPU destruction.

For listening acceptance, use headphones/speakers and record output hardware,
distinct source positions, listener rotation, moving footsteps, closed/open/
obstructed-door gain, ducking, pause/minimize/resume, shutdown, Russian speech
intelligibility and the completed-call contradiction. Measure output latency
and caption/audio drift against 100 ms plus measured device latency, stating the
measurement method. Device-free cursor tests are not that measurement. Repeat
muted and `--silent`, inspect captions at 800x600 through 3840x2160 and HiDPI,
and author/save/audition/Play from another working directory. Record observations
and unavailable checks in the [P04 validation record](../openspec/changes/archive/2026-09-07-add-spatial-audio-and-captions/validation.md).

## Build Targets

- `near_laugh_platform`: GLFW windowing and physical input collection.
- `near_laugh_world`: version-10 level data with exact version-2/3/4/5/6/7/8/9 read compatibility,
  private JSON codec, validation, and immutable runtime handoff.
- `near_laugh_physics`: Jolt lifetime, static proxies, accepted kinematic doors,
  up to four catalog actor capsules, 16 dynamic boxes, one hold constraint and
  one virtual player character with its owned inner capsule. The
  fixed-step caller advances the shared world before participant movement.
- `near_laugh_render`: Vulkan renderer, resource loading, and immutable scene
  GPU ownership.
- `near_laugh_runtime`: application facade, composition, player input,
  player/flashlight and household policy, fixed-step coordination, and main loop.
- `near_laugh_audio`: bounded PCM/caption preparation, miniaudio playback,
  authored room/door transmission, cue coordination and compiled P04 fixture.
- `near_laugh_text`: trusted font validation, atlas baking and shared caption/readable layout.
- `near_laugh_animation`: bounded animated GLB decoding, deterministic TR
  sampling/transitions and CPU deformation; one compiled cgltf owner is shared
  with the existing static loader.
- `character_animation_viewer`: explicit P07a animation inspection and timing.
- `scripted_characters`: P07b route development controls through the runtime.
- `scripted_character_measure`: opt-in Release route comparison and short
  Debug/Release timing-recovery checks.
- `household_measure`: opt-in baseline/16-awake-box Release comparisons and
  Debug/Release workload/readback checks with actual fixed-boundary counts.
- `audio_captions_fixture`: explicit P04 demo using the internal runtime entry.
- `near_laugh`: game launcher linking only `near_laugh_runtime`.
- `near_laugh_editor_core`: document workflow, play preparation, native child
  ownership, and camera.
- `near_laugh_editor_ui`: Dear ImGui integration and workspace.
- `near_laugh_editor_render`: editor Vulkan rendering and active-document GPU
  replacement.
- `level_editor`: standalone authoring application.

## Vulkan Smoke Validation

The deterministic `debug` preset excludes window/GPU-dependent tests. Run both
game and editor Vulkan smoke paths explicitly:

```sh
ctest --preset vulkan-smoke --output-on-failure
```

The smoke paths exercise normal rendering, resize, minimize/restore,
swapchain recovery, partial construction, and orderly shutdown. They verify
that immutable texture, lighting, and mesh resources survive recovery and that
error-severity Vulkan validation messages fail the test. A desktop session and
a Vulkan 1.3 presentation-capable device are required.

The editor smoke also exercises object duplication/removal, undo, invalid
entry preview and refused saving, canceled close, light/prop edits, and
semantic save/reload using a temporary level copy. Deterministic UI tests drive
real ImGui button, shortcut, capture, and numeric-drag behavior without a GPU.
Terrain smoke coverage includes active multi-stamp strokes, coalesced buffer
replacement, smoothing, undo/redo, sculpted save/reload, and resize/minimize
recovery followed by an unsaved exit decision.
Interior smoke covers both named starts, runtime selected-entry failures,
terrain/interior replacement, an empty world mesh, and failed replacement
followed by rendering with the prior resources. Furnished-scene smoke changes
door open/lock previews, duplicates/deletes/restores shared-model placements,
and exercises changing geometry through fenced frame slots and recovery. Deterministic tests exercise
the saved-file Play transaction and a real native child argument probe.

Light-switch coverage includes all point-light/spotlight enable combinations,
editor add/remove and link changes, initial-state preview, and terrain rebuilds.
After scene/shadow shader changes, regenerate and validate the changed stages:

```sh
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/prototype_scene_vertex.glsl -o resources/shaders/prototype_scene_vertex.spv
glslc -O -fshader-stage=frag --target-env=vulkan1.3 --target-spv=spv1.5 resources/shaders/prototype_scene_fragment.glsl -o resources/shaders/prototype_scene_fragment.spv
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/point_shadow_vertex.glsl -o resources/shaders/point_shadow_vertex.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 --target-spv=spv1.5 resources/shaders/point_shadow_fragment.glsl -o resources/shaders/point_shadow_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/point_shadow_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/point_shadow_fragment.spv
```

Both material fragment stages deliberately target SPIR-V 1.5: GLSL `discard` lowers to
`OpKill` without requiring the optional `shaderDemoteToHelperInvocation`
device feature. Keep the Vulkan 1.3 runtime feature baseline unchanged.

For visual inspection, confirm stable one-metre texture scale across face
orientations, outward-normal lighting, mip stability at distance, independent structural materials, the phone cord cutout, the dark transition between authored light pools,
flashlight cone/range behavior, chair rendering and collision, and persistence
through resize/recovery.

## Completion Checks

The opt-in P10 timing executable is excluded from the default build. Build
`interior_lighting_measure` explicitly in a separate Clang/Ninja Release tree
for performance measurements; use the Debug target's `check` mode for Vulkan
validation of query readback and recovery. It requires an available primary
monitor mode of 1920x1080 at 60 Hz and switches its own window to fullscreen.
Closing the window or interrupting its framebuffer invalidates a measurement.

```powershell
cmake --build build/p10-release --target interior_lighting_measure
.\build\p10-release\bin\interior_lighting_measure.exe resources/levels/apartment-stairs.level.json stationary build/baseline.csv
python scripts/summarize_lighting_timings.py build/baseline.csv
.\scripts\measure_interior_lighting.ps1 -OutputDirectory build/p10-final-measurements
```

The output path must be new. Each `stationary` or `route` invocation records
10 seconds warm-up and 60 seconds sampling and closes automatically. `route`
drives the furnished interior's ordinary walking, both doors, switch and
flashlight actions; it is a measurement fixture, not gameplay inferred from a
filename. `check` records 40 frames with explicit swapchain recovery and is not
a performance run. CSV writing occurs after GPU teardown and retains all raw
rows; summaries use nearest-rank percentiles after warm-up. Retained evidence
may use lossless `.csv.gz` files, which the summarizer also reads. Blank GPU fields
mean unavailable timing. See the current
[P10 validation record](../openspec/changes/archive/2026-09-07-add-interior-lighting/validation.md)
for hardware, timing scopes, exact commands, results and acceptance status.

The batch script runs three stationary and three route samples for each
packaged six/eight-light scene, plus one equivalent unshadowed stationary and
route baseline for each. It writes baseline copies with only `casts_shadows`
disabled and retains per-run CSV/logs and `summary.json`. Run no concurrent
game/editor, build or GPU tests during performance sampling. Run this GUI
measurement on the ordinary interactive Windows desktop. An agent's isolated
execution environment can change presentation pacing; use its approved desktop
execution mode and retain failed diagnostic runs separately. The local T1
gates are CPU/GPU p95 <=16.67 ms and frame p50/p95/p99 <=16.9/20/33.4 ms in
every run. Preserve failures and inspect separated waits and action costs.

`interior_lighting_visual` is a separate opt-in GPU readback target. It checks
fixed 1920x1080 views, actual player-obstructed/reversed door poses, independent
flashlight state and matching editor/runtime initial values. Lossless PNGs
retain actual stored framebuffer RGB; they are not performance measurements.
The optional isolated GLB controls verify texture alpha, factor/cutoff and
both-sided OPAQUE/MASK shadow coverage without collision proxies.

```powershell
cmake --build --preset debug --target interior_lighting_visual
python scripts/prepare_lighting_material_fixture.py build/material-controls
.\build\debug\bin\interior_lighting_visual.exe build/lighting-captures build/material-controls
python scripts/analyze_lighting_captures.py build/lighting-captures --prune-ppm
```

Both output directories must be fresh. The analyzer can rerun directly from
its PNG files. `--prune-ppm` removes redundant PPMs only after verifying an
exact RGB round trip. D16 fallback validation uses the existing test control
`NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE=shadow_d32_unavailable`; clear it before
ordinary runs. Reproduce the packaged lighting scenes with
`python scripts/prepare_interior_lighting.py`; the historical level migration
and audio preparation scripts also emit deterministic v10 data. For packaged
v8 inputs, run `python scripts/level_characters_v9.py` followed by
`python scripts/level_household_v10.py`. The latter accepts exact version
markers 9/10 only, adds empty household arrays to v9 while retaining every
prior authored field, and preserves v10 household content;
retained compatibility fixtures under `tests/fixtures/levels` stay unchanged.

Before reporting an implementation complete:

1. configure the affected preset;
2. build the affected targets;
3. run affected deterministic tests;
4. run Vulkan smoke validation when rendering, resources, windows, or lifetime
   behavior changed;
5. review `git diff` and current OpenSpec validation;
6. report any step the environment could not perform.

Compilation alone is not behavioral validation.

## P06 Household Interaction Checks

Run the reproducible verification from the repository root:

```powershell
powershell -NoProfile -File scripts/check_household.ps1
```

By default this configures and builds the complete Debug preset, then runs all
non-GPU CTest checks, including deterministic physics/runtime tests and real
ImGui tests in memory. It creates no application windows and sends no desktop
input. Each run retains stage logs, JUnit test results and `result.json` in a
fresh `build/household-check-<UTC timestamp>/` directory. A failed stage stops
the run and returns its native exit code; an empty test selection is an error.
Use `-OutputDirectory <fresh-path>` to select the artifact directory or
`-SkipBuild` when the Debug build already matches the sources. The opt-in
`-Vulkan` switch also runs automated GPU readbacks, which create native windows
and require the resolutions described below.

The code checks cover hold lag and fixed-step actions, wall/door/player/actor
contacts, door restart after obstruction, drop/throw priority, occupied-hand
actions, reading, radio/captions, suspension and fresh-run state. They do not
establish subjective hold/throw feel or physical audio listening. Further manual
testing of this change was stopped at the user's request on 2026-09-11.

When changing fixture generation, regenerate the three neutral authored scenes
and run affected deterministic checks:

```sh
python -B scripts/prepare_household_level.py
cmake --preset debug
cmake --build --preset debug --target engine_tests near_laugh level_editor vulkan_smoke household_measure -j 4
build/debug/tests/engine_tests.exe --gtest_filter="Household*.*:EditorHousehold.*:EditorHouseholdPicking.*"
```

Preparation overwrites only `household-interactions.level.json` (four boxes),
`household-baseline.level.json` (zero boxes) and `household-capacity.level.json`
(16 initially awake boxes). The ordinary game uses their authored records;
filenames do not select behavior. The shared table, document, radio, thin wall,
door and actor route support repeated interaction and obstruction checks.
Launch the primary scene with:

```sh
build/debug/bin/near_laugh.exe --level resources/levels/household-interactions.level.json
```

Existing deterministic ImGui tests run without a GPU:

```sh
build/debug/tests/engine_tests.exe --gtest_filter="EditorHouseholdUiInteraction.*"
```

For agent-driven work, delegate those automated UI tests to `ui_test_runner`.
Delegate a prepared screenshot interaction scenario to `ui_driver` only with
authorization for that desktop run. Automated UI results do not establish
visual acceptance. The GPU preset includes `vulkan_household_interactions` and
`editor_household_interactions`; their readback scenarios check full box
orientation, door-free and 248-box scenes, both caption lanes with reader text,
800x600/1920x1080/3840x2160, replacement failures, recovery and final lifetime.
`household_smoke_fixtures` preflights runtime fixtures without creating a window.
The GPU runs require a desktop that can provide every requested resolution.

```powershell
ctest --preset debug --output-on-failure
ctest --preset vulkan-smoke --output-on-failure
.\scripts\measure_household.ps1 -OutputDirectory build/household-debug-check -Check -DebugBuild
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target household_measure -j 4
.\scripts\measure_household.ps1 -OutputDirectory build/household-release-check -Check
.\scripts\measure_household.ps1 -OutputDirectory build/household-release-timings
```

Use fresh output directories. Measurement runs fullscreen at 1920x1080/60 Hz
FIFO, alternating three paired zero-box/16-box runs with 10 s warmup and 60 s
sampling. The fixed-boundary driver applies documented impulses, hold targets,
drop/throw and actor/door requests to existing runtime owners. Sidecar workload
CSV records actual awake/rest counts and accepted actions at every boundary;
any sample with fewer than 16 awake bodies invalidates the capacity workload.
No pose replacement or sleep bypass is used. `-Check` produces readbacks and
exercises recovery; those rows are functional checks, never timings for acceptance.
The script retains raw timing/workload data, hardware/configuration and all T1
gates, including failures. Audio uses silent output; listening remains separate.

Functional checks, Vulkan recovery/lifetime checks, the independent UI-authored
save/reopen/Play scene and controlled Release comparisons have passed. Retain
their results and earlier desktop observations; human P06/T3 acceptance and
subjective feel/listening remain unverified. Do not infer performance from
object counts or run measurements concurrently with builds, tests or other GPU
work. Track results and unavailable checks in the selected
[P06 validation record](../openspec/changes/add-household-interactions/validation.md).

## P07a character animation

Run `build/debug/bin/character_animation_viewer.exe` from any working directory.
F5 cycles the first mannequin through idle/walk/interact; additional instances
use independent clips and offsets. P pauses, A/D seeks by 0.1 s and pauses,
Space restarts, M switches one/four instances, E changes the fixed inspection
view, R toggles the spotlight and Escape closes. Clip/time/status and controls
appear on screen. Minimize freezes playback without accumulating waited time.
The viewer owns no gameplay or audio coordinator and ordinary level filenames
activate no viewer sequence.

`--preflight` validates/deforms all three prepared clips without creating a
window. The process tests also run a copied executable containing only the
prepared character resources, from a different working directory.

```powershell
cmake --preset debug
cmake --build --preset debug --target character_animation_viewer character_animation_smoke
.\build\debug\bin\character_animation_viewer.exe --preflight
.\build\debug\bin\character_animation_smoke.exe build/character-captures
python scripts/retain_character_captures.py build/character-captures build/character-captures-png
.\scripts\check_character_viewer.ps1 -OutputDirectory build/character-viewer-controls
ctest --preset vulkan-smoke --output-on-failure
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target character_animation_viewer
.\build\p10-release\bin\character_animation_viewer.exe --check 4 build/character-timing-check.csv
.\scripts\measure_character_animation.ps1 -OutputDirectory build/character-timings
```

Capture/timing output paths must be fresh. The smoke executable records bind,
clip and interrupted-transition poses, off-screen shadows and editor retention,
and checks injected allocation/upload failures and recovery with validation
alive through teardown. The retention script verifies exact stored RGB after
converting PPM readbacks to PNG, and preserves hashes and readback results.
The Windows desktop control script sends actual viewer keys and retains
screenshots/logs. `--check` records 40 frames with requested swapchain recovery
and drains timing queries through teardown; its short CSV is a functional check.
It also saves the rendered measurement view after recovery beside the CSV with
the same stem and a `.ppm` extension; both paths must be fresh. Inspect this
readback to verify character placement and visibility in the fullscreen scene.
Use the Debug viewer with this mode to enable Vulkan validation, then recheck
the final Release build before sampling. Capture waits are excluded from
performance runs; `--measure` requests no framebuffer readback.

Release measurement uses the packaged eight-light/four-caster interior with
the same fixed camera and initial doors for zero, one and four characters.
The script records three runs per setup, each with 10 s warm-up and 60 s
sampling at fullscreen 1920x1080/60 Hz. It preserves all nine raw CSV/log pairs
and writes `summary.json`. Raw CSV separates active CPU, character deformation/upload,
GPU whole-frame/shadows and presentation/fence waits. Run it on the ordinary
interactive desktop without concurrent builds or GPU work. CPU/GPU p95 must
meet 16.67 ms and frame p50/p95/p99 must meet 16.9/20/33.4 ms in every run;
retain failed gates and unavailable GPU fields. Existing T1 gates remain
unchanged; this P07a fixture alone does not establish full T2.
See [source preparation](../resources/characters/README.md) and the
[P07a validation record](../openspec/changes/archive/2026-09-08-add-character-animation/validation.md).

## P07b scripted characters

Run `build/debug/bin/scripted_characters.exe`, optionally with `--four` or
`--silent`. Ordinary `near_laugh --level resources/levels/scripted-characters.level.json
--entry view` also starts the authored route; the filename selects no
special gameplay. The walker turns at the corner, climbs three 20 cm rises,
waits at the closed door, and performs the final interaction after the player
opens it with E. The four-actor file adds an independent east route and opposing
north/south actors that intentionally wait for each other.

Development controls while the cursor is captured: F5 explicitly restarts each
initial route from current accepted placement, F6 cancels, P suspends/resumes
player/door/actor/animation/cue time together, and M mutes. F5 retains pause/mute
settings. A new process restores authored placements. Escape releases the
cursor and continues world/audio time. Minimized waits discard suspended time
and held control edges; restore neither catches up nor restarts actions.

The editor shows initial idle poses, preserves character data through
unrelated edits/undo/save, and preflights selected mannequin and actor-linked
audio before Play. Actor catalog visual bounds, mark/facing handles and ordered
route links are viewport-selectable. A selected actor distinguishes visual bounds
from the cyan capsule proxy. Missing links are labeled at resolved marks; records
without a finite spatial anchor retain list/Properties access without a fallback
position. A missing endpoint leaves a gap in the route overlay.

In **Objects > Characters**, add/select actors, marks and routes. **Properties**
edits their durable IDs, catalog model, speed, initial route and sound links,
mark feet/yaw, route owner, ordered mark links and optional final `interact`.
Actor placement belongs to its initial mark: **Select/edit initial mark** opens
that mark's properties, which identify all sharing actors and route entries.
Adding an actor reuses an explicitly selected mark, or creates a new initial
mark in the same command. New placement may overlap existing content until
it is moved. **Place on surface** works for actors and scene marks; actor placement
edits the shared initial mark, whose consumers remain visible in Properties.
Feet use the exact nearest upward structural/terrain hit, including upper floors.
Walls and undersides block placement, Escape cancels, and a click makes one edit.
Duplicating an actor creates an independent
offset initial mark and clears its route/sound links. A missing initial mark
must be repaired before duplication. Route duplication retains its owner/order.
**Duplicate**, **Delete**, Ctrl+D, Delete and Ctrl+Z/Ctrl+Y use the shared history;
compound actor/mark creation and incoming-link renames each undo in one step.
Deletion leaves broken references visible; new automatic IDs do not reconnect
them. Unknown references and finite invalid values remain repairable and block
Save/Play until corrected. Nonfinite values and capacity overflow are refused.

**Character inspection** starts only on explicit request. Select an actor for
idle/walk/interact playback, optionally choose a scene mark for inspection, or
select a mark and choose the actor to inspect there. **Preview clip** uses the
shared 0.15-second transitions; **Pause/Resume preview**, **Restart preview**,
**Stop preview** and **Clip time** affect only the snapshot. Seeking is silent.
Select a route for **Start route snapshot**: the panel lists its ordered marks,
current segment/target, standing turn/walk/final-action stage and feet/yaw.
This is schematic motion, including interpolated stair elevation; it excludes
collision, door operation and sound. Use saved-file **Play** for physical checks.
Missing required references or current character resources prevent Start with
diagnostics. Other actors retain initial poses. Edits, undo/redo, selection,
New/Open/Close, minimize and Play stop snapshots; restoration never restarts them.
Starting character inspection stops audio audition and starting audition stops
character inspection. Changing list selection commits a pending character field
before selecting the next record. Preview controls leave authored values and
history unchanged; text/numeric focus captures navigation input.

Playback updates fenced character vertex buffers while static geometry,
materials, indices and lighting remain installed. Resize and attachment-format
recovery preserve the current running or paused snapshot. Failed resource
replacement retains the complete previous scene with a stale diagnostic; it
requires correction or undo before a new snapshot can start. An edit made while
minimized installs on restoration without resuming inspection. The character
Vulkan smoke checks presented seek/pause pixels and final GPU teardown.
The independent second-scene authoring and ordinary Play acceptance are recorded
in [P07c validation](../openspec/changes/archive/2026-09-09-add-character-authoring/validation.md).
Automated checks alone do not establish T2 acceptance: the retained scene,
desktop evidence and explicit audio-observation limits are part of that record.

```powershell
python -B scripts/prepare_scripted_characters.py
python -B scripts/prepare_scripted_character_audio.py
cmake --build --preset debug --target engine_tests scripted_characters scripted_character_measure level_editor vulkan_smoke -j 4
ctest --preset debug --output-on-failure
ctest --preset vulkan-smoke --output-on-failure
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-debug-check -Check -DebugBuild
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target scripted_character_measure -j 4
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-release-check -Check
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-release-timings
```

Use fresh output directories. The measurement keeps P07a's eight lights/four
casters, initial doors, camera, fullscreen 1920x1080/60 Hz FIFO and disabled
flashlight. It uses collision-valid short lanes beside the room chair, with
explicit alternating out/back requests and final interactions. Geometry,
entries and lighting remain identical for zero/one/four actors. Audio runs the
real offline mixer at 48 kHz; captions are excluded from timing presentation
to preserve the P07a color workload. This does not measure device callbacks.
The script activates the measurement window; startup focus notifications are
drained before timing. Any interruption during sampling invalidates the run.

Three runs per count retain 10 seconds warmup and 60 seconds sampling each,
with the unchanged P07a gates and no concurrent builds/tests/GPU work. Added
CSV scopes separate route decisions, actor collision, pose/contact processing,
world/player/doors and audio handoff/mixing from deformation/upload and GPU
costs. Only callers supplying timing storage perform these clock reads.
`-Check` instead captures 40 frames and a lossless measurement view, including
recovery; these rows are functional evidence, never performance samples.

The runtime Vulkan smoke retains route/door/action/caption readbacks beneath
its working directory's `build/scripted-character-runtime-captures`. Contact
tests retain a simulation/handoff/offline-onset CSV under `build/`. Neither
proves physical output latency or listening quality. See the
[P07b validation record](../openspec/changes/archive/2026-09-08-add-scripted-characters/validation.md)
for retained runs, observations and unavailable evidence. T2 remains pending
P07c's second scene authored and played with the editor.
