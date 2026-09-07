"""Generate the separate P04 fixture from the unchanged apartment geometry."""
import json
from pathlib import Path
from level_lighting_v8 import migrate_lighting, write_level

root = Path(__file__).resolve().parents[1]
level = migrate_lighting(json.loads((root / "resources/levels/apartment-stairs.level.json").read_text()))

def position(x, y, z):
    return dict(x=float(x), y=float(y), z=float(z))

identities = ["radio", "phone-ring", "footsteps", "phone-conversation", "invitation"]
points = [position(2.95, 3.9, -4), position(2.2, 3.9, -4), position(0, 3.2, -.5),
          position(2.2, 3.9, -4), position(-.3, 4.5, 3.7)]
level["audio"] = dict(
    cues=[dict(id=name, clip=name, caption=name,
               kind="ambience" if name == "radio" else "essential" if name in identities[1:3] else "dialogue",
               loop=name == "radio", spatial=True) for name in identities],
    sources=[dict(id=name, cue=name, position=point, gain=.65 if name == "radio" else 1.0,
                  near_distance=1.0, far_distance=25.0, autoplay=False) for name, point in zip(identities, points)],
    rooms=[dict(id="lena-room", center=position(-3.0625, 4.5, 3.5), half_extent=position(1.9375, 1.5, 2.5)),
           dict(id="corridor", center=position(0, 4.5, 0), half_extent=position(1.125, 1.5, 6)),
           dict(id="kitchen", center=position(3.0625, 4.5, -2.875), half_extent=position(1.9375, 1.5, 3.125))],
    connections=[dict(id="room-door", room_a="lena-room", room_b="corridor", door="lena-room", closed_gain=.2, open_gain=1.0),
                 dict(id="kitchen-opening", room_a="corridor", room_b="kitchen", door=None, closed_gain=1.0, open_gain=1.0),
                 dict(id="stairs", room_a="corridor", room_b=None, door=None, closed_gain=.8, open_gain=.8)])
write_level(root / "resources/levels/audio-captions.level.json", level)
