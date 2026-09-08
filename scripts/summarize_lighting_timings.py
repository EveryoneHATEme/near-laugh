"""Summarize raw P10 CSV samples; keep unavailable GPU results explicit."""

import argparse
import csv
import gzip
import json
import math
from pathlib import Path


def summarize(path: Path):
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8", newline="") as source:
        raw = list(csv.DictReader(source))
    rows = [row for row in raw if 10 <= float(row["elapsed_seconds"]) < 70]
    if not rows:
        raise ValueError(f"{path}: no post-warmup samples")
    metrics = {}
    names = ["cpu_active_ms", "gpu_frame_ms", "gpu_shadow_ms", "interval_ms",
             "fence_ms", "acquire_ms", "present_ms"]
    names += [name for name in ("character_deformation_ms", "character_upload_ms")
              if name in raw[0]]
    for name in names:
        values = sorted(float(row[name]) for row in rows if row[name] != "")
        if any(not math.isfinite(value) or value < 0 for value in values):
            raise ValueError(f"{path}: invalid {name} value")
        metrics[name] = {
            "available": len(values), "unavailable": len(rows) - len(values),
            **({f"p{p}": values[max(0, math.ceil(p / 100 * len(values)) - 1)]
                for p in (50, 95, 99)} if values else {}),
            "max": max(values) if values else None,
        }
    gates = {}
    for name, percentile, limit in (
        ("cpu_active_ms", "p95", 16.67), ("gpu_frame_ms", "p95", 16.67),
        ("interval_ms", "p50", 16.9), ("interval_ms", "p95", 20),
        ("interval_ms", "p99", 33.4),
    ):
        metric = metrics[name]
        gates[f"{name}.{percentile}<={limit}"] = (
            "unavailable" if metric["unavailable"] else
            "pass" if metric[percentile] <= limit else "FAIL")
    actions = {}
    for row in rows:
        action = actions.setdefault(row["action"], {"frames": 0, "max_cpu_ms": 0,
                                                    "max_gpu_ms": None,
                                                    "max_interval_ms": 0})
        action["frames"] += 1
        action["max_cpu_ms"] = max(action["max_cpu_ms"], float(row["cpu_active_ms"]))
        action["max_interval_ms"] = max(action["max_interval_ms"], float(row["interval_ms"]))
        if row["gpu_frame_ms"]:
            action["max_gpu_ms"] = max(action["max_gpu_ms"] or 0, float(row["gpu_frame_ms"]))
    return {
        "path": path.as_posix(), "raw_rows": len(raw), "sample_rows": len(rows),
        "first_sample_seconds": float(rows[0]["elapsed_seconds"]),
        "last_sample_seconds": float(rows[-1]["elapsed_seconds"]),
        "percentile_method": "nearest rank",
        "non_submitted_sample_rows": sum(row["submitted"] != "1" for row in rows),
        "framebuffer_sizes": sorted({f'{row["width"]}x{row["height"]}' for row in rows}),
        "metrics": metrics, "gates": gates, "actions": actions,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", type=Path, nargs="+")
    args = parser.parse_args()
    print(json.dumps([summarize(path) for path in args.files], indent=2))
