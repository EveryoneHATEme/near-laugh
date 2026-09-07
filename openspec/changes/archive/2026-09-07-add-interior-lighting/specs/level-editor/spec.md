
## ADDED Requirements

### Requirement: Inspectable interior lighting preview
The editor SHALL expose the bounded light/switch collections, per-light initial and shadow flags, ambient value, stable references and shadow-budget diagnostics. Initial light and door values SHALL determine the same supported lighting/shadow result as a fresh game run, without constructing gameplay simulation. Broken light links SHALL remain selectable and repairable and SHALL prevent Save/Play. Unsafe lighting inputs SHALL not reach GPU submission. Resource replacement SHALL install coherent geometry, light state and shadows together after dependent work completes; failure SHALL retain usable prior resources and explicitly identify the preview as stale. Unrelated edits, terrain sculpting, undo/redo and presentation recovery SHALL not silently restore fixed-light defaults or stale occluders.

#### Scenario: Edited scene is previewed and played
- **WHEN** the author sets several light initial values and door initial poses, saves and starts Play at a matching inspection view
- **THEN** editor and fresh game agree on light enables, ambient, supported wall/door shadows and material coverage

#### Scenario: Light link is broken and repaired
- **WHEN** the author deletes a referenced light and then repairs the link or undoes deletion
- **THEN** a switch-specific diagnostic and Save/Play gating appear and clear with the document state without a silent link substitution

#### Scenario: Terrain changes while shadows are visible
- **WHEN** a terrain edit or undo changes a supported occluder
- **THEN** subsequent coherent preview uses the updated terrain for visible geometry and shadows while preserving other light/switch values

#### Scenario: Preview resource replacement fails
- **WHEN** an edit requires new light/shadow resources and their preparation fails
- **THEN** the document remains editable and the last coherent preview is marked stale until a complete replacement succeeds
