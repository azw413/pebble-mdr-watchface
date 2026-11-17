// MDR watchface: layouts A "Standard" and B "Large" (docs/DESIGN_SPEC.md 4.3, 4.4),
// with the 5-minute refinement animation (section 5).
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include "anim.h"
#include "field.h"
#include "layout.h"
#include "michroma_metrics.h"
#include "settings.h"
#include "theme.h"

// Palette roles (spec section 2), taken from the user's colour theme.
static Theme s_theme;
#define COL_TIME   (s_theme.bright)
#define COL_ACCENT (s_theme.accent)
#define COL_DATE   (s_theme.accent)
#define COL_TRACK  (s_theme.field)
#define COL_LOW    (s_theme.low)

// Layers, back to front: field, band, time (date and digits), hud (header, bins).
// The header and bins sit above the digits, so an old digit disappears into
// the bin's mouth.
static Window *s_window;
static Layer *s_field_layer, *s_band_layer, *s_time_layer, *s_hud_layer;
static FFont *s_michroma;
static GFont s_ss_label;  // Silkscreen: battery percentage and bin labels

static Settings s_settings;
#define s_layout (s_settings.layout)

static char s_time[6] = "00:00";
static char s_date[12] = "";
static uint8_t s_batt = 100;

// Placeholder bin fill levels until complications exist (spec section 7).
static uint8_t s_bin_fill[4] = {62, 35, 88, 14};
static const char *BIN_LABEL[4] = {"01", "02", "03", "04"};

static Anim s_anim;
static AnimFrame s_frame;
static AppTimer *s_anim_timer;
static uint32_t s_anim_start_ms;
static uint16_t s_anim_frames;  // frames drawn in this run (demo builds log the rate)

// ---------------------------------------------------------------------------
// Small helpers

static void outline_rect(GContext *ctx, GRect r) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, COL_ACCENT);
  graphics_draw_rect(ctx, r);
}

static void fill(GContext *ctx, GColor c, GRect r) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
}

// Silkscreen text positioned by cap top (spec section 3).
static void draw_ss(GContext *ctx, const char *s, GFont f, int16_t x, int16_t w,
                    int16_t cap_top, GTextAlignment align, GColor c) {
  graphics_context_set_text_color(ctx, c);
  GRect r = GRect(x, cap_top - LAYOUT.ss_cap_off, w, LAYOUT.ss_cap_h * 3);
  graphics_draw_text(ctx, s, f, r, GTextOverflowModeFill, align, NULL);
}

static bool rects_overlap(GRect a, GRect b) {
  return a.origin.x < b.origin.x + b.size.w && b.origin.x < a.origin.x + a.size.w &&
         a.origin.y < b.origin.y + b.size.h && b.origin.y < a.origin.y + a.size.h;
}

static uint32_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000 + ms;
}

// ---------------------------------------------------------------------------
// Geometry shared by drawing and the animation

static GRect bin_rect(int i) {
  const LayoutA *a = &LAYOUT.a;
  // Spread the bins across the area; widths differ by at most 1px.
  const int16_t span = a->bins.size.w + a->bin_gap;
  const int16_t bx0 = a->bins.origin.x + (span * i + 2) / 4;
  const int16_t bx1 = a->bins.origin.x + (span * (i + 1) + 2) / 4 - a->bin_gap;
  return GRect(bx0, a->bins.origin.y, bx1 - bx0, a->bins.size.h);
}

static int tabular_index(char ch) { return ch == ':' ? 10 : ch - '0'; }
static int32_t slot_units(char ch) { return ch == ':' ? MICHROMA_COLON_SLOT : MICHROMA_DIGIT_SLOT; }

// Font units -> fctx fixed point at an em size given in 1/16 px.
static fixed_t fu16(int32_t units, int32_t em16) {
  return (fixed_t)(((int64_t)units * em16 + MICHROMA_UPEM / 2) / MICHROMA_UPEM);
}

// Tabular slots (spec 3): every digit gets the same slot and the row is centred
// on cx. Writes each character's slot centre x.
static void slot_centres(const char *s, int n, int px, fixed_t cx, fixed_t *out) {
  int32_t total = 0;
  for (int i = 0; i < n; i++) total += slot_units(s[i]);
  int32_t pen = -total / 2;
  for (int i = 0; i < n; i++) {
    out[i] = cx + fu16(pen + slot_units(s[i]) / 2, px * 16);
    pen += slot_units(s[i]);
  }
}

// Resting positions of the time characters for the current layout:
// A "HH:MM" -> 5 slots, B -> H H / M M.
static int time_slots(GSize screen, FPoint *slot) {
  const fixed_t cx = INT_TO_FIXED(screen.w) / 2;
  fixed_t xs[5];
  if (s_layout == LAYOUT_A) {
    slot_centres("00:00", 5, LAYOUT.a.time_px, cx, xs);
    for (int i = 0; i < 5; i++) slot[i] = FPoint(xs[i], LAYOUT.a.time_cy);
    return 5;
  }
  slot_centres("00", 2, LAYOUT.b.digit_px, cx, xs);
  slot[0] = FPoint(xs[0], LAYOUT.b.hours_cy);
  slot[1] = FPoint(xs[1], LAYOUT.b.hours_cy);
  slot[2] = FPoint(xs[0], LAYOUT.b.mins_cy);
  slot[3] = FPoint(xs[1], LAYOUT.b.mins_cy);
  return 4;
}

// ---------------------------------------------------------------------------
// Michroma glyphs (fctx: anti-aliased, sub-pixel, scaled by transform)

// Set any em size, including fractions (1/16 px). fctx_set_text_em_height only
// takes whole pixels; it is a wrapper over fctx_set_scale, so scale its ratio.
static void set_em16(FContext *fctx, int32_t em16) {
  fctx_set_text_em_height(fctx, s_michroma, 1);
  const FPoint from = fctx->transform_scale_from, to = fctx->transform_scale_to;
  fctx_set_scale(fctx, FPoint(from.x * 16, from.y * 16), FPoint(to.x * em16, to.y * em16));
}

// One digit or colon with its ink centred on pos.x and its caps centred on pos.y.
static void draw_glyph(FContext *fctx, char ch, FPoint pos, int32_t em16, GColor color) {
  char one[2] = {ch, 0};
  set_em16(fctx, em16);
  fctx_set_fill_color(fctx, color);
  fctx_set_offset(fctx, FPoint(pos.x - fu16(MICHROMA_INK_CX[tabular_index(ch)], em16), pos.y));
  fctx_begin_fill(fctx);
  fctx_draw_string(fctx, one, s_michroma, GTextAlignmentLeft, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);
}

// ---------------------------------------------------------------------------
// Field and band

static void field_update(Layer *layer, GContext *ctx) {
  field_set_color(s_anim.active && s_frame.field_lit ? s_theme.field_lit : s_theme.field);
  field_draw(ctx, layer_get_bounds(layer));
}

// The band's current line rows. It folds symmetrically to its centre line during
// the animation. Returns false while it is fully folded.
static bool band_lines(int16_t *top, int16_t *bottom) {
  const int16_t y0 = s_layout == LAYOUT_A ? LAYOUT.a.band_y0 : LAYOUT.b.band_y0;
  const int16_t y1 = s_layout == LAYOUT_A ? LAYOUT.a.band_y1 : LAYOUT.b.band_y1;
  const int32_t open = s_anim.active ? s_frame.band_open : Q16_ONE;
  if (open < Q16_ONE / 100) return false;
  // Height in whole px; split so the fold stays centred on the band's middle line.
  const int32_t h = (int32_t)(((int64_t)(y1 - y0) * open + Q16_ONE / 2) / Q16_ONE);
  *top = (y0 + y1 - h) / 2;
  *bottom = (y0 + y1 + h + 1) / 2;
  return true;
}

static void band_update(Layer *layer, GContext *ctx) {
  const int16_t w = layer_get_bounds(layer).size.w;
  int16_t top, bottom;
  if (!band_lines(&top, &bottom)) return;
  fill(ctx, GColorBlack, GRect(0, top, w, bottom - top + 1));
  fill(ctx, COL_ACCENT, GRect(0, top, w, 1));
  fill(ctx, COL_ACCENT, GRect(0, bottom, w, 1));
}

// ---------------------------------------------------------------------------
// HUD: header and bins (always on top)

static void draw_header(GContext *ctx, const HeaderLayout *h) {
  outline_rect(ctx, h->box);
  const int16_t x0 = h->box.origin.x + 1 + h->pad;                       // content left
  const int16_t x1 = h->box.origin.x + h->box.size.w - 1 - h->pad;       // content right (excl)
  const int16_t cy2 = h->box.origin.y * 2 + h->box.size.h - 1;           // 2 x inner centre y

  char pct[5];
  snprintf(pct, sizeof(pct), "%d%%", s_batt);
  draw_ss(ctx, pct, s_ss_label, x0 - LAYOUT.ss_lsb, h->pct_w + 8,
          (cy2 - LAYOUT.ss_cap_h + 1) / 2, GTextAlignmentLeft, COL_TIME);

  // Bar outline, then fill inset by 2 (1px outline + 1px padding), then nub.
  const int16_t bx = x0 + h->pct_w + h->gap;
  const int16_t bw = x1 - h->nub_w - bx;
  const int16_t by = (cy2 - h->bar_h + 1) / 2;
  graphics_context_set_stroke_color(ctx, COL_ACCENT);
  graphics_draw_rect(ctx, GRect(bx, by, bw, h->bar_h));
  const int16_t inner = bw - 4;
  fill(ctx, s_batt <= 20 ? COL_LOW : COL_ACCENT,
       GRect(bx + 2, by + 2, inner * s_batt / 100, h->fill_h));
  fill(ctx, COL_ACCENT, GRect(bx + bw, (cy2 - h->nub_h + 1) / 2, h->nub_w, h->nub_h));
}

// A bin's top edge is two flaps hinged at the outer corners. lid (Q16) opens
// them to 110 degrees.
static void draw_bin_outline(GContext *ctx, GRect b, int32_t lid, GColor c) {
  const int16_t x0 = b.origin.x, x1 = b.origin.x + b.size.w - 1;
  const int16_t y0 = b.origin.y, y1 = b.origin.y + b.size.h - 1;
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_antialiased(ctx, false);
  graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x0, y1));
  graphics_draw_line(ctx, GPoint(x1, y0), GPoint(x1, y1));
  graphics_draw_line(ctx, GPoint(x0, y1), GPoint(x1, y1));
  const int32_t len = b.size.w / 2;
  // Angle in trig units: 110 degrees = 110/360 of a turn, scaled by lid.
  const int32_t ang = (int32_t)((int64_t)TRIG_MAX_ANGLE * 110 / 360 * lid / Q16_ONE);
  const int32_t dx = len * cos_lookup(ang) / TRIG_MAX_RATIO;
  const int32_t dy = len * sin_lookup(ang) / TRIG_MAX_RATIO;
  graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x0 + dx, y0 - dy));
  graphics_draw_line(ctx, GPoint(x1, y0), GPoint(x1 - dx, y0 - dy));
}

static void draw_bins(GContext *ctx, const LayoutA *a) {
  for (int i = 0; i < 4; i++) {
    const GRect b = bin_rect(i);
    const bool chosen = s_anim.active && i == s_anim.bin;
    fill(ctx, GColorBlack, b);
    draw_bin_outline(ctx, b, chosen ? s_frame.lid : 0,
                     chosen && s_frame.bin_flash ? COL_TIME : COL_ACCENT);
    draw_ss(ctx, BIN_LABEL[i], s_ss_label, b.origin.x, b.size.w,
            b.origin.y + a->bin_label_cap_top, GTextAlignmentCenter, COL_TIME);
    const int16_t tx = b.origin.x + (b.size.w - a->bin_bar_w) / 2;
    const int16_t ty = b.origin.y + a->bin_bar_top;
    int fill_pct = s_bin_fill[i] + (chosen && s_frame.bin_bumped ? 6 : 0);
    if (fill_pct > 100) fill_pct = 100;
    fill(ctx, COL_TRACK, GRect(tx, ty, a->bin_bar_w, a->bin_bar_h));
    fill(ctx, COL_ACCENT, GRect(tx, ty, (a->bin_bar_w * fill_pct + 50) / 100, a->bin_bar_h));
  }
}

static void hud_update(Layer *layer, GContext *ctx) {
  if (s_layout == LAYOUT_A) {
    draw_header(ctx, &LAYOUT.a.header);
    draw_bins(ctx, &LAYOUT.a);
  } else {
    draw_header(ctx, &LAYOUT.b.header);
  }
}

// ---------------------------------------------------------------------------
// Time layer: date (on its black box) and the time digits

// The date's black box, so the field never shows through behind it.
static GRect date_box(GContext *ctx, GSize screen) {
  FContext fctx;
  const int px = s_layout == LAYOUT_A ? LAYOUT.a.date_px : LAYOUT.b.date_px;
  fctx_init_context(&fctx, ctx);
  fctx_set_text_em_height(&fctx, s_michroma, px);
  const int16_t w = FIXED_TO_INT(fctx_string_width(&fctx, s_date, s_michroma) + FIX1 - 1);
  fctx_deinit_context(&fctx);
  if (s_layout == LAYOUT_A) {
    const LayoutA *a = &LAYOUT.a;
    const int16_t cap = a->date_px * 3 / 4, cy = FIXED_TO_INT(a->date_cy);
    return GRect(screen.w / 2 - w / 2 - a->date_box_pad, cy - cap / 2 - a->date_box_v,
                 w + 2 * a->date_box_pad, cap + 2 * a->date_box_v);
  }
  const LayoutB *b = &LAYOUT.b;
  return GRect(screen.w / 2 - w / 2 - b->date_box_pad, b->date_box_y,
               w + 2 * b->date_box_pad, b->date_box_h);
}

static GRect s_date_box;  // last drawn, for picking animation cells

// Plain graphics and fctx never overlap: each fctx pass has its own context.
static void draw_date(GContext *ctx, GSize screen) {
  s_date_box = date_box(ctx, screen);
  fill(ctx, GColorBlack, s_date_box);
  // A band line sweeping through the box (while the band unfolds) passes behind
  // the date rather than stopping at the box edges.
  int16_t top, bottom;
  if (band_lines(&top, &bottom)) {
    const GRect bx = s_date_box;
    const int16_t lines[2] = {top, bottom};
    for (int i = 0; i < 2; i++) {
      if (lines[i] >= bx.origin.y && lines[i] < bx.origin.y + bx.size.h) {
        fill(ctx, COL_ACCENT, GRect(bx.origin.x, lines[i], bx.size.w, 1));
      }
    }
  }
  const int px = s_layout == LAYOUT_A ? LAYOUT.a.date_px : LAYOUT.b.date_px;
  const fixed_t cy = s_layout == LAYOUT_A ? LAYOUT.a.date_cy : LAYOUT.b.date_cy;
  FContext fctx;
  fctx_init_context(&fctx, ctx);
  fctx_set_fill_color(&fctx, COL_DATE);
  fctx_set_text_em_height(&fctx, s_michroma, px);
  fctx_set_offset(&fctx, FPoint(INT_TO_FIXED(screen.w) / 2, cy));
  fctx_begin_fill(&fctx);
  fctx_draw_string(&fctx, s_date, s_michroma, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);
  fctx_deinit_context(&fctx);
}

static void draw_digits(GContext *ctx, GSize screen) {
  FContext fctx;
  fctx_init_context(&fctx, ctx);
  if (s_anim.active) {
    for (int i = 0; i < s_frame.n_glyphs; i++) {
      const AnimGlyph *g = &s_frame.glyphs[i];
      draw_glyph(&fctx, g->ch, g->pos, g->em16,
                 g->color == GLYPH_HIGHLIGHT ? s_theme.highlight : COL_TIME);
    }
  } else {
    FPoint slot[5];
    const int n = time_slots(screen, slot);
    const int px = s_layout == LAYOUT_A ? LAYOUT.a.time_px : LAYOUT.b.digit_px;
    const char *chars = s_layout == LAYOUT_A ? s_time : (const char[]){s_time[0], s_time[1],
                                                                        s_time[3], s_time[4]};
    for (int i = 0; i < n; i++) draw_glyph(&fctx, chars[i], slot[i], px * 16, COL_TIME);
  }
  fctx_deinit_context(&fctx);
}

static void time_update(Layer *layer, GContext *ctx) {
  const GSize screen = layer_get_bounds(layer).size;
  // A: the digits pass over the date. B: the date box covers the falling digits.
  if (s_layout == LAYOUT_A) {
    draw_date(ctx, screen);
    draw_digits(ctx, screen);
  } else {
    draw_digits(ctx, screen);
    draw_date(ctx, screen);
  }
}

// ---------------------------------------------------------------------------
// Animation driver

static void mark_all_dirty(void) {
  if (!s_field_layer) return;
  layer_mark_dirty(s_field_layer);
  layer_mark_dirty(s_band_layer);
  layer_mark_dirty(s_time_layer);
  layer_mark_dirty(s_hud_layer);
}

// A source cell must be fully visible once the band has folded: on screen, not
// under the header, the bins (A) or the date box (B), and inside the circle on
// round displays.
static bool cell_usable(int c, int r, GSize screen, GRect date) {
  const GRect g = field_glyph_rect(c, r);
  const GRect hdr = s_layout == LAYOUT_A ? LAYOUT.a.header.box : LAYOUT.b.header.box;
  if (g.origin.x < 0 || g.origin.y < 0 || g.origin.x + g.size.w > screen.w ||
      g.origin.y + g.size.h > screen.h) return false;
  if (rects_overlap(g, hdr)) return false;
  if (s_layout == LAYOUT_A && rects_overlap(g, LAYOUT.a.bins)) return false;
  if (s_layout == LAYOUT_B && rects_overlap(g, date)) return false;
#ifdef PBL_ROUND
  const int32_t rad = screen.w / 2 - 2, cx = screen.w / 2, cy = screen.h / 2;
  for (int i = 0; i < 4; i++) {
    const int32_t x = g.origin.x + (i & 1 ? g.size.w : 0) - cx;
    const int32_t y = g.origin.y + (i & 2 ? g.size.h : 0) - cy;
    if (x * x + y * y > rad * rad) return false;
  }
#endif
  return true;
}

// Four random usable cells, no two sharing an edge (as in the prototypes).
static void pick_cells(Anim *a, GSize screen, GRect date) {
  const FieldLayout *f = &LAYOUT.field;
  for (int k = 0; k < 4; k++) {
    int c = 1, r = 1;
    for (int tries = 0; tries < 200; tries++) {
      c = 1 + rand() % (f->cols - 2);
      r = rand() % f->rows;
      if (!cell_usable(c, r, screen, date)) continue;
      bool clash = false;
      for (int i = 0; i < k; i++) {
        const int dc = c - a->cell_c[i], dr = r - a->cell_r[i];
        if ((dc == 0 && dr == 0) || (dc * dc + dr * dr == 1)) clash = true;
      }
      if (!clash) break;
    }
    a->cell_c[k] = c;
    a->cell_r[k] = r;
  }
}

static void anim_apply_cells(void) {
  // Until 700 the chosen cells show the new digits; then they are blank while
  // the digits are in flight.
  static const uint8_t DIGIT_OF[4] = {0, 1, 3, 4};
  for (int k = 0; k < 4; k++) {
    const int d = s_frame.cells_hidden ? -1 : s_anim.to[DIGIT_OF[k]] - '0';
    field_set_cell(s_anim.cell_c[k], s_anim.cell_r[k], d);
  }
}

static void anim_finish(void) {
  for (int k = 0; k < 4; k++) field_restore_cell(s_anim.cell_c[k], s_anim.cell_r[k]);
  if (s_anim.layout == LAYOUT_A) {
    s_bin_fill[s_anim.bin] = s_bin_fill[s_anim.bin] + 6 > 100 ? 100 : s_bin_fill[s_anim.bin] + 6;
  }
  memcpy(s_time, s_anim.to, sizeof(s_time));
  s_anim.active = false;
  s_anim_timer = NULL;
#ifdef MDR_DEMO
  APP_LOG(APP_LOG_LEVEL_INFO, "refine: %d frames in %d ms", s_anim_frames,
          (int)(now_ms() - s_anim_start_ms));
#endif
  mark_all_dirty();
}

__attribute__((unused))  // freeze builds hold one frame and never step
static void anim_step(void *data) {
  const int32_t real = (int32_t)(now_ms() - s_anim_start_ms);
  s_anim.t = real / ANIM_SLOWDOWN;
  if (s_anim.t >= ANIM_DURATION_MS) {
    anim_finish();
    return;
  }
  anim_frame(&s_anim, &s_frame);
  anim_apply_cells();
  mark_all_dirty();
  s_anim_frames++;
  // Aim for a steady ~30 fps: wait out only what is left of this frame's 33 ms.
  const int32_t next = (real / ANIM_FRAME_MS + 1) * ANIM_FRAME_MS;
  const int32_t wait = next - (int32_t)(now_ms() - s_anim_start_ms);
  s_anim_timer = app_timer_register(wait > 5 ? wait : 5, anim_step, NULL);
}

static void anim_start(const char *from, const char *to) {
  if (s_anim.active || !s_window) return;
  const GSize screen = layer_get_bounds(window_get_root_layer(s_window)).size;
  Anim *a = &s_anim;
  *a = (Anim){.layout = s_layout};
  memcpy(a->from, from, sizeof(a->from));
  memcpy(a->to, to, sizeof(a->to));

  AnimGeom *g = &a->geom;
  time_slots(screen, g->slot);
  const int px = s_layout == LAYOUT_A ? LAYOUT.a.time_px : LAYOUT.b.digit_px;
  g->fs16 = px * 16;
  g->k_q16 = s_layout == LAYOUT_A ? px * Q16_ONE / 46 : px * Q16_ONE / 84;
  g->kh_q16 = screen.h * Q16_ONE / 228;
  g->screen_h = screen.h;
  // Field digits are 7px tall; Michroma's caps are 0.75 em, so 28/3 px em.
  g->start_em16 = 7 * 16 * 4 / 3;
  anim_randomise(a);

  // The date box from the last frame; before the first draw, the whole strip.
  GRect date = s_date_box;
  if (date.size.w == 0) date = GRect(0, LAYOUT.b.date_box_y, screen.w, LAYOUT.b.date_box_h);
  pick_cells(a, screen, date);
  for (int k = 0; k < 4; k++) {
    const GRect r = field_glyph_rect(a->cell_c[k], a->cell_r[k]);
    g->cell[k] = FPoint(INT_TO_FIXED(r.origin.x) + INT_TO_FIXED(r.size.w) / 2,
                        INT_TO_FIXED(r.origin.y) + INT_TO_FIXED(r.size.h) / 2);
  }
  if (s_layout == LAYOUT_A) {
    // Old digits land two thirds of the way down the chosen bin.
    const GRect b = bin_rect(a->bin);
    g->bin_mouth = FPoint(INT_TO_FIXED(b.origin.x) + INT_TO_FIXED(b.size.w) / 2,
                          INT_TO_FIXED(b.origin.y) + INT_TO_FIXED(b.size.h) * 2 / 3);
  }

  a->active = true;
  a->t = 0;
  s_anim_frames = 0;
  anim_frame(a, &s_frame);
  anim_apply_cells();
  s_anim_start_ms = now_ms();
#ifdef MDR_FREEZE
  // Preview builds: hold one frame for screenshots (see wscript).
  a->t = MDR_FREEZE;
  anim_frame(a, &s_frame);
  anim_apply_cells();
#else
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_step, NULL);
#endif
  mark_all_dirty();
}

static void anim_cancel(void) {
  if (!s_anim.active) return;
  if (s_anim_timer) app_timer_cancel(s_anim_timer);
  s_anim_timer = NULL;
  for (int k = 0; k < 4; k++) field_restore_cell(s_anim.cell_c[k], s_anim.cell_r[k]);
  memcpy(s_time, s_anim.to, sizeof(s_time));
  s_anim.active = false;
}

// ---------------------------------------------------------------------------
// Services

static void format_time(struct tm *t, char *out) {
  int hour = t->tm_hour;
  if (!clock_is_24h_style()) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  snprintf(out, 6, "%02d:%02d", hour, t->tm_min);
}

static void update_date(struct tm *t) {
  char wday[8];
  strftime(wday, sizeof(wday), "%a", t);
  for (char *p = wday; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
  }
  snprintf(s_date, sizeof(s_date), "%s %d", wday, t->tm_mday);
}

static void tick_handler(struct tm *t, TimeUnits changed) {
  char next[6];
  format_time(t, next);
  update_date(t);
  // Every 5 minutes the old time is refined into the new one (spec 5).
  if (t->tm_min % 5 == 0 && strcmp(next, s_time) != 0 && !s_anim.active) {
    anim_start(s_time, next);
    return;
  }
  if (!s_anim.active) memcpy(s_time, next, sizeof(s_time));
  mark_all_dirty();
}

static void battery_handler(BatteryChargeState st) {
  s_batt = st.charge_percent;
  if (s_hud_layer) layer_mark_dirty(s_hud_layer);
}

#ifdef MDR_DEMO
// Demo builds: a tap (or `pebble emu-tap`) plays the animation into the next
// five-minute mark.
static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_anim.active) return;
  int h = (s_time[0] - '0') * 10 + (s_time[1] - '0');
  int m = (s_time[3] - '0') * 10 + (s_time[4] - '0');
  m = (m / 5 + 1) * 5;
  if (m >= 60) {
    m -= 60;
    h = (h + 1) % 24;
  }
  char next[6];
  snprintf(next, sizeof(next), "%02u:%02u", (unsigned)h % 24u, (unsigned)m % 60u);
  anim_start(s_time, next);
}
#endif

// ---------------------------------------------------------------------------
// Settings

static void build_field(void) {
  field_deinit();
  field_init(GSize(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT),
             s_layout == LAYOUT_A ? LAYOUT.a.field_y0 : LAYOUT.b.field_y0);
  field_set_color(s_theme.field);
}

static void apply_settings(bool layout_changed) {
  s_theme = theme_get(s_settings.theme);
  if (layout_changed) build_field();  // each layout has its own grid offset
  mark_all_dirty();
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  const LayoutId old_layout = s_layout;
  if (settings_update_from_message(&s_settings, iter)) {
    anim_cancel();
    settings_save(&s_settings);
    apply_settings(s_layout != old_layout);
  }
}

// ---------------------------------------------------------------------------
// Window

static Layer *add_layer(Layer *root, GRect b, LayerUpdateProc proc) {
  Layer *l = layer_create(b);
  layer_set_update_proc(l, proc);
  layer_add_child(root, l);
  return l;
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  const GRect b = layer_get_bounds(root);
  s_field_layer = add_layer(root, b, field_update);
  s_band_layer = add_layer(root, b, band_update);
  s_time_layer = add_layer(root, b, time_update);
  s_hud_layer = add_layer(root, b, hud_update);
}

static void window_unload(Window *w) {
  layer_destroy(s_hud_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_band_layer);
  layer_destroy(s_field_layer);
  s_field_layer = s_band_layer = s_time_layer = s_hud_layer = NULL;
}

static void init(void) {
  s_michroma = ffont_create_from_resource(RESOURCE_ID_FFONT_MICHROMA);
#ifdef PBL_PLATFORM_BASALT
  s_ss_label = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_8));
#else
  s_ss_label = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_16));
#endif
  s_settings = settings_load();
  apply_settings(true);
  srand(time(NULL));

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){.load = window_load, .unload = window_unload});
  window_stack_push(s_window, false);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  format_time(t, s_time);
  update_date(t);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_handler(battery_state_service_peek());
  battery_state_service_subscribe(battery_handler);

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);

#ifdef MDR_DEMO
  accel_tap_service_subscribe(tap_handler);
#endif
#ifdef MDR_FREEZE
  srand(1);  // the same run every time, so frames can be compared
  anim_start("10:10", "10:15");  // as in the prototypes
#endif
}

static void deinit(void) {
#ifdef MDR_DEMO
  accel_tap_service_unsubscribe();
#endif
  anim_cancel();
  app_message_deregister_callbacks();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
  field_deinit();
  fonts_unload_custom_font(s_ss_label);
  ffont_destroy(s_michroma);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
