## Purpose

Defines this game's small authored character routes and action results so
motion, collision, visible animation and localized sound remain coherent.

## ADDED Requirements

### Requirement: Bounded character definitions and references
A level SHALL contain a required characters object with actors, marks and routes arrays bounded to four, 32 and 16 records respectively. IDs SHALL match [a-z][a-z0-9-]{0,63} and be unique within their collection. Each actor SHALL contain only id, catalog model, initial_mark, nullable initial_route, speed in [0.25,1.5] metres per second, nullable footstep_source and nullable interaction_source. Model scale and collision/animation profile SHALL come from the selected catalog, with no per-actor scale or arbitrary clip map. Each mark SHALL contain only id, finite feet position and finite yaw. Each route SHALL contain only id, actor reference, an ordered nonempty list of at most 32 mark references, and nullable final_clip, whose only supported non-null value is interact. Initial_route SHALL belong to the referencing actor. Every reference SHALL resolve with record/field diagnostics; the same mark can be shared. No record SHALL contain file paths, backend handles, gameplay scripts or mutable playback state. Actors with no initial route SHALL stand at their initial mark.

#### Scenario: Actor has an initial route
- **WHEN** a valid actor references a route owned by that actor
- **THEN** a fresh run starts the route once from the actor's initial mark without depending on the filename

#### Scenario: References are invalid
- **WHEN** a route names an absent actor/mark, selects an unsupported final clip, or an actor selects another actor's route
- **THEN** saving and runtime handoff fail with the affected record and field while safe editor data remains repairable

### Requirement: Supported scene placement and traversal
Every mark SHALL have structural or terrain foot support and sufficient standing clearance for the catalog actor proxy; props and doors SHALL NOT supply foot support. Initial actor proxies SHALL NOT overlap each other, any standing player entry, static blockers or initially positioned doors. Route marks SHALL be clear of static blockers, but a later route segment or non-initial mark obstructed by a door/actor SHALL remain valid for runtime waiting. Routes SHALL use supported grounded walking, including steps at most 0.30 m and slopes within the existing player walking profile; gaps, unsupported falls, jumps, teleports and pathfinding SHALL NOT be synthesized. Mark validation SHALL NOT claim that every segment is traversable. Runtime SHALL retain the last supported accepted pose when further movement is obstructed or unsupported.

#### Scenario: Route encounters a closed door
- **WHEN** valid endpoint marks lie on either side of an initially closed leaf
- **THEN** the route can be authored and the moving actor waits at the accepted obstruction until clearance exists

#### Scenario: Mark lies on supported sloped terrain
- **WHEN** a mark lies on terrain within the existing walking-slope limit
- **THEN** authored and published feet remain anchored to the accepted surface, and a separate support-derived vertical offset places the catalog capsule clear of that surface
- **AND** the same actual capsule placement governs initial overhead/static/entry/actor/door clearance and continuous runtime sweeps, including offset changes, while the 0.02 m arrival tolerance applies to ground feet

#### Scenario: Route crosses an unsupported gap
- **WHEN** the authored segment leads beyond supported traversable ground
- **THEN** the actor remains on its last supported accepted pose, reports blocked support and generates no arrival or interaction

### Requirement: Concrete route action lifecycle
The character controller SHALL accept start and cancel requests for existing actor-owned routes, exposing idle, turning, walking, blocked, interacting, completed and canceled results with the relevant actor/route/mark and obstruction reason. Repeating start for an active route SHALL be idempotent; requesting a different route while active SHALL return busy without replacing it. A new explicit start after completion or cancellation SHALL begin a new action from the actor's current accepted pose, without teleporting to its initial mark. Cancel SHALL discard pending movement and interaction events, stop sounds owned by that action, retain accepted placement and return presentation toward idle. Completed and canceled actions SHALL NOT restart automatically.

#### Scenario: Active route is requested repeatedly
- **WHEN** the same start request is repeated while the route is walking, blocked or interacting
- **THEN** the existing action continues with its original instance and no duplicate arrival or sound

#### Scenario: Route is canceled before its final mark
- **WHEN** cancel is requested while walking or waiting
- **THEN** later clearance does not resume it and no final interaction fires until a new explicit start

### Requirement: Accepted travel determines visible action
Actors SHALL turn toward each segment while standing, walk toward its next mark using collision-accepted displacement, then face that mark's authored yaw before advancing to the next segment or final interaction. Turning SHALL follow the shortest yaw arc at 120 degrees per second with positive rotation for an exact 180-degree tie. A mark SHALL be reached only after accepted feet position is within 0.02 m and facing within one degree; tolerance SHALL NOT permit an unchecked snap across a blocker. Locomotion phase and contact events SHALL advance from accepted horizontal distance using the calibrated catalog walk cycle. When travel stops, presentation SHALL transition toward idle without advancing walking distance or emitting stationary footsteps. Blocked actors SHALL retry on subsequent simulation steps without timeout, bypass or autonomous door operation. Final interact SHALL begin only after the final mark is reached and faced.

#### Scenario: Player stands in the corridor
- **WHEN** a requested actor step would cross the player's protected motion envelope
- **THEN** only clear supported displacement is accepted, further progress reports blocked and stationary time adds no walking contacts or final action

#### Scenario: Obstruction clears
- **WHEN** a waiting actor's next step becomes clear
- **THEN** it resumes from its current feet pose and saved accepted-distance phase without jumping forward to wall-clock schedule

#### Scenario: Route changes direction
- **WHEN** the actor reaches a corner mark
- **THEN** it finishes that mark's facing and turns toward the next segment while standing, using the supported idle/yaw turn without assuming a turn clip

### Requirement: Character sound ownership and markers
Actor audio links SHALL reference distinct exclusively actor-owned spatial non-autoplay one-shot sources. Footsteps SHALL use short ambience-kind samples at most 0.15 s; catalog contact spacing at allowed actor speed SHALL separate contacts by at least 0.20 s. Interaction sources SHALL use one-second essential cues containing at most 0.25 s of audible effect followed by silence, with a one-second Russian caption. Foreground arbitration and logical completion SHALL use the full one-second cue. Null links SHALL mean silent actions. Footstep starts SHALL follow accepted contact crossings once; blocked time SHALL create no backlog. Interaction sound SHALL be requested once at the catalog interaction phase from the accepted actor position. If foreground arbitration is busy, the interaction SHALL hold at that marker and report audio contention until the start succeeds or the action is canceled. Successful start SHALL retain one cue instance; muted or silent-device playback SHALL follow the same logical result. Source motion SHALL use accepted actor placement without restarting playback. Cancellation SHALL affect only the action's owned cues, and SHALL NOT interrupt unrelated foreground content. Detailed dialogue/lip-sync and prop manipulation SHALL remain outside this interaction.

#### Scenario: Interaction reaches its sound marker
- **WHEN** the actor has reached the mark and its interaction reaches the calibrated event phase with an available source
- **THEN** exactly one localized cue and its neutral Russian caption begin from that actor's accepted location

#### Scenario: Foreground cue is busy
- **WHEN** another essential or dialogue cue is active at the interaction marker
- **THEN** the actor holds its marker pose without stealing the cue, and a later successful retry fires once

#### Scenario: Route is muted or has no output device
- **WHEN** the same accepted motion and requests occur with mute enabled or no device available
- **THEN** contact/action results and captions match the audible run, independently of hardware completion

### Requirement: Neutral route acceptance and limits
The project SHALL package an explicitly authored neutral interior route with straight travel, a corner, supported stairs, an operable blocking door and a final interaction mark. A capacity setup SHALL exercise four actors, including actor-to-actor waiting. The scenario SHALL require no P05 narrative events, P09 checkpoint state or final cast/story. Acceptance SHALL record player/door/static/actor blocking, release and cancel paths, foot contact/scale/turn limitations, source/caption placement, matching animated shadows, pause/restart/recovery and one/four-actor Release timings under P07a's comparison conditions. Complete T2 acceptance SHALL additionally require the P07c authoring workflow.

#### Scenario: Fixture is played
- **WHEN** the saved route fixture is launched in the ordinary game
- **THEN** authored initial routes exercise the supported behavior without a hidden filename sequence or editor-only state

#### Scenario: Opposing actors wait
- **WHEN** two routes meet in a corridor without clearance to pass
- **THEN** actors retain nonpenetrating accepted poses and report blockage until a concrete cancel/new route or external clearance resolves it, without promising deadlock avoidance
