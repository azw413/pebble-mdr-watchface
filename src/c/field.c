#include "field.h"
#include "layout.h"

#define FIELD_MAX_CELLS (20 * 20)
#define GLYPH_W 5
#define GLYPH_H 7

// Thin 5x7 digits, 1px strokes. One byte per row; bit 4 is the leftmost pixel.
static const uint8_t GLYPHS[10][GLYPH_H] = {
  {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  // 0
  {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  // 1
  {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},  // 2
  {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},  // 3
  {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  // 4
  {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},  // 5
  {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},  // 6
  {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  // 7
  {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  // 8
  {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},  // 9
};

static GBitmap *s_bitmap;
static GColor *s_palette;       // [0] background (black), [1] digit colour
static uint8_t s_digits[FIELD_MAX_CELLS];
static int16_t s_y0;            // grid y origin for the current layout

// Plot (or clear) one cell's glyph in the field bitmap.
static void blit_cell(int c, int r, int digit, bool on) {
  uint8_t *data = gbitmap_get_data(s_bitmap);
  const uint16_t bpr = gbitmap_get_bytes_per_row(s_bitmap);
  const GSize size = gbitmap_get_bounds(s_bitmap).size;
  const FieldLayout *f = &LAYOUT.field;
  const int x0 = f->x0 + c * f->cell_w + (f->cell_w - GLYPH_W) / 2;
  const int y0 = s_y0 + r * f->cell_h + (f->cell_h - GLYPH_H) / 2;
  for (int gy = 0; gy < GLYPH_H; gy++) {
    const int y = y0 + gy;
    if (y < 0 || y >= size.h) continue;
    for (int gx = 0; gx < GLYPH_W; gx++) {
      const int x = x0 + gx;
      if (x < 0 || x >= size.w) continue;
      uint8_t *byte = &data[y * bpr + x / 8];
      const uint8_t mask = (uint8_t)(0x80 >> (x % 8));  // 1-bit palettised: MSB first
      if (on && (GLYPHS[digit][gy] & (0x10 >> gx))) *byte |= mask;
      else if (!on) *byte &= (uint8_t)~mask;
    }
  }
}

void field_init(GSize screen, int16_t y0) {
  s_y0 = y0;
  // Same Park-Miller sequence as the prototypes' renderVals() (row-major). On
  // gabbro the round display clips the grid to the circle per pixel, as the
  // prototype's border-radius does.
  const FieldLayout *f = &LAYOUT.field;
  uint32_t seed = f->seed;
  const int n = f->cols * f->rows;
  for (int i = 0; i < n && i < FIELD_MAX_CELLS; i++) {
    seed = (uint32_t)(((uint64_t)seed * 16807u) % 2147483647u);
    s_digits[i] = seed % 10;
  }
  s_palette = malloc(2 * sizeof(GColor));
  s_palette[0] = GColorBlack;
  s_palette[1] = GColorMidnightGreen;
  s_bitmap = gbitmap_create_blank_with_palette(screen, GBitmapFormat1BitPalette, s_palette, true);
  if (!s_bitmap) return;
  for (int r = 0; r < f->rows; r++) {
    for (int c = 0; c < f->cols; c++) blit_cell(c, r, s_digits[r * f->cols + c], true);
  }
}

void field_deinit(void) {
  if (s_bitmap) gbitmap_destroy(s_bitmap);  // also frees s_palette
  else free(s_palette);
  s_bitmap = NULL;
  s_palette = NULL;
}

void field_set_color(GColor c) {
  if (s_palette) s_palette[1] = c;
}

void field_draw(GContext *ctx, GRect bounds) {
  if (!s_bitmap) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_bitmap, bounds);
}
