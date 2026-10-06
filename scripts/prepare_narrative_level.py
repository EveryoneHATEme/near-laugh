"""Prepare ordinary neutral T4 scenes. Runtime behavior comes only from definitions."""

import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def event(name, trigger, steps, cancel=None, repeat="once"):
    return dict(id=name, trigger=trigger, guards=[], cancel=cancel,
                repeat=repeat, steps=steps)


def scenes(root=ROOT):
    level = json.loads((root / "resources/levels/household-interactions.level.json").read_text(encoding="utf-8"))
    level["characters"]["actors"][0]["initial_route"] = None
    level["household"]["boxes"] = [level["household"]["boxes"][1]]
    level["household"]["radios"][0]["initially_on"] = True
    level["household"]["documents"][0]["title"] = "Проверка последовательности"
    level["household"]["documents"][0]["pages"] = [
        "Войди в область перед столом: включится свет, прозвучит сигнал, затем персонаж пройдёт по комнате.",
        "Выключи радио, чтобы отменить оставшиеся действия. Чтение сохраняет ход мира. P — пауза проверки; M — без звука."]
    level["environment_light"]["point_lights"][0]["initially_on"] = False
    level["audio"]["cues"].append(dict(id="sequence-cue", clip="character-interaction",
        caption="character-interaction", kind="essential", loop=False, spatial=True))
    level["audio"]["sources"].append(dict(id="sequence-source", cue="sequence-cue",
        position=dict(x=-2., y=1., z=-1.), gain=1., near_distance=1., far_distance=20., autoplay=False))
    cancel = [dict(kind="radio", radio="receiver", enabled=False)]
    steps = [dict(kind="set_light", light="table-light", enabled=True),
             dict(kind="play_cue", source="sequence-source"),
             dict(kind="delay", seconds=2.),
             dict(kind="run_route", actor="walker", route="walk"),
             dict(kind="set_fact", fact="completed", value=True)]
    level["narrative"] = dict(facts=[dict(id="completed", initial_value=False)],
        regions=[dict(id="approach", center=dict(x=-2., y=1., z=1.5),
                      half_extent=dict(x=.8, y=1., z=.4))],
        events=[event("neutral-sequence", dict(kind="region_entry", region="approach"), steps, cancel)])
    result = {"narrative-t4": level}

    door = copy.deepcopy(level)
    door["narrative"]["events"][0]["steps"] = [
        dict(kind="set_door_open", door="contact-door", open=True),
        dict(kind="wait_until", predicates=[dict(kind="door_endpoint", door="contact-door", open=True)]),
        *steps]
    result["narrative-t4-door"] = door
    refused = copy.deepcopy(door)
    refused["doors"][0]["lock_side"] = "positive-z"
    refused["doors"][0]["initially_locked"] = True
    result["narrative-t4-refusal"] = refused

    interactions = copy.deepcopy(level)
    interactions["narrative"]["facts"] += [dict(id="box-accepted", initial_value=False),
                                              dict(id="document-opened", initial_value=False)]
    for name, kind, target, action in (("box-accepted", "box", "floor-box", "box_pickup"),
                                       ("document-opened", "document", "letter", "document_open")):
        interactions["narrative"]["events"].append(event(name,
            dict(kind="interaction", target_kind=kind, target=target, action=action),
            [dict(kind="set_fact", fact=name, value=True)]))
    result["narrative-t4-interactions"] = interactions
    busy = copy.deepcopy(level)
    busy["narrative"]["events"] += [event("earlier-cue", dict(kind="scene_entry"),
        [dict(kind="play_cue", source="sequence-source")])]
    result["narrative-t4-busy"] = busy
    return result


def main():
    for name, scene in scenes().items():
        path = ROOT / "resources/levels" / (name + ".level.json")
        path.write_text(json.dumps(scene, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\n")
        print(path.name)


if __name__ == "__main__":
    main()
