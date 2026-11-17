#include "field.h"
#include "layout.h"

#define FIELD_MAX_CELLS (20 * 20)

static GBitmap *s_bitmap;
static GColor *s_palette;       // [0] background (black), [1] digit colour
static GFont s_font;
static bool s_built;
static uint8_t s_digits[FIELD_MAX_CELLS];

static int16_t cell_x(int c) { return (int16_t)(c * LAYOUT.field.cell_w2 / 2); }
static int16_t cell_w(int c) { return (int16_t)(cell_x(c + 1) - cell_x(c)); }

static bool cell_visible(int c, int r, GSize size) {
  if (!LAYOUT.field.round_clip) return true;
  // Keep a cell only if its whole box is inside the display circle.
  const int32_t cx = size.w / 2, cy = size.h / 2, rad = size.w / 2;
  const int16_t x0 = cell_x(c), x1 = cell_x(c) + cell_w(c);
  const int16_t y0 = r * LAYOUT.field.cell_h, y1 = y0 + LAYOUT.field.cell_h;
  const int16_t xs[2] = {x0, x1}, ys[2] = {y0, y1};
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      int32_t dx = xs[i] - cx, dy = ys[j] - cy;
      if (dx * dx + dy * dy > rad * rad) return false;
    }
  }
  return true;
}

void field_init(GFont digit_font) {
  s_font = digit_font;
  // Same Park-Miller sequence as the prototypes' renderVals() (seed 7, row-major).
  uint32_t seed = 7;
  const int n = LAYOUT.field.cols * LAYOUT.field.rows;
  for (int i = 0; i < n && i < FIELD_MAX_CELLS; i++) {
    seed = (uint32_t)(((uint64_t)seed * 16807u) % 2147483647u);
    s_digits[i] = seed % 10;
  }
  s_palette = malloc(2 * sizeof(GColor));
  s_palette[0] = GColorBlack;
  s_palette[1] = GColorMidnightGreen;
}

void field_deinit(void) {
  if (s_bitmap) gbitmap_destroy(s_bitmap);  // also frees s_palette
  else free(s_palette);
  s_bitmap = NULL;
  s_palette = NULL;
}

void field_set_color(GColor c) { s_palette[1] = c; }

// Draw every digit as white text, then copy lit framebuffer pixels into the bitmap.
static void build(GContext *ctx, GRect bounds) {
  s_bitmap = gbitmap_create_blank_with_palette(bounds.size, GBitmapFormat1BitPalette,
                                               s_palette, true);
  if (!s_bitmap) return;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  char buf[2] = {0, 0};
  for (int r = 0; r < LAYOUT.field.rows; r++) {
    for (int c = 0; c < LAYOUT.field.cols; c++) {
      if (!cell_visible(c, r, bounds.size)) continue;
      buf[0] = '0' + s_digits[r * LAYOUT.field.cols + c];
      GRect rc = GRect(cell_x(c) + LAYOUT.field.dx, r * LAYOUT.field.cell_h + LAYOUT.field.cap_top - LAYOUT.ss_cap_off,
                       cell_w(c), LAYOUT.field.cell_h + LAYOUT.ss_cap_off);
      graphics_draw_text(ctx, buf, s_font, rc, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
  }

  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  uint8_t *dst = gbitmap_get_data(s_bitmap);
  const uint16_t dst_bpr = gbitmap_get_bytes_per_row(s_bitmap);
  for (int y = 0; y < bounds.size.h; y++) {
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
    for (int x = row.min_x; x <= row.max_x; x++) {
      if (row.data[x] != GColorBlack.argb) {
        dst[y * dst_bpr + x / 8] |= (uint8_t)(0x80 >> (x % 8));  // MSB-first
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
  s_built = true;
}

void field_draw(GContext *ctx, GRect bounds) {
  if (!s_built) build(ctx, bounds);
  if (!s_bitmap) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_bitmap, bounds);
}
