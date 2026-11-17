#include "layout.h"

// Sources, in priority order:
//  1. basalt: measured from docs/reference/pebble_time_pixels_v3.png, the agreed
//     raster. Where it differs from the spec text, the PNG wins: header gap 3
//     (bar at x37), A time/date cap centres 78/112, A bin label cap top +4,
//     B date cap centre 155.
//     Exception: A bins follow the spec's x5-138 span so they line up with the
//     header box (the PNG's bins stop at x136).
//  2. emery, gabbro: the spec's cap centres (CSS line-box centre + 0.07 em), and
//     the prototypes' CSS (docs/reference/layouts-*.dc.html) for box internals.
// Number field: thin 5x7 digits in 14px cells on every platform (a deliberate
// change from the spec's Silkscreen 8px field). Columns are centred on the
// screen; each layout's field_y0 minimises digits cut by the header, band, bins
// and date box.
//
// Layout A band (with its time and date) is raised from the spec/PNG position so
// the field gaps above and below it are equal: basalt -12, emery -17, gabbro -13.
// The time/date positions inside the band are unchanged.

#if defined(PBL_PLATFORM_BASALT)
#define HEADER {.box = {{5, 5}, {134, 15}}, .pct_w = 24, .pad = 4, .gap = 3, \
                .bar_h = 7, .fill_h = 3, .nub_w = 2, .nub_h = 3}
const MdrLayout LAYOUT = {
  .big_text = false, .ss_cap_off = 3, .ss_lsb = 1, .ss_cap_h = 5,
  .field = {.cols = 10, .rows = 12, .x0 = 2, .cell_w = 14, .cell_h = 14, .seed = 7},
  .a = {.header = HEADER, .field_y0 = 3, .band_y0 = 40, .band_y1 = 115,
        .time_px = 32, .time_cy = PX16(66), .date_px = 18, .date_cy = PX16(100),
        .date_box_pad = 4, .date_box_v = 5,
        .bins = {{5, 136}, {134, 26}}, .bin_gap = 4,
        .bin_label_cap_top = 4, .bin_bar_top = 17, .bin_bar_w = 22, .bin_bar_h = 3},
  .b = {.header = HEADER, .field_y0 = 3, .band_y0 = 26, .band_y1 = 141,
        .digit_px = 60, .hours_cy = PX16(56.5), .mins_cy = PX16(111.5),
        .date_px = 17, .date_cy = PX16(155),
        .date_box_y = 145, .date_box_h = 20, .date_box_pad = 4},
};

#elif defined(PBL_PLATFORM_EMERY)
#define HEADER {.box = {{7, 7}, {186, 20}}, .pct_w = 48, .pad = 6, .gap = 6, \
                .bar_h = 10, .fill_h = 6, .nub_w = 2, .nub_h = 4}
const MdrLayout LAYOUT = {
  .big_text = true, .ss_cap_off = 6, .ss_lsb = 2, .ss_cap_h = 10,
  .field = {.cols = 14, .rows = 16, .x0 = 2, .cell_w = 14, .cell_h = 14, .seed = 11},
  .a = {.header = HEADER, .field_y0 = 2, .band_y0 = 57, .band_y1 = 156,
        .time_px = 46, .time_cy = PX16(95), .date_px = 24, .date_cy = PX16(137),
        .date_box_pad = 6, .date_box_v = 7,
        .bins = {{7, 186}, {186, 34}}, .bin_gap = 6,
        .bin_label_cap_top = 9, .bin_bar_top = 25, .bin_bar_w = 28, .bin_bar_h = 4},
  .b = {.header = HEADER, .field_y0 = 11, .band_y0 = 36, .band_y1 = 187,
        .digit_px = 84, .hours_cy = PX16(74.5), .mins_cy = PX16(149.5),
        .date_px = 22, .date_cy = PX16(209.5),
        .date_box_y = 194, .date_box_h = 28, .date_box_pad = 6},
};

#elif defined(PBL_PLATFORM_GABBRO)
const MdrLayout LAYOUT = {
  .big_text = true, .ss_cap_off = 6, .ss_lsb = 2, .ss_cap_h = 10,
  .field = {.cols = 18, .rows = 18, .x0 = 4, .cell_w = 14, .cell_h = 14, .seed = 13},
  .a = {.header = {.box = {{65, 30}, {130, 20}}, .pct_w = 48, .pad = 6, .gap = 6,
                   .bar_h = 10, .fill_h = 6, .nub_w = 2, .nub_h = 4},
        .field_y0 = 4, .band_y0 = 75, .band_y1 = 170,
        .time_px = 58, .time_cy = PX16(114), .date_px = 26, .date_cy = PX16(153),
        .date_box_pad = 6, .date_box_v = 6,
        .bins = {{52, 196}, {156, 30}}, .bin_gap = 6,
        .bin_label_cap_top = 7, .bin_bar_top = 23, .bin_bar_w = 24, .bin_bar_h = 4},
  .b = {.header = {.box = {{75, 24}, {110, 18}}, .pct_w = 48, .pad = 5, .gap = 5,
                   .bar_h = 9, .fill_h = 5, .nub_w = 2, .nub_h = 4},
        .field_y0 = 11, .band_y0 = 50, .band_y1 = 205,
        .digit_px = 80, .hours_cy = PX16(92), .mins_cy = PX16(164),
        .date_px = 20, .date_cy = PX16(223),
        .date_box_y = 210, .date_box_h = 24, .date_box_pad = 6},
};

#else
#error "Unsupported platform: add a LAYOUT entry"
#endif
