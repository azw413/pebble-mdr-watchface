# MDR Watchface — project notes for Claude Code

A Pebble watchface inspired by the *Severance* MDR screens. **Read `docs/DESIGN_SPEC.md` before writing code.** It has the geometry, colours, fonts and animation timings for each platform.

## Targets
- `basalt` (Pebble Time, 144×168), `emery` (Pebble Time 2, 200×228), `gabbro` (Pebble Round 2, 260×260 round).
- Use the current PebbleOS SDK from Core Devices / Rebble. Confirm the platform names against the installed SDK before relying on them.

## Rendering decisions (fixed)
- Time and date: **Michroma**, anti-aliased, sub-pixel positioned, scalable for the animation. Use **pebble-fctx**, which requires converting Michroma to its font format; check the current toolchain for that.
- Small text (battery %, bin labels, number field): **Silkscreen**, 1-bit, at native 8px (16px on emery/gabbro).
- Number field: one pre-rendered bitmap or cached layer, never per-cell text drawing in the frame loop.
- Only colours from the Pebble 64-colour palette (see spec §2). No alpha.

## Implementation guidance
- Put the layout numbers in a per-platform struct table (`PBL_PLATFORM_*` / `PBL_IF_ROUND_ELSE`). Don't scatter magic numbers.
- Position glyphs by **cap centre** (spec §3), and use tabular digit slots.
- Run the animation on a timer (~33 ms). Keep positions in fixed-point and redraw only the dirty layers.
- Build both layouts A and B. Default to A until settings exist.
- Verify each platform in the emulator and compare screenshots with `docs/reference/pebble_time_pixels_v3.png` and the prototypes.

## Reference material
- `docs/reference/*.dc.html`: the interactive design prototypes (HTML + JS). The animation maths in their `renderVals()` is the source of truth for motion.
- `tools/render_preview.py`: rasterises the basalt layouts at true pixels in the 64-colour palette (needs Pillow and the two TTFs).
- Fonts (OFL): Michroma `https://raw.githubusercontent.com/google/fonts/main/ofl/michroma/Michroma-Regular.ttf`, Silkscreen `https://raw.githubusercontent.com/google/fonts/main/ofl/silkscreen/Silkscreen-Regular.ttf`. Include their OFL licence files with the resources.
- Don't use the Lumon logo or any other show branding. Keep the style "inspired by".
