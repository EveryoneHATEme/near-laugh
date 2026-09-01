## ADDED Requirements

### Requirement: Fixed-step shooting-range coordination
The Engine-owned main-thread loop SHALL sample rifle input through the same first-person control-active decision used by player movement, SHALL retain trigger edges until a fixed step can process them, and SHALL coordinate player movement, recoil recovery, shot emission, closest-static-hit resolution, target damage, and feedback timing during each complete fixed simulation step. Rendering SHALL receive only the resulting camera and prototype-scene presentation after all complete steps for that iteration.

#### Scenario: Complete simulation step accepts a shot
- **WHEN** first-person controls are active, the rifle is ready, and a complete fixed step processes active or pending primary input
- **THEN** the runtime resolves one shot from the current simulated player aim, applies its closest hit to shooting-range state, applies recoil for subsequent aim, and exposes the resulting camera and target presentation to the next frame request

#### Scenario: Iteration has no complete simulation step
- **WHEN** primary input begins during a render-loop iteration that retains only a fractional fixed-step remainder
- **THEN** the runtime does not resolve a partial-step shot and retains the trigger press for a later complete step

#### Scenario: First-person controls are inactive
- **WHEN** the cursor is released or is being recaptured
- **THEN** movement and rifle input are neutral while physics, recoil recovery, target feedback timing, and other active fixed simulation continue

#### Scenario: Multiple fixed steps precede rendering
- **WHEN** one bounded loop interval produces multiple complete fixed steps
- **THEN** shooting and target state advance once per step in order and only the final resulting presentation is supplied to the iteration's single frame request

### Requirement: Gameplay-free render request
The runtime SHALL convert target hit and destruction state into a backend-neutral prototype-scene presentation containing only highlighted-solid and dimmed-solid masks. Frame requests and renderer interfaces SHALL NOT expose rifle state, target health, damage values, physics hits, or gameplay implementation types.

#### Scenario: Target state is prepared for rendering
- **WHEN** shooting-range state contains highlighted or destroyed targets
- **THEN** the frame request identifies their associated prototype solids through presentation masks without including gameplay state

#### Scenario: Runtime-render boundary is inspected
- **WHEN** frame and renderer-facing declarations are inspected
- **THEN** they contain no weapon, health, damage, or physics-library types

