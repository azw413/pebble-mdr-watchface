# MDR Watchface — Design Spec

A large, legible Pebble watchface inspired by the Macrodata Refinement (MDR) screens in *Severance*. This spec captures every decision made in the design mockups. The interactive prototypes are in `docs/reference/`, and their numbers match this file.

Targets: **Pebble Time** (`basalt`, 144×168), **Pebble Time 2** (`emery`, 200×228) and **Pebble Round 2** (`gabbro`, 260×260, round). All three have 64-colour displays.

> All coordinates are native screen pixels, with the origin top-left. "Cap centre y" is the vertical centre of a digit's cap height, which is where the eye reads the digit's centre. Position glyphs by this value, not by line box (see §3).

---

## 1. Two layouts (user-selectable later)

| | **A — MDR** | **B — Large display** |
|---|---|---|
| Time | One line `HH:MM` | Hours stacked over minutes, no divider |
| Date | Under the time, inside the band | Bottom of screen on a black backing box |
| Bins | 4 bins (`01`–`04`) along the bottom | None |
| 5-min animation | Old digits fly into a random bin | Old digits fall off the bottom |

Both layouts share: a black background, a dim background **number field**, a black **band** behind the time (1px accent lines top and bottom), and a **battery header** at the top.

## 2. Palette (Pebble 64-colour only)

| Role | Hex | Pebble constant |
|---|---|---|
| Background, band and box fills | `#000000` | `GColorBlack` |
| Time digits, small labels | `#AAFFFF` | `GColorCeleste` |
| Date, band lines, box outlines, battery fill | `#55AAAA` | `GColorCadetBlue` |
| Number field (rest), bin bar track | `#005555` | `GColorMidnightGreen` |
| Number field (during animation) | `#00AAAA` | `GColorTiffanyBlue` |
| Emerging-digit highlight, bin flash | `#55FFFF` / `#AAFFFF` | `GColorElectricBlue` / `GColorCeleste` |
| Battery ≤ 20% fill | `#FF5555` | `GColorSunsetOrange` |

The date stays deliberately subdued in Cadet Blue.

This is the default **cyan** theme. Users can pick another base colour in settings (§8); every theme keeps these roles and the same intensity steps (see `src/c/theme.c`).

## 3. Typography and rendering split

| Use | Font | Rendering |
|---|---|---|
| Time digits, date | **Michroma** (Google Fonts, OFL) | **Anti-aliased, sub-pixel positioned** (pebble-fctx) |
| Battery %, bin labels | **Silkscreen** (Google Fonts, OFL) | **1-bit**, at native 8px, or 16px (exactly 2×) on emery/gabbro |
| Number field | Custom thin **5×7** pixel digits (1px strokes) | **1-bit**, drawn straight into the field bitmap |

Why: when rasterised at true resolution, Michroma is crisp at 17px and up, but turns to mush at 7–8px. Silkscreen is a pixel font designed for 8px. Anti-aliasing is required on the large Michroma digits.

### Michroma metrics (unitsPerEm 2048)
- hhea ascent 2368, descent −544. Cap height 1536 (**0.75 em**).
- Digit advance **1948 (0.951 em)**. Exceptions: `4` is 2048, and `7` and `9` are 1984. Colon advance 448 (0.219 em).
- **Use tabular slots**: give every digit a fixed slot 0.951 em wide and centre each glyph's ink in its slot, so the time never shifts sideways. Note that `1` has a large left side bearing (ink spans 368–1730).
- In a CSS-style line box, Michroma's caps sit **0.07 em below** the box centre. On Pebble, position by the cap centre directly: baseline = capCentreY + 0.375 em.
- Rough width of `HH:MM` ≈ 4.02 em, or about 4.17 em for times with many 4s.

### Silkscreen metrics (unitsPerEm 1000)
- Ascent 1030, descent 250. Caps are **5px tall at 8px** and **10px at 16px**.
- Centre labels by their cap height, not their line height. This fixed the off-centre battery percentage.
- `100%` is 24px wide at 8px and 48px at 16px. Reserve that width so the battery bar never jumps.

## 4. Per-platform geometry

### 4.1 Common: battery header
A box with a 1px accent outline and black fill. Inside it, from left to right: the percentage (Silkscreen, reserved width), a gap, then a battery bar (1px outline, 1px inset, fill = level %, coloured Sunset Orange at ≤ 20%), then a 2px "nub" on the right end.

| | basalt | emery | gabbro A | gabbro B |
|---|---|---|---|---|
| Box | x5 y5 134×15 | x7 y7 186×20 | x65 y30 130×20 | x75 y24 110×18 |
| % font, reserved width | SS 8px, 24px | SS 16px, 48px | SS 16px, 48px | SS 16px, 48px |
| Padding, gap | 4, 4 | 6, 6 | 6, 6 | 5, 5 |
| Bar outer height, fill height | 7, 3 | 10, 6 | 10, 6 | 9, 5 |
| Nub | 2×3 | 2×4 | 2×4 | 2×4 |

### 4.2 Number field
A grid of random digits (seeded) in thin 5×7 pixel digits, Midnight Green. **Draw it as one pre-rendered bitmap** (or cached layer), not as hundreds of text calls. The animation needs to brighten it (recolour it, or swap the bitmap's palette to Tiffany Blue) and to hide individual cells.

Changed from the original Silkscreen 8px field (5px caps), which read as too small: Silkscreen is only crisp at 8 and 16px, so the field uses its own 7px-tall digits instead. Each digit is centred in a 14×14 cell. The columns are centred on the screen. Each layout has its own grid y offset, chosen so the field gaps show whole digits rather than ones cut by the header, band, bins or date box.

| | basalt | emery | gabbro |
|---|---|---|---|
| Grid | 10 cols × 12 rows, x0 2 | 14 × 16, x0 2 | 18 × 18, x0 4, clipped to the circle by the display |
| Grid y0, A / B | 3 / 3 | 2 / 11 | 4 / 11 |
| Seed | 7 | 11 | 13 |

### 4.3 Layout A — MDR

| | basalt | emery | gabbro |
|---|---|---|---|
| Band (y range) | 52–127 | 74–173 | 88–183 |
| Time font, cap centre y | Michroma **32px** (34 is too tight), ~80 | Michroma 46px, ~112 | Michroma 58px, ~127 |
| Date font, cap centre y | Michroma 18px, ~113 | Michroma 24px, ~154 | Michroma 26px, ~166 |
| Bins area | x5–138, y136–161, gap 4 | x7–192, y186–219, gap 6 | x52–207, y196–225, gap 6 |
| Bin label | SS 8px | SS 16px | SS 16px |
| Bin fill bar (track Midnight Green) | 22×3 | 28×4 | 24×4 |

The time is horizontally centred. Each bin is a 1px accent outline on black, with the label above a small progress bar. Bins are placeholders for complications (TBD).

### 4.4 Layout B — Large display

| | basalt | emery | gabbro |
|---|---|---|---|
| Band (y range) | 26–141 | 36–187 | 50–205 |
| Digit font | Michroma 60px | Michroma 84px | Michroma 80px |
| Hours cap centre y | 56.5 | 74.5 | ~92 |
| Minutes cap centre y | 111.5 | 149.5 | ~164 |
| Date font, cap centre y | Michroma 17px, ~156 | Michroma 22px, ~209 | Michroma 20px, ~223 |

The two digits of each row are centred as a pair, using tabular slots. On emery the slot centres are x ≈ 60 and 140. The date sits on a black box with 4–6px horizontal padding, so the number field doesn't show through behind it.

Round 2 check: every element was kept inside the circle's chord at its y position.

## 5. The 5-minute "refinement" animation

This runs when the minute ticks over to a multiple of 5. The timeline below spans **2.6 s of design time**. The watch plays it **2× slower** (`ANIM_SLOWDOWN`, so **~5.2 s** in real time) at about 30 fps: the same motion over a longer time, because at full speed it was over before you could follow it. Both prototypes in `docs/reference/` are the source of truth for the motion. The values below are for emery (Time 2). The other platforms scale them by time size (A: `time_px / 46`, B: `digit_px / 84`) and, for B's fall, by screen height (`h / 228`).

**Engineering rules (learned in prototyping):**
- **Never change the font's point size frame by frame.** Render each digit as a vector glyph scaled by a transform, with **sub-pixel (fixed-point) positions**. Integer snapping produces visible shimmer. pebble-fctx does this natively.
- No opacity or alpha. Every effect is position, scale, line endpoints, clipping or a palette colour step.
- Easing: `easeInOutCubic(v) = v<.5 ? 4v³ : 1 − (−2v+2)³/2`.

### 5.1 Layout A timeline (t in ms)
1. **Agitate, 0–700.** Each old char (`H`,`H`,`:`,`M`,`M`, index j, phase φ = 1.3·j) has `bump = sin(π·t/700)` and `wob = 0.75 + 0.25·sin(t/70 + φ)`. Size = FS·(1 + 0.16·bump·wob). y offset = 3.5·bump·sin(t/55 + φ).
   **Band fold, 250–700:** the band collapses symmetrically to its centre line (`open = 1 − ease((t−250)/450)`).
   **Field brighten, 600–2100:** Midnight Green → Tiffany Blue (a single step).
2. **Bin, from 600.** A random bin i opens its lid: two 1px flaps hinged at the outer corners, rotating to ±110° with `a = ease((t−600)/250) − ease((t−1750)/250)`.
   Old char j starts at 700 + 70·j and takes 800 ms (eased). It follows a quadratic Bézier from its slot, via a control point (midpoint x, CY − 36), to (binCentreX + 3·(j−2), y = 208), shrinking to 9px. The bins are drawn above the digits, so a digit disappears as it enters the bin's mouth.
   The bin outline flashes Celeste during 1500–2000. Its fill bar gains +6% at 1600.
3. **Refine, from 700.** Pick 4 random field cells (not adjacent to each other; on the watch, any cell that is fully visible once the band folds: not under the header, bins or date box, not in the edge columns, and inside the circle on round) and set them to the new digits. At 700 they are hidden in the field and replaced by floating Michroma glyphs in Electric Blue.
   New digit k pulses in place (8 → 13 → 8px) until 900 + 90·k. It then eases over 1100 ms to its slot while growing from 8px to FS, and turns Celeste when it arrives.
4. **Settle.** The colon reappears at 2000. The band unfolds over 1900–2400. The animation ends at 2600, and the field reverts to its rest digits.

### 5.2 Layout B timeline (differences from A)
- Agitate swell is 0.12 instead of 0.16, and the bob is 4px.
- **Drop:** old digits leave in a random order, each starting at 700 + 110·rank. With u = (t − start)/900 (linear time, which gives natural gravity): x = slotX + drift·u, where drift is random in ±16; y = slotY − 26·u + 300·u². A digit is hidden once it is fully below the screen. The date box is drawn above the falling digits.
- New digits are as in A, flying to the four stacked slots. No colon.
- **Watch change:** each row swells from the edge next to the gap between the rows (hours grow upward, minutes downward) and bobs only away from that gap. With the prototype's symmetric swell and bob, the stacked zeros touched and "10 / 10" briefly read as "18 / 18".
- Open question: old and new digits cross mid-screen. Consider delaying the emergence if it reads as too busy.

### 5.3 Open animation questions
- Should every digit be replaced, or only the ones that changed? (The prototype replaces all of them.)
- Timing still needs designing for basalt (A and B) and gabbro (A and B). Scale positions from emery.
- Power: 12 runs per hour × 2.6 s. Measure on device.

## 6. Battery
- `battery_state_service`. Show the percentage and a proportional fill.
- Fill turns **Sunset Orange at ≤ 20%**.
- Not designed yet: the charging state, and the "plugged in" state.

## 7. Not yet designed
- Bin complications (steps, weather, etc.).
- Settings: 12/24h (currently follows the watch's setting).
- Low-battery colour per theme, and the dim steps of the blue and amber themes, need checking on a device.
- Date format. The mock uses an uppercase weekday and day number (`TUE 17`).

## 8. Settings
A phone settings page (Clay, `@rebble/clay`; `src/pkjs/config.js`) sends the choices to the watch, which saves them with `persist_*`.
- **Style:** Standard (layout A) or Large (layout B).
- **Colour:** one of 8 base colours: cyan (default), green, amber, red, magenta, blue, yellow, white. The swatch is the theme's bright (time) colour. The other roles are derived from it with the rule the cyan palette follows (bright = FF/AA, highlight = FF/55, accent = AA/55, field during the animation = AA/00, field at rest = 55/00 for channels on/off). Amber is hand-picked. Red, amber and magenta use yellow for the low-battery fill.
