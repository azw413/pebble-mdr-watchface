// Colour themes. The user picks a base colour; each theme supplies the intensity
// ramp that the spec's palette roles use (spec section 2), all from the Pebble
// 64-colour palette.
#pragma once
#include <pebble.h>

typedef struct {
  GColor bright;     // time digits, small labels           (cyan: Celeste)
  GColor highlight;  // emerging-digit highlight, bin flash  (cyan: Electric Blue)
  GColor accent;     // date, band lines, outlines, battery  (cyan: Cadet Blue)
  GColor field_lit;  // number field during the animation   (cyan: Tiffany Blue)
  GColor field;      // number field at rest, bin bar track  (cyan: Midnight Green)
  GColor low;        // battery fill at <= 20%
} Theme;

#define THEME_COUNT 8
#define THEME_DEFAULT 0  // cyan, the spec palette

Theme theme_get(uint8_t id);

// Map a colour from the settings page (0xRRGGBB, a theme's bright colour) to a
// theme id. Unknown colours fall back to THEME_DEFAULT.
uint8_t theme_from_rgb(uint32_t rgb);
