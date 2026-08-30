# shooting-targets Specification

## Purpose

Defines the fixed damageable target plates and their immediate visual response for the deterministic built-in FPS shooting range.

## Requirements

### Requirement: Concrete shooting-range targets
The runtime SHALL create gameplay state for exactly the three target plates declared by the immutable prototype level, SHALL associate each target with one stable prototype solid, and SHALL give each target the same fixed positive starting health. Target state SHALL be owned by shooting-range gameplay rather than by physics or rendering.

#### Scenario: Shooting range starts
- **WHEN** the immutable prototype level, physics world, and shooting-range gameplay initialize successfully
- **THEN** three live target states correspond one-to-one with the three visible and collidable target plates

#### Scenario: Target mapping is invalid
- **WHEN** target descriptions do not identify three distinct valid target solids
- **THEN** initialization fails before the runtime enters the main loop

### Requirement: Target hit and destruction
A closest static hit on a live target's associated solid SHALL remove one fixed amount of health from that target without affecting any other target. Health SHALL NOT fall below zero; reaching zero SHALL permanently destroy the target for the current run, and later hits SHALL NOT change its health. A destroyed plate SHALL remain present as a static visible and collidable surface and SHALL continue to stop later closest-hit rays.

#### Scenario: Live target is hit
- **WHEN** an accepted shot's closest static hit identifies a live target plate
- **THEN** only that target loses the configured amount of health

#### Scenario: Target health reaches zero
- **WHEN** a hit reduces a live target to zero health
- **THEN** the target enters its destroyed state and cannot take further damage

#### Scenario: Destroyed target is hit again
- **WHEN** a later shot first intersects the static plate of an already destroyed target
- **THEN** the plate stops the shot while target health and every other target remain unchanged

#### Scenario: Environment solid is hit
- **WHEN** a shot's closest static hit identifies a floor, boundary, obstacle, step, or low-clearance solid
- **THEN** no shooting target loses health

### Requirement: Target presentation feedback
Every damaging hit SHALL highlight the affected plate for one fixed simulation-time duration, refreshing that duration when another damaging hit occurs. After the final-hit highlight expires, a destroyed plate SHALL remain visibly dimmed; live plates whose highlight has expired SHALL return to their authored appearance. Feedback timing SHALL advance only with fixed simulation steps and SHALL be supplied to rendering without exposing target health.

#### Scenario: Damaging hit is rendered
- **WHEN** a live target receives damage and a frame is requested before its highlight duration expires
- **THEN** that plate is visibly highlighted while other non-highlighted live plates retain their authored appearance

#### Scenario: Live-target highlight expires
- **WHEN** a damaged target remains alive and its highlight duration elapses through fixed simulation
- **THEN** subsequent frames show its authored appearance without a highlight

#### Scenario: Final-hit highlight expires
- **WHEN** a target is destroyed and its final highlight duration elapses
- **THEN** subsequent frames show the plate with the persistent destroyed dimming

#### Scenario: Simulation is paused by a minimized window
- **WHEN** the runtime waits for a non-zero framebuffer without executing simulation steps
- **THEN** target highlight durations do not elapse during the blocking wait
