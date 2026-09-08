## ADDED Requirements

### Requirement: Character-bearing document compatibility
The editor SHALL retain every safely decoded character/mark/route field and order through Open, unrelated edits, undo/redo, Save As and semantic saved-file Play verification. It SHALL display renderable initial actor poses independently of authored route autoplay, with diagnostic markers for invalid references. Save and Play SHALL use shared character validation; Play SHALL also prepare all selected character and linked sound resources before creating a process. Missing selected data SHALL block launch with actor/model/clip/source context while unselected character assets SHALL not be required. Actor routes SHALL not run automatically in the editor.

#### Scenario: Lighting is edited in a character level
- **WHEN** a character-bearing file is opened, a light is edited and the document is saved/reopened
- **THEN** all character values and references remain identical and the editor shows authored initial poses

#### Scenario: Selected character fails preflight
- **WHEN** a saved level is otherwise valid but its mannequin or selected sound metadata is unsupported or absent
- **THEN** no game process starts and the editable document and previous usable preview remain available with diagnostics
