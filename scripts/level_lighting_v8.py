"""Deterministic v7 lighting migration used by packaged-level preparation."""
import json
from pathlib import Path
from level_characters_v9 import migrate_characters


def migrate_lighting(level):
    if level["version"] in (8, 9):
        return level
    if level["version"] != 7:
        raise ValueError("Preparation expects v7, v8 or v9; legacy fixtures remain unchanged")
    lights = level["environment_light"]["point_lights"]
    switch = level["light_switch"]
    for index, light in enumerate(lights):
        lights[index] = dict(id=f"point-light-{index}", **light,
                             initially_on=True, casts_shadows=False)
    switches = []
    if switch is not None:
        target = lights[switch["point_light_index"]]
        target["initially_on"] = switch["initially_on"]
        switches.append(dict(id="light-switch-0", position=switch["position"],
                             yaw_degrees=switch["yaw_degrees"], light_id=target["id"]))
    # Preserve the canonical root order while replacing the old field.
    return {key if key != "light_switch" else "light_switches":
            8 if key == "version" else switches if key == "light_switch" else value
            for key, value in level.items()}


def write_level(path, level):
    Path(path).write_text(json.dumps(migrate_characters(level), indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    for name in ("prototype", "apartment-stairs", "audio-captions"):
        path = root / "resources/levels" / f"{name}.level.json"
        write_level(path, migrate_lighting(json.loads(path.read_text(encoding="utf-8"))))
