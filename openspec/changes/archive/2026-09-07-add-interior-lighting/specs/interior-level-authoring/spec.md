## MODIFIED Requirements

### Requirement: Interior blockout creation
The editor SHALL offer a new interior document with no terrain, a supporting floor, one valid default entry, and two editable enabled/unshadowed starter light records with durable IDs, ambient 0.12 and an empty switch array, a valid structural floor material, empty props and empty doors. It SHALL start unsaved and dirty. Authors SHALL be able to construct rooms, corridors, openings, landings, and walkable stairs using the existing bounded axis-aligned solids. Creating an interior SHALL NOT require hidden terrain, model import, a scene hierarchy, or new runtime geometry types.

#### Scenario: Author starts an interior
- **WHEN** the user creates a new interior after resolving any dirty-document decision
- **THEN** a valid editable starting document appears with no terrain and no save path, and its floor, entry and two starter lights are available for editing, duplication and removal within the current profile without a mandatory chair

#### Scenario: Author constructs an upper floor
- **WHEN** the user adds and positions floor slabs, walls, and individual stair treads within the supported solid limit
- **THEN** the document can represent connected walkable floors at different heights, with openings formed by gaps between solids
