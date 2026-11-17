# Store submission: Refinement 1.0.0

Everything the Pebble appstore listing needs, field by field.

## Listing

| Field | Value |
|---|---|
| Title | Refinement |
| Type | Watchface |
| Developer | Andrew Whaley |
| Version | 1.0.0 |
| Description | A very usable and legible watchface inspired by the Macrodata Refinement activity from Apple TV's Severance show. |
| Source code | https://github.com/azw413/pebble-mdr-watchface |
| Platforms | Pebble Time / Time Steel (basalt), Pebble Time 2 (emery), Pebble Round 2 (gabbro) |
| Configurable | Yes |
| Release file | `build/mdr-watchface.pbw` (run `pebble build` first; the name comes from the repo folder) |

### Release notes (1.0.0)

First release.
- Two styles: Standard (time and date in a band, with bins) and Large (stacked hours and minutes).
- Animated "refinement" of the time every 5 minutes, or at an interval you choose (1 minute to 1 hour, or never).
- Eight colour themes.
- Battery level in the header.

## Images

| Store slot | File |
|---|---|
| Banner (720×320) | `banner-720x320.png` |
| Large icon (144×144) | `icon-144.png` |
| Small icon (48×48) | `icon-48.png` |
| Screenshots, Pebble Time (144×168) | `basalt/standard-refinement.gif`, `basalt/standard.png`, `basalt/large.png` |
| Screenshots, Pebble Time 2 (200×228) | `emery/standard-refinement.gif`, `emery/standard.png`, `emery/large.png` |
| Screenshots, Pebble Round 2 (260×260) | `gabbro/standard-refinement.gif`, `gabbro/standard.png`, `gabbro/large.png` |

Screenshots are at each watch's native resolution, in the app's true colours
(snapped to the Pebble 64-colour palette). Put the GIF first so the listing
opens on the animation.

## Regenerating

After visual changes (all from the project root):

```
python3 tools/store_assets.py screenshots   # standard.png, large.png per platform
python3 tools/store_assets.py gif           # standard-refinement.gif (~4 min per platform)
python3 tools/store_art.py                  # banner and icons (uses the emery GIF)
pebble build                                # the capture build is left behind otherwise
```

## Before submitting

- Install the release build on a watch and save the settings page once from the
  phone. Check that style, colour and interval all take effect.
- Keep the UUID in `package.json` unchanged from now on: the store and installed
  watches identify the app by it.
