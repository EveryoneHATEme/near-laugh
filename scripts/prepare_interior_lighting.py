"""Build neutral T1 lighting fixtures from the authored apartment geometry."""
import copy
import json
from pathlib import Path
from level_lighting_v8 import migrate_lighting, write_level

root = Path(__file__).resolve().parents[1]
levels = root / "resources/levels"
level = migrate_lighting(json.loads((levels / "apartment-stairs.level.json").read_text(encoding="utf-8")))


def position(x, y, z):
    return dict(x=float(x), y=float(y), z=float(z))


def light(name, xyz, color, intensity, radius, shadow):
    return dict(id=name, position=position(*xyz), color=color, intensity=float(intensity),
                radius=float(radius), initially_on=True, casts_shadows=shadow)


level["environment_light"] = dict(point_lights=[
    light("room-ceiling", (-3, 5.25, 3.5), [1.0, .82, .64], .9, 7, True),
    light("corridor-ceiling", (0, 5.3, .5), [.74, .83, 1.0], .85, 8, True),
    light("kitchen-ceiling", (3, 5.25, -2.8), [1.0, .93, .78], .9, 7, True),
    light("landing-ceiling", (0, 3.3, -15.5), [.65, .78, 1.0], 1.0, 9, True),
    light("bedside-fill", (-4.25, 4.2, 4.6), [1.0, .54, .29], .18, 2.5, False),
    light("stairs-fill", (0, 4.5, -6.5), [.72, .81, 1.0], .3, 5, False)],
    ambient_intensity=.035)
level["light_switches"] = [
    dict(id="room-corridor-switch", position=position(-.721, 4.3, 4.05), yaw_degrees=-90.0, light_id="room-ceiling"),
    dict(id="room-inner-switch", position=position(-1.261, 4.3, 5.2), yaw_degrees=-90.0, light_id="room-ceiling"),
    dict(id="corridor-switch", position=position(.979, 4.3, 1.2), yaw_degrees=-90.0, light_id="corridor-ceiling"),
    dict(id="kitchen-switch", position=position(1.261, 4.3, -4.8), yaw_degrees=90.0, light_id="kitchen-ceiling"),
    dict(id="landing-switch", position=position(1.979, 1.4, -16.2), yaw_degrees=-90.0, light_id="landing-ceiling"),
    dict(id="bedside-switch", position=position(-3, 4.3, 1.141), yaw_degrees=0.0, light_id="bedside-fill"),
    dict(id="stairs-switch", position=position(-.879, 4.3, -6.5), yaw_degrees=90.0, light_id="stairs-fill")]
level["doors"].append(dict(id="kitchen-door", hinge_position=position(1.12, 3.02, -2.93),
    closed_yaw_degrees=-90.0, width=1.26, height=2.1, thickness=.06,
    open_angle_degrees=90.0, speed_degrees_per_second=90.0, lock_side="none",
    initially_open=True, initially_locked=False))
# Give the new door's open leaf clearance from the south kitchen chair.
next(prop for prop in level["props"] if prop["id"] == "kitchen-chair-south")["translation"]["x"] = 3.2
level["entries"] += [
    dict(id="corridor-inspection", foot_position=position(0, 3, 4.7), yaw_degrees=-90.0),
    dict(id="kitchen-inspection", foot_position=position(3, 3, -1.2), yaw_degrees=-90.0)]
# Neutral fixtures contain no P04 authored audio sequence.
level["audio"] = dict(cues=[], sources=[], rooms=[], connections=[])
write_level(levels / "interior-lighting.level.json", level)
capacity = copy.deepcopy(level)
capacity["environment_light"]["point_lights"] += [
    light("room-fill", (-2, 4.7, 2), [.66, .75, 1.0], .15, 3, False),
    light("kitchen-fill", (4, 4.3, -4.5), [1.0, .65, .38], .15, 3, False)]
write_level(levels / "interior-lighting-capacity.level.json", capacity)
