// Per-platform layout table (docs/DESIGN_SPEC.md section 4). All geometry lives
// here; drawing code must not hard-code positions.
#pragma once
#include <pebble.h>

typedef struct {
  GRect box;          // outline box (1px accent outline, black fill)
  uint8_t pct_w;      // reserved width for "100%"
  uint8_t pad;        // inner padding, left and right
  uint8_t gap;        // gap between percentage and bar
  uint8_t bar_h;      // bar outer height (1px outline + 1px inset)
  uint8_t fill_h;     // bar fill height
  uint8_t nub_w, nub_h;
} HeaderLayout;

typedef struct {
  uint8_t cols, rows;
  uint8_t cell_w2;    // cell width x2 (emery is 12.5px: alternating 12/13 columns)
  uint8_t cell_h;
  int8_t cap_top;     // digit cap top, relative to the cell top
  int8_t dx;          // x nudge: Pebble centres by advance, the reference by ink
  bool round_clip;    // drop cells outside the circle (gabbro)
} FieldLayout;

typedef struct {
  int16_t band_y0, band_y1;   // band accent lines (inclusive), black between
  uint8_t time_px;            // Michroma em size
  int16_t time_cy;            // time cap centre y
  uint8_t date_px;
  int16_t date_cy;
  GRect bins;                 // area covering all four bins
  uint8_t bin_gap;
  int8_t bin_label_cap_top;   // label cap top, relative to bin top
  int8_t bin_bar_top;         // fill bar top, relative to bin top
  uint8_t bin_bar_w, bin_bar_h;
} LayoutA;

typedef struct {
  bool big_text;      // Silkscreen 16px (emery, gabbro) rather than 8px
  int8_t ss_cap_off;  // graphics_draw_text: cap top = rect.origin.y + this
  int8_t ss_lsb;      // Silkscreen left side bearing: ink starts at rect.origin.x + this
  uint8_t ss_cap_h;   // Silkscreen cap height in px
  HeaderLayout header_a;
  FieldLayout field;
  LayoutA a;
} MdrLayout;

extern const MdrLayout LAYOUT;
