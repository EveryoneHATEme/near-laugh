## ADDED Requirements

### Requirement: Explicit character clip inspection
The editor SHALL provide explicit clip selection, play, pause/resume, restart, stop and seek for one selected character snapshot using the same validated assets and sampler as the game. Idle/walk/interact and their supported transitions SHALL be inspectable at the actor's initial mark or an explicitly selected scene mark without changing authored data, dirty state or history. The remaining actors SHALL retain their authored initial poses. Seek SHALL generate no gameplay result or audio event. Character preview SHALL not construct game physics, player input or a runtime session.

#### Scenario: Clip is paused and scrubbed
- **WHEN** the author seeks within interact while preview is paused
- **THEN** the viewport and current time show the corresponding stable pose at the selected mark and the saved document remains unchanged

#### Scenario: Preview clip changes during a blend
- **WHEN** the author requests another supported clip while playback is transitioning
- **THEN** the current displayed pose transitions according to character-animation without a bind-pose reset

### Requirement: Explicit schematic route preview
The editor SHALL expose a selected route's ordered marks, facing, current segment and final action and provide explicit start, pause/resume and stop for a schematic motion preview. The preview SHALL be labeled as excluding collision, door operation and action sound; real obstruction and audio behavior SHALL be tested through saved-file Play. It SHALL use the authored route and supported motion/clip conventions without changing mark positions, initial actor state or source definitions. It SHALL not certify a path as traversable or silently move authored geometry to fit it.

#### Scenario: Route crosses a closed door
- **WHEN** the author inspects the schematic route
- **THEN** its motion/links are inspectable with the stated limits, and the UI directs physical obstruction acceptance to Play without claiming that the preview proves clearance

#### Scenario: Route cannot be resolved
- **WHEN** a selected route has a missing actor, mark or unsupported final clip
- **THEN** Start is unavailable with the affected reference identified while safe records remain editable

### Requirement: Character preview invalidation and resource recovery
Any edit, undo/redo, document replacement, active document object selection change, minimization or Play request SHALL stop active character clip/route preview and discard its snapshot and pending actions; restoration SHALL not automatically restart it. Changing the clip within the preview controls SHALL follow the supported transition behavior without changing document selection. Preview controls SHALL operate independently from authored initial routes. Character preview and P04 audio audition SHALL be mutually exclusive: starting either SHALL stop the other, and character preview SHALL remain silent. Successful GPU replacement SHALL install a complete current static/actor/material/lighting set; failed replacement SHALL retain the previous coherent preview and label it stale. Presentation recovery without document invalidation SHALL retain a running preview's current state without replaying commands.

#### Scenario: Preview snapshot becomes obsolete
- **WHEN** a mark edit, undo, different actor selection or Play request occurs during preview
- **THEN** preview stops once, no old snapshot resumes later and subsequent preview requires a fresh explicit Start

#### Scenario: Character preview allocation fails
- **WHEN** a model/resource change cannot install its new GPU data
- **THEN** the prior complete preview remains labeled stale, the active document is retained and correction or undo can install a coherent current preview

### Requirement: Complete neutral character authoring acceptance
T2 acceptance SHALL demonstrate authoring a second neutral interior from supported editor operations, adding a character, marks, a route, audio links and local shadowed lighting, saving to an independent file, reopening and playing it in the ordinary game from another working directory. It SHALL verify literal Unicode paths, saved-file freshness, initial editor/game agreement, real player/door obstruction and release, localized action sound/captions, accepted movement and shadows, and preview invalidation/undo/recovery. The evidence SHALL identify manual observations and unavailable checks separately from automated test results. No runtime code or hand-edited JSON SHALL be needed to define this second scene.

#### Scenario: Author saves and plays a second scene
- **WHEN** its valid editor-authored route and sound links are saved and preflight succeeds
- **THEN** one ordinary game process uses those saved definitions and visibly exercises the supported character behavior without relying on preview state

#### Scenario: Saved asset or document is stale
- **WHEN** the saved file no longer matches the prepared document or selected character/audio preflight fails
- **THEN** no game process launches, diagnostics identify the cause and a fresh successful request is required
