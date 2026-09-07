## Purpose

Defines this game's bounded authored interior lighting, controllable key-light
occlusion, and the observable visual and performance acceptance for T1.

## ADDED Requirements

### Requirement: Bounded authored interior light set
The level SHALL support zero through eight point lights, each with a unique durable ID, finite world-space position, finite non-negative RGB color, positive finite intensity and radius, and boolean initial enabled and casts-shadows values. Light IDs SHALL use one through sixty-four ASCII characters, start with a lowercase letter, and contain only lowercase letters, digits and hyphens, with case-sensitive uniqueness within the collection. Ambient SHALL be one independently authored finite scalar from 0 through 0.20 inclusive. Zero lights and zero ambient SHALL be valid. At most four light definitions SHALL request shadows, including initially disabled ones; their radii SHALL be between 0.25 and 20 metres inclusive. Definitions outside the profile SHALL produce actionable validation failures before saving or runtime handoff. Reordering definitions SHALL NOT change switch references or initial light values.

#### Scenario: Distinct rooms are authored
- **WHEN** a valid interior contains several lights with distinct initial states and linked switches
- **THEN** each light uses its own authored parameters and initial enable independently of collection position or how many switches reference it

#### Scenario: Completely dark environment is authored
- **WHEN** the author removes all point lights and sets ambient to zero
- **THEN** the level remains valid if its other content is valid and no hidden environment light is introduced

#### Scenario: Shadow capacity is exceeded by disabled lights
- **WHEN** a fifth light is marked as casting shadows even though it starts disabled
- **THEN** validation reports the configured shadow-budget violation instead of accepting a set that cannot support every switch combination

### Requirement: Supported key-light occlusion
Enabled shadow-casting point lights SHALL have their direct illumination blocked by supported rendered terrain, opaque structural solids, static OPAQUE/MASK prop geometry and current generated door geometry, regardless of whether an occluder appears in the camera view. Occlusion SHALL use rendered shape and material coverage independently of collision proxies. MASK-discarded regions SHALL NOT become solid occluders and OPAQUE materials SHALL ignore source alpha for coverage. With emitters at least 5 cm clear of surfaces and ordinary walls/door leaves at least 5 cm thick inside the supported radius, the body of an opaque wall or closed leaf SHALL NOT transmit visible key-light patches. Ambient, explicitly unshadowed point lights and the existing optional dynamic spotlight SHALL retain their unoccluded behavior. Fine shadow-edge aliasing and sub-centimetre contact accuracy SHALL be documented limitations rather than promises of exact visibility.

#### Scenario: Closed wall blocks one isolated light
- **WHEN** one shadowed key source is isolated, a supported opaque wall lies between it and a receiving surface, and the comparison is away from the shadow boundary
- **THEN** a fixed interior receiver patch excluding shadow edges differs from the source-disabled reference by at most 2/255 mean absolute RGB error and 4/255 maximum channel error in stored normalized color, without a visible light patch through the wall body

#### Scenario: Camera turns away from the blocker
- **WHEN** a wall or prop leaves the camera view but still lies between the key light and a visible receiver
- **THEN** its supported shadow remains on the receiver

#### Scenario: Decorative cutout casts a shadow
- **WHEN** a supported MASK prop with no collision boxes is illuminated by a shadowed key light
- **THEN** its covered visible geometry casts the corresponding shadow while cutout gaps remain open within documented resolution limits

#### Scenario: Unshadowed lighting is isolated
- **WHEN** only ambient, an explicitly unshadowed point light or the optional dynamic spotlight illuminates a surface
- **THEN** that contribution follows the existing unoccluded diffuse profile without being represented as key-light wall blocking

### Requirement: Shadows follow accepted door presentation
Each submitted frame's supported key-light shadows SHALL use the same accepted door geometry as its visible door presentation. Opening, reversing, closing or obstructing a door SHALL alter the expected lit region according to its accepted pose without anticipating requested motion, replaying an action, or rebuilding the immutable world. A fully blocked door SHALL retain its accepted shadow until another accepted pose is supplied. Switch changes and presentation recovery SHALL preserve coherent light and occluder state.

#### Scenario: Door is stopped by the player
- **WHEN** an opening door stops at an accepted partial angle because the player blocks further motion
- **THEN** its visible leaf and shadow stop together and no fully-open shadow appears until the door actually reaches that pose

#### Scenario: Light changes while door motion reverses
- **WHEN** the player toggles a linked light and reverses a moving door
- **THEN** the next submitted frame reflects the current enable and accepted pose without an old shadow detached from the leaf

### Requirement: Neutral furnished T1 acceptance scene
The project SHALL package a separate neutral interior acceptance scene with furnished rooms, corridor/stairs, six point lights including four shadowed key sources and two unshadowed fills, at least six reachable switches including two linked to one source, two operable doors and named inspection starts. It SHALL use representative existing structural and OPAQUE/MASK model materials. A capacity setup SHALL additionally exercise eight enabled lights and four configured shadow casters in the same interior. The intended walking route SHALL retain visually assessed navigational readability with the flashlight off. Opening or playing this level SHALL require no filename-triggered gameplay or narrative events. Historical P01/P04 scenes SHALL retain their mapped light parameters and initial enables after migration.

#### Scenario: Authoring and play agree
- **WHEN** the author edits initial lights/doors and ambient, saves, reopens and plays the T1 scene from a matching view
- **THEN** editor and fresh game agree on initial illumination and supported shadows while the saved source remains unchanged during play

#### Scenario: Route is inspected without the flashlight
- **WHEN** the intended T1 route is walked with the flashlight disabled and authored initial lighting
- **THEN** visual acceptance records readable navigation, distinct light pools and dark transitions without relying on leaked key light

### Requirement: Recorded T1 visual and performance evidence
T1 acceptance SHALL retain manual observations and comparison captures for wall/door blocking, accepted obstructed motion, off-screen occluders, face-seam stability, material coverage, readable darkness, initial editor/game agreement, all-off state and presentation recovery. It SHALL record actual hardware, driver, build, framebuffer, power/presentation settings and a reproducible measurement setup. On the selected current AMD Radeon(TM) Graphics machine at a 1920x1080 framebuffer and 60 Hz presentation, three warmed-up 60-second release runs of both the furnished and capacity setups SHALL each achieve p95 CPU active-frame and whole-frame GPU time at most 16.67 ms, end-to-end median at most 16.9 ms, p95 at most 20 ms and p99 at most 33.4 ms. CPU active work SHALL exclude and separately report presentation/fence waits; GPU timing SHALL measure GPU work, not infer it from FIFO-paced frame intervals. Measurements SHALL include an equivalent unshadowed baseline and moving-door/switch cases. Unavailable checks and missed thresholds SHALL be recorded without claiming T1 acceptance or silently reducing the supported profile. Vulkan correctness SHALL be checked separately with validation enabled through resource teardown.

#### Scenario: Performance is evaluated
- **WHEN** T1 timings are collected after warm-up with validation disabled for the performance run
- **THEN** the record contains p50/p95/p99 timings, separate waits and GPU shadow/whole-frame measurements, sample conditions, baseline comparison and an explicit result against each threshold

#### Scenario: Only automated tests are available
- **WHEN** unit and Vulkan smoke tests pass but visual acceptance or required GPU timing is unavailable
- **THEN** the missing evidence remains explicitly recorded and T1 is not described as accepted solely from those tests
