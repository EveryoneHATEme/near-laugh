# Independent character-authoring acceptance evidence

The retained scene is `../Нейтральный маршрут 02.level.json` (format v9).
It was created, changed, saved and reopened only through the level editor UI.
No scene JSON or runtime/editor source code was edited for this authoring run.
The older `../Нейтральный маршрут 01.level.json` is an earlier starter, not the
accepted second scene.

Final scene SHA-256:

```text
935d4eaba3587d73419a772aac9f597b74733bb0ab3becc1080f6790d5249b78
```

## Selected evidence

- `final-330-geometry-saved.png`: authored door and geometry saved.
- `final-354-lights-saved.png`: authored shadow-light properties.
- `final-386-cue1-fixed.png`, `final-377-cue2-final.png`: distinct spatial,
  non-looping footstep and essential interaction cues.
- `final-407-actor-links-clean.png`: actor, route and distinct sound-source links.
- `final-426-default-reopened.png`, `final-426-entry1-reopened.png`: named starts
  reopened through the UI without losing their positions/facing.
- `final-431-mark2-reopened.png`, `final-431-mark3-reopened.png`: corrected route
  coordinates reopened; later evidence uses this final saved scene.
- `section4-inside-idle-running-corrected.png`: actual mannequin mesh in the
  editor's silent clip snapshot, with the document still clean.
- `section4-route-running.png`, `section4-route-stopped-fresh.png`: transient
  schematic route inspection and its explicit no-collision/door/audio boundary.
- `section4-game-stationary-01.png`, `section4-game-stationary-02.png`: actor
  remains stopped before the stationary player, 10.6 seconds apart.
- `section4-door-view-01.png`, `section4-door-view-02.png`: after the player
  moves aside, the actor reaches the closed door and remains there, 14 seconds
  apart; its posed shadow is visible on the leaf.
- `section4-fresh-door-visible.png`: closed-door state before the final fresh run.
- `section4-fresh-door-finale-010.png`: opened door, actor walking through it,
  and coherent animated shadow on the leaf/floor.
- `section4-fresh-door-finale-039.png` through `043.png`: the short interaction
  caption is visible in frames 040-042 and absent on either side.
- `section4-final-pose.png`: final actor placement/idle presentation and shadow
  in the rear room after the route completes.

## Provenance and interpretation

The editor ran from `%TEMP%`, outside the repository. A read-only Windows process
query confirmed editor PID 24640 launched game PID 16696 with parent PID 24640
and the literal command line:

```text
"D:\programming\near-laugh\build\debug\bin\near_laugh.exe" --level "D:\programming\near-laugh\openspec\changes\add-character-authoring\evidence\Нейтральный маршрут 02.level.json" --entry "entry-1"
```

The final capture used a fresh ordinary Play process, PID 20936. An `E` press
and the 100-frame capture were issued in one uninterrupted command; no movement
or tool round trip separated them. The series ran from
`2026-09-09T12:55:24.5688762Z` to `2026-09-09T12:55:54.9166877Z`.
Frame 041 shows `Манекен: Тихий щелчок.`. Selected PNGs are unmodified copies.
Full working captures remain under `build/character-authoring-section4-resume`.

`authoring-actions.jsonl` and `game-actions.jsonl` retain the action history,
including failed inputs and non-acceptance attempts. In particular, the older
`section4-route-replay-*` series is not route-restart evidence: F5/F6/P/M are
fixture-only controls and have no such effect in ordinary `near_laugh`.
For a fresh ordinary run, close the child and use editor Play again. Default
`key` fields on a log row whose action is `capture` do not represent key input.

The user confirmed audible output during Play: `да, звук есть`. Screenshots
establish caption/visual behavior, not physical audibility or device latency.
No separate per-cue listening matrix, loopback recording or hardware-latency
measurement is claimed. See the parent validation record for the acceptance
decision, existing automated results and the retained P07b limitation.
