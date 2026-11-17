// MDR watchface: static Layout A (docs/DESIGN_SPEC.md section 4.3).
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include "layout.h"
#include "field.h"
#include "michroma_metrics.h"

// Palette roles (spec section 2).
#define COL_TIME   GColorCeleste
#define COL_ACCENT GColorCadetBlue
#define COL_DATE   GColorCadetBlue
#define COL_TRACK  GColorMidnightGreen
#define COL_LOW    GColorSunsetOrange

static Window *s_window;
static Layer *s_field_layer, *s_hud_layer, *s_time_layer;
static FFont *s_michroma;
static GFont s_ss8, s_ss_label;  // field digits; header and bin labels

static char s_time[6] = "00:00";
static char s_date[12] = "";
static uint8_t s_batt = 100;

// Placeholder bin fill levels until complications exist (spec section 7).
static const uint8_t BIN_FILL[4] = {62, 35, 88, 14};
static const char *BIN_LABEL[4] = {"01", "02", "03", "04"};

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

// ---------------------------------------------------------------------------
// HUD: header, band, bins

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

static void draw_bins(GContext *ctx, const LayoutA *a) {
  // Spread the bins across the area; widths differ by at most 1px.
  const int16_t span = a->bins.size.w + a->bin_gap;
  for (int i = 0; i < 4; i++) {
    const int16_t bx0 = a->bins.origin.x + (span * i + 2) / 4;
    const int16_t bx1 = a->bins.origin.x + (span * (i + 1) + 2) / 4 - a->bin_gap;
    GRect b = GRect(bx0, a->bins.origin.y, bx1 - bx0, a->bins.size.h);
    outline_rect(ctx, b);
    draw_ss(ctx, BIN_LABEL[i], s_ss_label, b.origin.x, b.size.w,
            b.origin.y + a->bin_label_cap_top, GTextAlignmentCenter, COL_TIME);
    const int16_t tx = b.origin.x + (b.size.w - a->bin_bar_w) / 2;
    const int16_t ty = b.origin.y + a->bin_bar_top;
    fill(ctx, COL_TRACK, GRect(tx, ty, a->bin_bar_w, a->bin_bar_h));
    fill(ctx, COL_ACCENT, GRect(tx, ty, (a->bin_bar_w * BIN_FILL[i] + 50) / 100, a->bin_bar_h));
  }
}

static void hud_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const LayoutA *a = &LAYOUT.a;
  draw_header(ctx, &LAYOUT.header_a);
  fill(ctx, GColorBlack, GRect(0, a->band_y0, bounds.size.w, a->band_y1 - a->band_y0 + 1));
  fill(ctx, COL_ACCENT, GRect(0, a->band_y0, bounds.size.w, 1));
  fill(ctx, COL_ACCENT, GRect(0, a->band_y1, bounds.size.w, 1));
  draw_bins(ctx, a);
}

// ---------------------------------------------------------------------------
// Time and date (Michroma via fctx: anti-aliased, sub-pixel)

static int tabular_index(char ch) { return ch == ':' ? 10 : ch - '0'; }

// Font units -> fixed-point pixels at em size px.
static fixed_t fu(int32_t units, int px) {
  return (fixed_t)((units * px * FIXED_POINT_SCALE + MICHROMA_UPEM / 2) / MICHROMA_UPEM);
}

// Draw a string of digits and colons with every glyph's ink centred in a fixed
// slot, so the time never shifts sideways. The row is centred on cx.
static void draw_tabular(FContext *fctx, const char *s, int px, fixed_t cx, fixed_t cap_cy) {
  int32_t total = 0;
  for (const char *p = s; *p; p++) total += (*p == ':') ? MICHROMA_COLON_SLOT : MICHROMA_DIGIT_SLOT;
  int32_t pen = -total / 2;  // font units relative to cx
  char one[2] = {0, 0};
  for (const char *p = s; *p; p++) {
    const int32_t slot = (*p == ':') ? MICHROMA_COLON_SLOT : MICHROMA_DIGIT_SLOT;
    const int32_t origin = pen + slot / 2 - MICHROMA_INK_CX[tabular_index(*p)];
    one[0] = *p;
    fctx_set_offset(fctx, FPoint(cx + fu(origin, px), cap_cy));
    fctx_begin_fill(fctx);
    fctx_draw_string(fctx, one, s_michroma, GTextAlignmentLeft, FTextAnchorCapMiddle);
    fctx_end_fill(fctx);
    pen += slot;
  }
}

static void time_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const LayoutA *a = &LAYOUT.a;
  const fixed_t cx = INT_TO_FIXED(bounds.size.w) / 2;

  FContext fctx;
  fctx_init_context(&fctx, ctx);

  fctx_set_fill_color(&fctx, COL_TIME);
  fctx_set_text_em_height(&fctx, s_michroma, a->time_px);
  draw_tabular(&fctx, s_time, a->time_px, cx, INT_TO_FIXED(a->time_cy));

  fctx_set_fill_color(&fctx, COL_DATE);
  fctx_set_text_em_height(&fctx, s_michroma, a->date_px);
  fctx_set_offset(&fctx, FPoint(cx, INT_TO_FIXED(a->date_cy)));
  fctx_begin_fill(&fctx);
  fctx_draw_string(&fctx, s_date, s_michroma, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);

  fctx_deinit_context(&fctx);
}

static void field_update(Layer *layer, GContext *ctx) {
  field_draw(ctx, layer_get_bounds(layer));
}

// ---------------------------------------------------------------------------
// Services

static void update_time(struct tm *t) {
  int hour = t->tm_hour;
  if (!clock_is_24h_style()) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  snprintf(s_time, sizeof(s_time), "%02d:%02d", hour, t->tm_min);

  char wday[8];
  strftime(wday, sizeof(wday), "%a", t);
  for (char *p = wday; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
  }
  snprintf(s_date, sizeof(s_date), "%s %d", wday, t->tm_mday);
  layer_mark_dirty(s_time_layer);
}

static void tick_handler(struct tm *t, TimeUnits changed) { update_time(t); }

static void battery_handler(BatteryChargeState st) {
  s_batt = st.charge_percent;
  layer_mark_dirty(s_hud_layer);
}

// ---------------------------------------------------------------------------
// Window

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  const GRect b = layer_get_bounds(root);
  s_field_layer = layer_create(b);
  s_hud_layer = layer_create(b);
  s_time_layer = layer_create(b);
  layer_set_update_proc(s_field_layer, field_update);
  layer_set_update_proc(s_hud_layer, hud_update);
  layer_set_update_proc(s_time_layer, time_update);
  layer_add_child(root, s_field_layer);
  layer_add_child(root, s_hud_layer);
  layer_add_child(root, s_time_layer);
}

static void window_unload(Window *w) {
  layer_destroy(s_time_layer);
  layer_destroy(s_hud_layer);
  layer_destroy(s_field_layer);
}

static void init(void) {
  s_michroma = ffont_create_from_resource(RESOURCE_ID_FFONT_MICHROMA);
  s_ss8 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_8));
#ifdef PBL_PLATFORM_BASALT
  s_ss_label = s_ss8;
#else
  s_ss_label = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_16));
#endif
  field_init(s_ss8);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){.load = window_load, .unload = window_unload});
  window_stack_push(s_window, false);

  time_t now = time(NULL);
  update_time(localtime(&now));
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_handler(battery_state_service_peek());
  battery_state_service_subscribe(battery_handler);
}

static void deinit(void) {
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
  field_deinit();
  if (s_ss_label != s_ss8) fonts_unload_custom_font(s_ss_label);
  fonts_unload_custom_font(s_ss8);
  ffont_destroy(s_michroma);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
