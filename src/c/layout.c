#include "layout.h"

// basalt values are measured from docs/reference/pebble_time_pixels_v3.png,
// the agreed raster. Where it differs from the spec text, the PNG wins:
//   - header gap 3 (bar starts at x37), time cap centre 78, date cap centre 112.
// Bins follow the spec's x5-138 span so they line up with the header box
// (the PNG's bins stop at x136).
//
// emery and gabbro are provisional, taken from the spec tables. Step 3 will
// verify them against the prototypes.

#if defined(PBL_PLATFORM_BASALT)
const MdrLayout LAYOUT = {
  .big_text = false, .ss_cap_off = 3, .ss_lsb = 1, .ss_cap_h = 5,
  .header_a = { .box = {{5, 5}, {134, 15}}, .pct_w = 24, .pad = 4, .gap = 3,
                .bar_h = 7, .fill_h = 3, .nub_w = 2, .nub_h = 3 },
  .field = { .cols = 12, .rows = 14, .cell_w2 = 24, .cell_h = 12, .cap_top = 2, .dx = 1 },
  .a = { .band_y0 = 52, .band_y1 = 127,
         .time_px = 32, .time_cy = 78, .date_px = 18, .date_cy = 112,
         .bins = {{5, 136}, {134, 26}}, .bin_gap = 4,
         .bin_label_cap_top = 4, .bin_bar_top = 17, .bin_bar_w = 22, .bin_bar_h = 3 },
};

#elif defined(PBL_PLATFORM_EMERY)
const MdrLayout LAYOUT = {
  .big_text = true, .ss_cap_off = 6, .ss_lsb = 2, .ss_cap_h = 10,
  .header_a = { .box = {{7, 7}, {186, 20}}, .pct_w = 48, .pad = 6, .gap = 6,
                .bar_h = 10, .fill_h = 6, .nub_w = 2, .nub_h = 4 },
  .field = { .cols = 16, .rows = 19, .cell_w2 = 25, .cell_h = 12, .cap_top = 2, .dx = 1 },
  .a = { .band_y0 = 74, .band_y1 = 173,
         .time_px = 46, .time_cy = 112, .date_px = 24, .date_cy = 154,
         .bins = {{7, 186}, {186, 34}}, .bin_gap = 6,
         .bin_label_cap_top = 5, .bin_bar_top = 23, .bin_bar_w = 28, .bin_bar_h = 4 },
};

#elif defined(PBL_PLATFORM_GABBRO)
const MdrLayout LAYOUT = {
  .big_text = true, .ss_cap_off = 6, .ss_lsb = 2, .ss_cap_h = 10,
  .header_a = { .box = {{65, 30}, {130, 20}}, .pct_w = 48, .pad = 6, .gap = 6,
                .bar_h = 10, .fill_h = 6, .nub_w = 2, .nub_h = 4 },
  .field = { .cols = 20, .rows = 20, .cell_w2 = 26, .cell_h = 13, .cap_top = 4, .dx = 1,
             .round_clip = true },
  .a = { .band_y0 = 88, .band_y1 = 183,
         .time_px = 58, .time_cy = 127, .date_px = 26, .date_cy = 166,
         .bins = {{52, 196}, {156, 30}}, .bin_gap = 6,
         .bin_label_cap_top = 4, .bin_bar_top = 21, .bin_bar_w = 24, .bin_bar_h = 4 },
};

#else
#error "Unsupported platform: add a LAYOUT entry"
#endif
