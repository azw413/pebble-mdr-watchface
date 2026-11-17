// Per-platform layout table (docs/DESIGN_SPEC.md section 4). All geometry lives
// here; drawing code must not hard-code positions.
#pragma once
#include <pebble.h>

// Sub-pixel values (Michroma cap centres) are stored in fctx fixed point (1/16 px).
#define PX16(v) ((int16_t)((v) * 16))

typedef enum { LAYOUT_A = 0, LAYOUT_B = 1 } LayoutId;

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
  int8_t x0;              // grid x origin (centres the grid on the screen)
  uint8_t cell_w, cell_h; // each 5x7 digit is centred in its cell
  uint32_t seed;          // Park-Miller seed, as in each prototype's renderVals()
} FieldLayout;

typedef struct {
  HeaderLayout header;
  int8_t field_y0;            // field grid y origin: chosen so the gaps show whole digits
  int16_t band_y0, band_y1;   // band accent lines (inclusive), black between
  uint8_t time_px;            // Michroma em size
  int16_t time_cy;            // time cap centre y, PX16
  uint8_t date_px;
  int16_t date_cy;            // PX16
  GRect bins;                 // area covering all four bins
  uint8_t bin_gap;
  int8_t bin_label_cap_top;   // label cap top, relative to bin top
  int8_t bin_bar_top;         // fill bar top, relative to bin top
  uint8_t bin_bar_w, bin_bar_h;
} LayoutA;

typedef struct {
  HeaderLayout header;
  int8_t field_y0;
  int16_t band_y0, band_y1;
  uint8_t digit_px;           // hours and minutes em size
  int16_t hours_cy, mins_cy;  // PX16
  uint8_t date_px;
  int16_t date_cy;            // PX16
  int16_t date_box_y;         // black backing box behind the date
  uint8_t date_box_h, date_box_pad;
} LayoutB;

typedef struct {
  bool big_text;      // Silkscreen 16px (emery, gabbro) rather than 8px
  int8_t ss_cap_off;  // graphics_draw_text: cap top = rect.origin.y + this
  int8_t ss_lsb;      // Silkscreen left side bearing: ink starts at rect.origin.x + this
  uint8_t ss_cap_h;   // Silkscreen cap height in px
  FieldLayout field;
  LayoutA a;
  LayoutB b;
} MdrLayout;

extern const MdrLayout LAYOUT;
