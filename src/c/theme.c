#include "theme.h"

// Ramps follow the rule the spec's cyan palette already obeys. For a hue whose
// channels are fully on (h=1) or off (h=0), each role mixes two channel levels:
//   bright    = h ? FF : AA      highlight = h ? FF : 55
//   accent    = h ? AA : 55      field_lit = h ? AA : 00
//   field     = h ? 55 : 00
// Amber is not a 0/1 hue, so its ramp is picked by hand.
// Keep the bright column in sync with the swatches in src/pkjs/config.js.
static const uint32_t RAMPS[THEME_COUNT][6] = {
  // bright    highlight  accent     field_lit  field      low
  {0xAAFFFF, 0x55FFFF, 0x55AAAA, 0x00AAAA, 0x005555, 0xFF5555},  // 0 cyan (spec)
  {0xAAFFAA, 0x55FF55, 0x55AA55, 0x00AA00, 0x005500, 0xFF5555},  // 1 green
  {0xFFAA55, 0xFFAA00, 0xAA5500, 0xAA5500, 0x550000, 0xFFFF55},  // 2 amber
  {0xFFAAAA, 0xFF5555, 0xAA5555, 0xAA0000, 0x550000, 0xFFFF55},  // 3 red
  {0xFFAAFF, 0xFF55FF, 0xAA55AA, 0xAA00AA, 0x550055, 0xFFFF55},  // 4 magenta
  {0xAAAAFF, 0x5555FF, 0x5555AA, 0x0000AA, 0x000055, 0xFF5555},  // 5 blue
  {0xFFFFAA, 0xFFFF55, 0xAAAA55, 0xAAAA00, 0x555500, 0xFF5555},  // 6 yellow
  {0xFFFFFF, 0xFFFFFF, 0xAAAAAA, 0xAAAAAA, 0x555555, 0xFF5555},  // 7 white
};

Theme theme_get(uint8_t id) {
  if (id >= THEME_COUNT) id = THEME_DEFAULT;
  const uint32_t *r = RAMPS[id];
  return (Theme){
    .bright = GColorFromHEX(r[0]), .highlight = GColorFromHEX(r[1]),
    .accent = GColorFromHEX(r[2]), .field_lit = GColorFromHEX(r[3]),
    .field = GColorFromHEX(r[4]), .low = GColorFromHEX(r[5]),
  };
}

uint8_t theme_from_rgb(uint32_t rgb) {
  for (uint8_t i = 0; i < THEME_COUNT; i++) {
    if (RAMPS[i][0] == (rgb & 0xFFFFFF)) return i;
  }
  return THEME_DEFAULT;
}
