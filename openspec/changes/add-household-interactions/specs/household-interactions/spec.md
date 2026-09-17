## Purpose

Provides this game's bounded readable documents, physical pickup boxes and stationary radios, with explicit authored identities and coherent interaction, collision, presentation and recovery behavior.

## ADDED Requirements

### Requirement: Explicit household definitions
A level SHALL support zero through 16 physical boxes, 32 readable documents and eight radio controls in separate household collections. Each record SHALL have a durable ID matching `[a-z][a-z0-9-]{0,63}`, unique within its collection. Boxes SHALL contain only ID, finite initial center and initial yaw; the supported model SHALL be a generated 0.30 metre cube with a fixed one-kilogram physical profile. Documents SHALL contain only ID, finite position/yaw, a nonempty title of at most 80 Unicode scalar values and one through 16 ordered nonempty pages of at most 480 scalar values each. Their world representation SHALL be a fixed thin nonblocking readable panel. Radios SHALL contain only ID, one static-prop reference, one audio-source reference and an initial on/off value. Records SHALL contain no runtime velocities, held state, body handles, filesystem paths, arbitrary actions or placement slots. Existing static props SHALL remain static and SHALL NOT become pickup objects by proximity or model identity.

#### Scenario: Only designated boxes can be picked up
- **WHEN** a scene contains a household box beside ordinary furniture and a decorative model
- **THEN** pickup is offered for the box only, with its authored identity retained throughout the run

#### Scenario: Definition exceeds the supported profile
- **WHEN** a count, ID, transform, title/page bound or concrete radio reference is invalid
- **THEN** validation identifies the record and field and refuses Save and runtime handoff while structurally safe editor data remains repairable

### Requirement: Physical pickup and holding
An eligible pickup SHALL acquire at most one reachable, visible free box using the shared authored target policy. The same box SHALL remain a dynamic physical body and a visible world object throughout holding, with gravity, contact and full rotation. Holding SHALL attract it toward a fixed point derived from the accepted fixed-step player eye/look, independently of render interpolation using bounded force and torque, without hands, teleportation, world-collision suppression or a second visual copy. A wall or other blocker SHALL prevent the body following an unreachable target. Excessive separation, an invalid hold target or loss of active exploration controls SHALL release the hold at the actual pose without a throw impulse; explicit world suspension and minimization SHALL instead preserve the hold until active time resumes. The held body SHALL remain responsive to changed targets after stillness and restoration; sleeping SHALL NOT prevent following or release. A refused acquisition SHALL leave both ownership and physical state unchanged with readable feedback.

#### Scenario: Box is picked up in clear space
- **WHEN** one eligible pickup is accepted with empty hands
- **THEN** that box approaches the visible hold position through physics, remains uniquely identified and cannot be picked up a second time while held

#### Scenario: Held box meets a wall
- **WHEN** the player looks or moves so the requested hold point lies behind a wall or door
- **THEN** the box stays on the reachable side, forces remain bounded, and excessive separation releases it without moving it through the obstruction

#### Scenario: Cursor is released while holding
- **WHEN** the player releases exploration cursor capture while holding a box in an otherwise active world
- **THEN** the box drops at its actual pose without an extra impulse and world physics continues

### Requirement: Physical drop and throw
Drop SHALL release the held box at its actual position, orientation and velocities without adding an impulse or requiring a placement marker. Throw SHALL release that same box and apply one bounded forward impulse using the accepted throw direction; charging, aim assistance, damage and destruction SHALL NOT be introduced. Both SHALL preserve world collision. A throw into a nearby obstacle SHALL collide there rather than relocate the body beyond it. Free boxes SHALL fall, rotate, collide with terrain, structures, static prop proxies, other boxes, the player, doors and actors, and settle or sleep. Pickup SHALL wake a sleeping box. There SHALL be no automatic respawn, inventory storage or silently corrected placement if a box leaves an authored reachable area.

#### Scenario: Box lands on a table
- **WHEN** the player drops a box over an authored collidable table or floor
- **THEN** gravity and contact determine its resulting pose and it can subsequently be picked up from that pose

#### Scenario: Throw input is held
- **WHEN** a throw press is accepted and its control remains down across later event batches
- **THEN** one existing box receives one impulse and no later box is automatically thrown

#### Scenario: Throw meets a near wall
- **WHEN** a box is thrown toward a supported thin wall immediately in front of its actual pose
- **THEN** it remains on the physically reachable side with finite state rather than starting from an unobstructed pose beyond the wall

#### Scenario: Two boxes meet and settle
- **WHEN** two free boxes collide off-centre and then come to rest on supported surfaces
- **THEN** their translations and full rotations respond independently to contact and remain consistent with their displayed and targetable geometry

### Requirement: Concrete stationary radio operation
Each radio SHALL reference one distinct `apartment-radio` static prop with no collision boxes and one exclusively radio-owned, spatial, non-autoplay source with a looping ambience cue and valid Russian caption. Actor-owned or other-radio-owned sources SHALL be rejected. The current source position SHALL follow the fixed radio prop through a run-local override. E interaction with the selected radio SHALL toggle its logical on/off state once; on starts one source instance, off cancels that instance and caption, and a later on starts anew. Initial-on state SHALL start once at scene entry. Muting or device loss SHALL NOT change logical state, suppress state feedback or replay a start. Runtime on/off state SHALL NOT modify the authored initial state or unrelated sources.

#### Scenario: Radio is switched repeatedly
- **WHEN** eligible on, off and on actions occur on a radio
- **THEN** the first instance stops completely and the last action starts one new instance without overlapping copies

#### Scenario: Radio is operated without sound
- **WHEN** a radio is switched while muted or without an available output device
- **THEN** its on/off feedback and eligible caption remain visible and logical source time follows the ordinary audio policy

#### Scenario: Audio or prop ownership conflicts
- **WHEN** a radio references another radio's prop/source, an actor-owned source or a prop with the wrong model or nonempty collision boxes
- **THEN** shared validation identifies the conflict and refuses Save and Play until repaired

### Requirement: Household state lifetime and observation
Runtime state SHALL distinguish each box's actual pose, linear/angular velocity and free/held ownership; each radio's on/off state; and the current readable document/page. These values SHALL be available through their concrete owners without a general entity registry, event bus, inventory or save format. Fresh scene entry SHALL restore authored initial values and zero initial box velocities once. Presentation skip/recovery SHALL preserve current state. Minimize and explicit development suspension SHALL freeze physical motion, hold targets and cue time, discard suspended wall time and consume inactive input without a deferred input-driven pickup/drop/throw. Suspension alone SHALL preserve holding; a safety release already required by earlier control loss SHALL survive suspension and recapture and complete before the next advancing physics step. Shutdown SHALL release hold dependencies before bodies and physics lifetime. This change SHALL provide no save-game or checkpoint serialization.

#### Scenario: Suspended thrown box resumes
- **WHEN** minimization or explicit suspension interrupts a moving box and an active radio
- **THEN** restoration continues the same box pose/velocities and cue identity without simulation catch-up or duplicate input

#### Scenario: New run follows a changed scene
- **WHEN** the player moves boxes and switches radios, exits, and starts the same saved level again
- **THEN** the new run restores authored initial states and the saved level bytes remain unchanged
