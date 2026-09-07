# Lighting capture evidence

`final-d32` contains the final 1920x1080 GPU readbacks and all material controls.
`d16-quantization` contains the passing D16 fallback controls. Re-run their
stored-RGB comparisons with `scripts/analyze_lighting_captures.py <directory>`.
The PNGs were encoded losslessly from framebuffer PPMs and decoded again to
verify exact RGB equality before removing the duplicate PPMs.

Each case's `.txt` records camera, field of view, framebuffer and accepted door
angles. In a pair, `-on`/`-off` changes the first point light only; remaining
lights use the case's authored initial values. In `furnished-all-off` and
`furnished-zero-ambient`, all other lights are off, so the `-off` member is the
fully disabled control. `furnished-editor` uses the same camera and initial
values as `furnished-on` without overlays/UI.

Earlier directories retain diagnostic images and their patch reports:
`first` exposed receiver self-shadow striping; `receiver-plane` fixed it;
`interior-taps` checked the shader arithmetic optimization; `complete-d32-4`
added the material controls; `complete-d16` exposed D16 quantization before
the final tolerance. Their results do not replace the final controls.

`editor-play` contains ordinary desktop screenshots and the saved v8 snapshot
from real Save As / Save and Play / Undo / fresh-process checks. Those images
use the normal window sizes and are separate from the 1920x1080 patch tests.
See [validation.md](../validation.md) for observations, limits and timing data.
