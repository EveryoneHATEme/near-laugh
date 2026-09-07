## ADDED Requirements

### Requirement: Owned audio and cue coordination
Runtime composition SHALL own audio playback, cue time, source overrides, caption selection and mute/suspension state independently of immutable level definitions. Audio backend types SHALL remain outside public runtime, world and frame boundaries. The listener SHALL use the current displayed eye pose and transmission SHALL use accepted door poses. Selected audio/caption/font dependencies SHALL resolve from the executable-relative resource root and validate before playback, preserving native Unicode paths. Device status SHALL NOT control application lifetime or story ordering. Partial startup and normal shutdown SHALL release audio users before their referenced data and stop device activity before destruction.

#### Scenario: Current view and door state are submitted
- **WHEN** a renderable iteration has an interpolated view and accepted door state
- **THEN** audio uses those same poses while the renderer receives only resolved text presentation and existing geometric/light data

#### Scenario: Resource path contains Unicode
- **WHEN** the selected level and executable resources reside under paths with non-ASCII characters
- **THEN** selected clips, captions and font load using their literal resolved paths and failures identify those paths

#### Scenario: Startup fails after audio initialization
- **WHEN** renderer initialization fails after audio resources have initialized
- **THEN** all playback activity stops and audio resources and prior owners are released safely without entering the main loop

### Requirement: Audio coordination across waits and presentation outcomes
Before entering a minimized event wait, runtime composition SHALL suspend audio and cue time, retaining cue offsets and identities. Restoration SHALL resume without counting blocked time. Existing inactive-input consumption SHALL prevent delayed interaction or fixture actions. Rendered, skipped and recovered outcomes SHALL NOT replay cues, reset source motion or determine cue completion. Close SHALL prevent further cue starts and begin orderly teardown. Cursor release SHALL retain its existing control behavior without becoming a session pause.

#### Scenario: Window waits while a cue is active
- **WHEN** the framebuffer becomes zero during speech
- **THEN** audio and captions suspend before the event wait and restoration continues the same cue from the retained offset

#### Scenario: Swapchain recovers after a completed cue
- **WHEN** presentation recovery follows a completed one-shot
- **THEN** the next frame contains only currently active text and the completed audio is not started again

#### Scenario: Close occurs while minimized
- **WHEN** a close event ends a minimized wait with suspended voices
- **THEN** shutdown releases those voices without resuming them or replaying waited input
