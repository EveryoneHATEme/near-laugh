# P07b Release measurements — 2026-09-08

All nine valid samples pass every unchanged P07a gate. Each records 10 seconds
warmup plus 60 seconds sampling at fullscreen 1920x1080/60 Hz FIFO, with Vulkan
validation disabled, the same eight lights/four D32 casters, initial doors and
fixed camera. Geometry and entries are unchanged; the actor lanes, offline
mixer and workload limits are detailed in [validation.md](validation.md).

Hardware queried after sampling: AMD Ryzen 5 4600H, 6 cores/12 threads;
AMD Radeon(TM) Graphics, driver 31.0.21921.1000; 16,505,966,592 bytes RAM;
1920x1080 at 60 Hz; Windows Balanced power scheme. These match P07a.
The renderer reports vendor 4098/device 5686, driver 8388887, 10 ns timestamp
period and 64 valid bits. The unused Honor virtual display is also installed.

Raw [compressed CSVs, logs and summaries](evidence/timings) retain every row.
The manifest verifies exact decompression with source paths and SHA-256.
Percentiles use nearest rank after warmup. CPU excludes measured fence,
acquire and present waits; GPU scopes use timestamps. Frame intervals are
successive CPU completion intervals, not display scanout measurements.

| Actors / run | CPU p95 | GPU p95 | Shadows p95 | Frame p50 / p95 / p99 |
| --- | ---: | ---: | ---: | --- |
| 0 / 1 | 0.720 | 9.727 | 1.839 | 16.679 / 17.318 / 17.635 |
| 0 / 2 | 0.716 | 9.692 | 1.838 | 16.686 / 17.334 / 17.679 |
| 0 / 3 | 0.719 | 9.804 | 1.855 | 16.681 / 17.355 / 17.701 |
| 1 / 1 | 1.268 | 10.226 | 2.076 | 16.680 / 17.366 / 17.646 |
| 1 / 2 | 1.254 | 10.183 | 2.057 | 16.672 / 17.409 / 17.725 |
| 1 / 3 | 1.201 | 10.160 | 2.075 | 16.692 / 17.349 / 17.683 |
| 4 / 1 | 2.419 | 12.654 | 3.683 | 16.681 / 17.587 / 17.967 |
| 4 / 2 | 2.501 | 12.540 | 3.595 | 16.704 / 17.643 / 18.100 |
| 4 / 3 | 2.413 | 12.638 | 3.683 | 16.696 / 17.716 / 18.109 |

All values are milliseconds. Every run has complete GPU timestamp availability,
a 1920x1080 sampled framebuffer and no non-submitted post-warmup rows.
Limits remain CPU/GPU p95 <=16.67 ms and frame p50/p95/p99 <=16.9/20/33.4 ms.

The following are the largest per-run p95 values in each actor-count group,
not pooled percentiles or additive attribution of a particular frame.

| CPU scope (ms) | Zero | One | Four |
| --- | ---: | ---: | ---: |
| Route decisions/restart | 0.0008 | 0.0085 | 0.0107 |
| Actor collision | 0.0012 | 0.0222 | 0.0337 |
| Pose, contacts and publication | 0.0005 | 0.0673 | 0.2107 |
| World/player/doors | 0.0452 | 0.0580 | 0.0645 |
| Audio handoff and offline mixing | 0.0201 | 0.0356 | 0.0592 |
| Deformation/conversion | 0.0000 | 0.4651 | 1.3193 |
| Mapped vertex upload | 0.0000 | 0.0429 | 0.2868 |

One actor traversed 15.7833 m and completed 21 legs in every run. Each of the
four actors traversed at least 15.0667 m, with 20–21 completed legs, retaining
standing turns, walks and interaction/foreground contention. Short lanes do
not claim the worst collision cost of every authored level.

The interrupted second baseline stopped at 32.767 seconds and remains in
[interrupted evidence](evidence/timings/interrupted); it is excluded from
these nine valid samples. Two startup-focus failures also remain retained.
No failed or interrupted run was overwritten or described as passing.

Debug/Release functional timing checks and six lossless measurement views
are retained under [checks](evidence/timings/checks). They are not performance
samples. No shaders, geometry/light budgets or timing gates were reduced.
The software costs fit this comparison; device callbacks, physical listening
and output latency remain unmeasured. T2 still needs P07c authoring acceptance.
