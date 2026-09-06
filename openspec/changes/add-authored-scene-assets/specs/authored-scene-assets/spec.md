## Purpose

Defines the bounded packaged model and material identities, static placements,
and authored proxies used to furnish this game's apartment without a general
asset-discovery or scene system.

## ADDED Requirements

### Requirement: Explicit packaged asset identities
A level SHALL reference models and structural materials by stable logical identities from the game's explicit finite catalog, never by filesystem path. Model materials SHALL belong to the selected packaged model definition. Startup SHALL resolve only assets required by the selected scene under the explicit package root, including fixed generated-door and switch materials when present. Unknown identities and missing required resources SHALL be diagnosed with the identity, placement or field, and resolved path when available; they SHALL NOT be silently substituted.

#### Scenario: Repeated model is referenced
- **WHEN** two placements reference the same known model identity
- **THEN** both use that model's geometry and material without requiring independent files or identities for each copy

#### Scenario: Unused catalog asset is absent
- **WHEN** all selected-scene dependencies exist but an unreferenced catalog model is missing
- **THEN** the selected scene starts without reading or requiring that unrelated model

#### Scenario: Reference is unknown
- **WHEN** a structurally safe document contains an unknown model or material identity
- **THEN** runtime handoff and saving fail with the affected field while the editor retains the reference for correction

### Requirement: Durable independent static placements
A level SHALL contain zero through 128 ordered static model placements. Each SHALL have a unique case-sensitive prop identifier matching `[a-z][a-z0-9-]{0,63}`, one known model identity, finite translation and yaw, positive finite uniform scale, and zero through eight independently authored local collision boxes. Prop identifiers SHALL be independent of array position, editor selection, entry identifiers, and door identifiers. Static placements SHALL retain their authored pose for the run and SHALL NOT acquire interaction, animation or dynamic-body behavior from imported model metadata.

#### Scenario: Repeated furniture is authored
- **WHEN** two chairs reference one model with different valid transforms and identifiers
- **THEN** save/reopen and runtime preserve both independent placements and their shared model identity

#### Scenario: No static props are needed
- **WHEN** an otherwise valid interior has an empty props array
- **THEN** it validates, renders and runs without requiring a chair or any unreferenced model

#### Scenario: Placement identity or count is invalid
- **WHEN** prop identifiers are duplicate or malformed or the placement count exceeds 128
- **THEN** shared validation identifies the violation and refuses saving and runtime handoff

### Requirement: Explicit placement collision boxes
Each placement box SHALL contain a finite local center and positive finite half extents. Placement translation, yaw and uniform scale SHALL consistently determine its world pose and dimensions. Physics and interaction obstruction SHALL use these authored boxes without parsing render models or deriving triangle/alpha collision. An empty list SHALL deliberately provide no blocking collision. All placement boxes SHALL participate in entry clearance and door obstruction, but SHALL NOT provide entry floor support.

#### Scenario: Placement is transformed
- **WHEN** a collidable chair's yaw, scale or translation changes before launch
- **THEN** its visible model and all declared boxes use the same placement and all entry/door validation considers the resulting boxes

#### Scenario: Decorative phone has no proxy
- **WHEN** a phone placement has no collision boxes
- **THEN** it renders and remains selectable in the editor but does not block the player or interaction rays

#### Scenario: Material contains a cutout
- **WHEN** a placement uses a cutout material and an authored box
- **THEN** that box remains the deliberate collision representation independently of holes in the render texture

### Requirement: Bounded base-color and cutout materials
Packaged materials SHALL use base-color RGB with a finite base-color factor in [0,1], an optional supported embedded texture, and either OPAQUE or MASK mode. OPAQUE SHALL ignore source alpha for coverage. MASK SHALL retain only fragments whose sampled alpha multiplied by factor alpha is greater than or equal to the finite authored cutoff in [0,1]; discarded fragments SHALL write neither color nor depth. Surviving fragments SHALL use the existing bounded diffuse-light and opaque-depth behavior. The profile SHALL NOT add blended transparency, sorting, roughness/specular, emission, normal maps or a material graph. Unsupported runtime material inputs SHALL be rejected with asset/material context instead of being ignored implicitly.

#### Scenario: Phone cord is viewed
- **WHEN** the selected staged phone with cutoff 0.5 is rendered against a contrasting surface
- **THEN** the cord's transparent atlas regions leave holes without writing color or depth and the surviving cord pixels occlude geometry behind them

#### Scenario: Opaque texture includes alpha
- **WHEN** the radio's OPAQUE material samples an albedo texel with alpha below one
- **THEN** that texel remains opaque and its RGB is shaded normally

#### Scenario: Unsupported material is supplied
- **WHEN** a required runtime model contains BLEND or unsupported material inputs outside the controlled profile
- **THEN** loading fails with the model identity, material and unsupported feature identified

### Requirement: Selected content acceptance
The game SHALL package controlled derivatives of the supplied chair, table, phone, and radio, with faithful base-color mapping and source nearest sampling, plus wood-floor and wallpaper structural material examples. The phone derivative SHALL preserve its cord silhouette using MASK rather than treating the source BLEND flag as opaque. Only selected game resources and their provenance SHALL be staged; runtime and normal builds SHALL NOT depend on the raw source pack. The representative furnished scene SHALL preserve the existing apartment/stairs route and P03's door behavior.

#### Scenario: Selected furnished scene is inspected
- **WHEN** the scene is viewed in the game and editor with repeated chairs, table, phone, radio, wood floor and wallpaper
- **THEN** their distinct appearances, placement anchors, phone cutout and authored proxies are inspectable and the walking route and Lena door remain usable

#### Scenario: Packaged application is moved
- **WHEN** the packaged executable/resources are launched without the source pack from another working directory
- **THEN** the selected furnished scene still loads from its explicit resource root
