# Reference prototypes

These are exported from the design canvas. They are "Design Component" HTML files: the markup uses `{{holes}}`, and the logic sits in `renderVals()` inside the `<script type="text/x-dc">` block. They need the canvas runtime (`support.js`) to run, so treat them as **readable specs**, not runnable pages.

- `layouts-*.dc.html`: static layouts A and B per platform (rendered at 2×; all inner coordinates are native pixels).
- `animation-layout-a.dc.html`, `animation-layout-b.dc.html`: the 5-minute refinement animations on emery. `renderVals()` contains the exact motion maths.
- `pebble_time_pixels_v3.png`: basalt layouts rasterised at true pixels with the agreed rendering split (made by `tools/render_preview.py`).
