"""Deterministic v7 lighting migration used by packaged-level preparation."""
import json
from pathlib import Path
from level_characters_v9 import migrate_characters
from level_household_v10 import migrate_household
from level_narrative_v11 import migrate_narrative


def migrate_lighting(level):
    if level["version"] in (8, 9, 10, 11):
        return level
    if level["version"] != 7:
        raise ValueError("Preparation expects v7 through v11; legacy fixtures remain unchanged")
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
    current = migrate_narrative(migrate_household(migrate_characters(level)))
    Path(path).write_text(json.dumps(current, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    for name in ("prototype", "apartment-stairs", "audio-captions"):
        path = root / "resources/levels" / f"{name}.level.json"
        write_level(path, migrate_lighting(json.loads(path.read_text(encoding="utf-8"))))
