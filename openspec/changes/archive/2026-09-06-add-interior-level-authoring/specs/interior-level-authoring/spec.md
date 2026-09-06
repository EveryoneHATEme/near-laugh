## Purpose

Defines the apartment and rear-stair blockout workflow for this game, from creating a multi-floor interior and named starts to launching a saved level in a separate game process.

## ADDED Requirements

### Requirement: Interior blockout creation
The editor SHALL offer a new interior document with no terrain, a supporting floor, one valid default entry, and the existing required light and packaged-prop data. It SHALL start unsaved and dirty. Authors SHALL be able to construct rooms, corridors, openings, landings, and walkable stairs using the existing bounded axis-aligned solids. Creating an interior SHALL NOT require hidden terrain, model import, a scene hierarchy, or new runtime geometry types.

#### Scenario: Author starts an interior
- **WHEN** the user creates a new interior after resolving any dirty-document decision
- **THEN** a valid editable starting document appears with no terrain and no save path, and its floor, entry, two lights, and chair are available for editing

#### Scenario: Author constructs an upper floor
- **WHEN** the user adds and positions floor slabs, walls, and individual stair treads within the supported solid limit
- **THEN** the document can represent connected walkable floors at different heights, with openings formed by gaps between solids

### Requirement: Durable named entry points
A valid level SHALL contain between one and sixteen entry points and an explicit default entry identifier. Each entry SHALL have a unique case-sensitive identifier of one through sixty-four ASCII characters, starting with a lowercase letter and containing only lowercase letters, digits, and hyphens, plus a finite foot position and yaw. Entry identifiers SHALL survive save/reload and unrelated edits without depending on array position or editor selection handles. Entry support and clearance SHALL be validated for every entry, including entries not selected for the next launch.

#### Scenario: Author defines two starts
- **WHEN** the author saves entries named `apartment` and `lower-landing` on their respective supported floors
- **THEN** both identifiers and poses survive reload and each can be selected independently for playtesting

#### Scenario: Entry reference is invalid
- **WHEN** identifiers are duplicate or malformed, the entry count is outside its bound, or the default identifier has no matching entry
- **THEN** validation identifies the entry or default field and refuses saving and runtime handoff

#### Scenario: An unselected entry is unsupported
- **WHEN** the selected entry is valid but another authored entry has no floor support or intersects blocking geometry
- **THEN** the level remains invalid for saving and launch until the other entry is repaired or removed

### Requirement: Explicit game launch selection
The game launcher SHALL accept `--level <path>` and `--entry <id>`. An omitted level SHALL select the packaged prototype; an omitted entry SHALL select the chosen level's declared default. A relative level argument SHALL resolve against the invoking working directory once, and an absolute path SHALL retain its meaning regardless of that directory. Non-level resources SHALL continue to resolve beside the actual executable. Unknown, repeated, or incomplete options SHALL report usage and exit unsuccessfully. A missing or invalid level or unknown entry SHALL fail startup without falling back to another file or entry.

#### Scenario: A saved interior is selected
- **WHEN** the game is launched with an absolute saved interior path and `--entry lower-landing` from another working directory
- **THEN** it starts at that entry in the selected file and uses the executable's packaged shaders, textures, and chair

#### Scenario: Default invocation is used
- **WHEN** the game is launched without level or entry arguments
- **THEN** it loads the packaged prototype and starts at its declared default entry

#### Scenario: Requested entry does not exist
- **WHEN** an explicit entry identifier does not exist in the selected level
- **THEN** startup reports the resolved level path and requested identifier, exits unsuccessfully, and initializes no level-dependent physics or renderer

#### Scenario: Relative path or invalid arguments are used
- **WHEN** a caller supplies a relative level path or malformed launch options
- **THEN** a valid relative path resolves once against that caller's working directory, while malformed options fail with usage before application startup

### Requirement: Apartment and stairs acceptance scene
The project SHALL package a temporary interior acceptance level alongside the prototype. It SHALL contain Lena's room, a corridor, a kitchen, rear stairs, and a lower landing; no terrain; a default `apartment` entry; and a `lower-landing` entry. The route SHALL be traversable in both directions with ordinary walking and the existing step behavior, without requiring jumping, crouching, terrain manipulation, or a movement-policy change. It SHALL retain the current two-light and single-chair profile and use temporary structural content.

#### Scenario: Both floors are exercised
- **WHEN** the saved acceptance scene is launched separately from each entry
- **THEN** the player starts on the correct floor, can traverse the connected route in both directions, and is blocked by the authored walls and slabs

#### Scenario: Authoring acceptance is repeated
- **WHEN** an author creates an interior, edits structural geometry and both entry poses, saves it, reopens it, and plays it from the editor
- **THEN** the saved authored values determine the playable scene without replacing the packaged prototype or modifying another open document
