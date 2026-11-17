# MDR Watchface — project notes for Claude Code

A Pebble watchface inspired by the *Severance* MDR screens, published as **Refinement** (by Andrew Whaley). **Read `docs/DESIGN_SPEC.md` before writing code.** It has the geometry, colours, fonts and animation timings for each platform.

## Targets
- `basalt` (Pebble Time, 144×168), `emery` (Pebble Time 2, 200×228), `gabbro` (Pebble Round 2, 260×260 round).
- Use the current PebbleOS SDK from Core Devices / Rebble. Confirm the platform names against the installed SDK before relying on them.

## Rendering decisions (fixed)
- Time and date: **Michroma**, anti-aliased, sub-pixel positioned, scalable for the animation. Use **pebble-fctx**, which requires converting Michroma to its font format (`tools/build_fonts.py`). The outlines are emboldened by 144 units at build time for legibility.
- Small text (battery %, bin labels): **Silkscreen**, 1-bit, at native 8px (16px on emery/gabbro).
- Number field: custom thin 5×7 pixel digits in 14px cells (spec §4.2), blitted into one 1-bit bitmap. Never draw per-cell text in the frame loop.
- Only colours from the Pebble 64-colour palette (see spec §2). No alpha. Colours come from the user's theme (`src/c/theme.c`), never hard-coded.

## Implementation guidance
- Put the layout numbers in a per-platform struct table (`PBL_PLATFORM_*` / `PBL_IF_ROUND_ELSE`). Don't scatter magic numbers.
- Position glyphs by **cap centre** (spec §3), and use tabular digit slots.
- Run the animation on a timer (~33 ms). Keep positions in fixed-point and redraw only the dirty layers.
- Build both layouts A ("Standard") and B ("Large"). The user picks one in settings (spec §8). A is the default.
- Verify each platform in the emulator and compare screenshots with `docs/reference/pebble_time_pixels_v3.png` and the prototypes. `tools/emu_shot.sh` installs the build and screenshots it at 10:09 and 73%. `tools/compare_ref.py` diffs a screenshot against the reference. `MDR_LAYOUT=B` and `MDR_THEME=<n>` set the defaults at build time, for previews. For the animation, `MDR_FREEZE=<design ms>` holds one frame of a fixed run (10:10 to 10:15), and `MDR_DEMO=1` plays it on a tap (`pebble emu-tap`) and logs the frame count.

## Reference material
- `docs/reference/*.dc.html`: the interactive design prototypes (HTML + JS). The animation maths in their `renderVals()` is the source of truth for motion.
- `store/<platform>/`: app-store screenshots (`standard.png`, `large.png`) and the Standard animation (`standard-refinement.gif`) at native resolution, in true palette colours. Regenerate them after visual changes with `python3 tools/store_assets.py screenshots` and `python3 tools/store_assets.py gif`, which use an `MDR_CAPTURE` build that steps the animation one frame per emulator tap. Run a plain `pebble build` afterwards. `tools/store_art.py` makes the banner and icons. `store/README.md` is the listing sheet (text, release notes, which file goes in which store slot).
- `tools/render_preview.py`: rasterises the basalt layouts at true pixels in the 64-colour palette (needs Pillow and the two TTFs).
- Fonts (OFL): Michroma `https://raw.githubusercontent.com/google/fonts/main/ofl/michroma/Michroma-Regular.ttf`, Silkscreen `https://raw.githubusercontent.com/google/fonts/main/ofl/silkscreen/Silkscreen-Regular.ttf`. Include their OFL licence files with the resources.
- Don't use the Lumon logo or any other show branding. Keep the style "inspired by".
