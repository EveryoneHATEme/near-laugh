"""Write neutral P07b authored routes; no filename-selected runtime behavior."""

import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def position(x, y, z):
    return dict(x=float(x), y=float(y), z=float(z))


def solid(center, extent, kind="boundary", material="wallpaper"):
    return dict(center=position(*center), half_extent=position(*extent),
                color=[190, 184, 170, 255], kind=kind, material=material)


def mark(name, xyz, yaw):
    return dict(id=name, feet_position=position(*xyz), yaw_degrees=float(yaw))


def add_actor(level, name, initial, marks, speed=1.):
    chars = level["characters"]
    chars["actors"].append(dict(id=name, model="test-mannequin", initial_mark=initial,
                               initial_route=name + "-route", speed=speed,
                               footstep_source=name + "-step", interaction_source=name + "-touch"))
    chars["routes"].append(dict(id=name + "-route", actor=name, marks=marks, final_clip="interact"))
    chars["routes"].append(dict(id=name + "-return", actor=name, marks=[initial], final_clip=None))
    origin = next(m["feet_position"] for m in chars["marks"] if m["id"] == initial)
    for role in ("step", "touch"):
        level["audio"]["sources"].append(dict(id=name + "-" + role, cue=role,
            position=copy.deepcopy(origin), gain=1., near_distance=1., far_distance=16., autoplay=False))


def main():
    level = dict(version=9, terrain=None, solids=[
        solid((0, -.25, 0), (6, .25, 7), "floor", "wood-floor"),
        solid((0, 4.15, 0), (6, .15, 7)),
        solid((-6, 2, 0), (.15, 2, 7)), solid((6, 2, 0), (.15, 2, 7)),
        solid((0, 2, -7), (6, 2, .15)), solid((0, 2, 7), (6, 2, .15)),
        solid((0, .10, 1.3), (1.3, .10, .3), "walkable_step", "wood-floor"),
        solid((0, .20, 1.9), (1.3, .20, .3), "walkable_step", "wood-floor"),
        solid((0, .30, 4.1), (1.3, .30, 1.9), "floor", "wood-floor"),
        solid((-1.2, 1.8, 3.3), (.15, 1.2, .15)),
        solid((1.2, 1.8, 3.3), (.15, 1.2, .15)),
    ], entries=[dict(id="view", foot_position=position(-4.5, 0, -5.5), yaw_degrees=55.)],
        default_entry="view", environment_light=dict(ambient_intensity=.035, point_lights=[]),
        props=[], light_switches=[], doors=[dict(id="route-door", hinge_position=position(-.9, .62, 3.3),
            closed_yaw_degrees=0., width=1.8, height=2.1, thickness=.06,
            open_angle_degrees=-90., speed_degrees_per_second=90., lock_side="none",
            initially_open=False, initially_locked=False)],
        audio=dict(cues=[
            dict(id="step", clip="character-footstep", caption=None, kind="ambience", loop=False, spatial=True),
            dict(id="touch", clip="character-interaction", caption="character-interaction", kind="essential", loop=False, spatial=True)
        ], sources=[], rooms=[], connections=[]),
        characters=dict(actors=[], marks=[mark("start", (-3, 0, -4), 0),
            mark("corner", (-3, 0, 0), 90), mark("stairs", (0, 0, 0), 0),
            mark("landing", (0, .6, 2.65), 0), mark("touch", (0, .6, 5.3), 90)], routes=[]))
    for name, xyz, color in (("front", (-2, 3.5, -3), [1., .85, .65]),
                              ("stairs", (1, 3.5, 1), [.7, .83, 1.]),
                              ("back", (-2, 3.5, 5), [1., .92, .8])):
        level["environment_light"]["point_lights"].append(dict(id=name,
            position=position(*xyz), color=color, intensity=1., radius=9., initially_on=True, casts_shadows=True))
    add_actor(level, "walker", "start", ["corner", "stairs", "landing", "touch"])
    four = copy.deepcopy(level)
    four["characters"]["marks"] += [mark("east-start", (3.5, 0, -4), 0),
        mark("east-end", (3.5, 0, 5), 180), mark("north-start", (-4.5, 0, 2), 0),
        mark("south-start", (-4.5, 0, 5), 180)]
    add_actor(four, "east", "east-start", ["east-end"], .75)
    add_actor(four, "north", "north-start", ["south-start"])
    add_actor(four, "south", "south-start", ["north-start"])
    for name, document in (("scripted-characters", level), ("scripted-characters-four", four)):
        (ROOT / "resources/levels" / (name + ".level.json")).write_text(
            json.dumps(document, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
