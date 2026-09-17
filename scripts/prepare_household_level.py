"""Write neutral P06 authored scenes; runtime never dispatches on their names."""

import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def position(x, y, z):
    return dict(x=float(x), y=float(y), z=float(z))


def solid(center, extent, kind="boundary", material="wallpaper"):
    return dict(center=position(*center), half_extent=position(*extent),
                color=[190, 184, 170, 255], kind=kind, material=material)


def parcel(name, center, yaw=0):
    return dict(id=name, center=position(*center), yaw_degrees=float(yaw))


def main():
    level = dict(version=10, terrain=None, solids=[
        solid((0, -.25, 0), (6, .25, 7), "floor", "wood-floor"),
        solid((0, 4.15, 0), (6, .15, 7)),
        solid((-6, 2, 0), (.15, 2, 7)), solid((6, 2, 0), (.15, 2, 7)),
        solid((0, 2, -7), (6, 2, .15)), solid((0, 2, 7), (6, 2, .15)),
        # Deliberately supported 0.02 m static wall for close hold/throw checks.
        solid((1.5, 1, -1), (.01, 1, 1.2)),
    ], entries=[dict(id="explore", foot_position=position(-2, 0, 2.8), yaw_degrees=-90.)],
        default_entry="explore", environment_light=dict(ambient_intensity=.05, point_lights=[]),
        props=[], light_switches=[], doors=[dict(id="contact-door", hinge_position=position(-.5, .02, -2),
            closed_yaw_degrees=0., width=1., height=2.1, thickness=.04,
            open_angle_degrees=90., speed_degrees_per_second=90., lock_side="none",
            initially_open=False, initially_locked=False)],
        audio=dict(cues=[dict(id="radio-loop", clip="radio", caption="radio", kind="ambience", loop=True, spatial=True)],
                   sources=[dict(id="receiver-loop", cue="radio-loop", position=position(5, 1, 5),
                                 gain=.6, near_distance=1., far_distance=20., autoplay=False)],
                   rooms=[], connections=[]),
        characters=dict(actors=[dict(id="walker", model="test-mannequin", initial_mark="route-start",
                                    initial_route="walk", speed=.75, footstep_source=None, interaction_source=None)],
                        marks=[dict(id="route-start", feet_position=position(3, 0, -4), yaw_degrees=0.),
                               dict(id="route-end", feet_position=position(3, 0, 4), yaw_degrees=180.)],
                        routes=[dict(id="walk", actor="walker", marks=["route-end"], final_clip=None),
                                dict(id="walk-back", actor="walker", marks=["route-start"], final_clip=None)]),
        household=dict(boxes=[parcel("table-box", (-2.5, .871, -1), 15),
                              parcel("floor-box", (-1.4, .15, 1), 25),
                              parcel("door-box", (0, .15, -3)),
                              parcel("route-box", (3, .15, 0))],
                       documents=[dict(id="letter", position=position(-2, .724, -.7), yaw_degrees=0.,
                            title="Записка на столе",
                            pages=["Ёжик оставил коробки у стола. Подними одну: E. Она остаётся физическим предметом и встречает преграды.",
                                   "E — опустить коробку. Правая кнопка мыши — бросить. Сначала освободи руки, чтобы читать, открыть дверь или включить радио.",
                                   "Радио стоит на столе. Чтение не останавливает мир. P — пауза проверки; M — без звука. A/D — страницы, E/Escape — закрыть."])],
                       radios=[dict(id="receiver", prop="radio-prop", source="receiver-loop", initially_on=False)]))
    table_boxes = [((0, .69, 0), (.837, .031, .548)),
                   ((-.72, .33, -.43), (.045, .33, .045)),
                   ((.72, .33, -.43), (.045, .33, .045)),
                   ((-.72, .33, .43), (.045, .33, .045)),
                   ((.72, .33, .43), (.045, .33, .045))]
    level["props"] = [dict(id="table", model="apartment-table", translation=position(-2, 0, -1),
                           yaw_degrees=0., uniform_scale=1., collision_boxes=[
                               dict(center=position(*center), half_extent=position(*extent)) for center, extent in table_boxes]),
                      dict(id="radio-prop", model="apartment-radio", translation=position(-1.5, .721, -1.3),
                           yaw_degrees=0., uniform_scale=1., collision_boxes=[])]
    for name, point in (("table-light", (-2, 3.3, -.5)), ("route-light", (3, 3.3, 1))):
        level["environment_light"]["point_lights"].append(dict(id=name, position=position(*point),
            color=[1., .9, .75], intensity=.9, radius=9., initially_on=True, casts_shadows=True))

    baseline = copy.deepcopy(level)
    baseline["household"]["boxes"] = []
    capacity = copy.deepcopy(level)
    # Sixteen initially awake airborne bodies. A measurement driver may apply
    # documented deterministic impulses; this file contains initial state only.
    capacity["household"]["boxes"] = [parcel(f"stress-{i:02}",
        (-4.5 + (i % 4) * .5, 1.4 + (i // 4) * .4, 3.5), i * 11) for i in range(16)]
    for name, document in (("household-interactions", level),
                           ("household-baseline", baseline), ("household-capacity", capacity)):
        path = ROOT / "resources/levels" / f"{name}.level.json"
        path.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n",
                        encoding="utf-8", newline="\n")
        print(f"{path.name}: {len(document['household']['boxes'])} authored boxes")


if __name__ == "__main__":
    main()
