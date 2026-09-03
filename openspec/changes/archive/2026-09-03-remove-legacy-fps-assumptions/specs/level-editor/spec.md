## MODIFIED Requirements

### Requirement: Standalone editor application
The project SHALL provide a desktop `level_editor` application that owns its window, editor input, editor camera, document state, rendering, and orderly shutdown independently from the `near_laugh` game application. Building or running the game SHALL NOT require editor UI code, editor document state, or editor-only resources, and neither application SHALL require the removed shooting-target texture.

#### Scenario: Editor starts
- **WHEN** the editor executable starts with valid current resources
- **THEN** it opens an editor workspace without constructing the grounded player, gameplay input mapper, player flashlight, or physics simulation

#### Scenario: Game target is inspected
- **WHEN** the shipping `near_laugh` target's transitive dependencies and public headers are inspected
- **THEN** they contain no editor UI dependency or editor document type

#### Scenario: Runtime resources are inspected
- **WHEN** the game and editor executable-relative resource layouts are inspected
- **THEN** neither layout contains or requires a shooting-target texture asset
