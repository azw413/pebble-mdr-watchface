// MDR watchface: layouts A "Standard" and B "Large" (docs/DESIGN_SPEC.md 4.3, 4.4).
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include "layout.h"
#include "field.h"
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

static Window *s_window;
static Layer *s_field_layer, *s_hud_layer, *s_time_layer;
static FFont *s_michroma;
static GFont s_ss_label;  // Silkscreen: battery percentage and bin labels

static Settings s_settings;
#define s_layout (s_settings.layout)

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

static void draw_band(GContext *ctx, int16_t w, int16_t y0, int16_t y1) {
  fill(ctx, GColorBlack, GRect(0, y0, w, y1 - y0 + 1));
  fill(ctx, COL_ACCENT, GRect(0, y0, w, 1));
  fill(ctx, COL_ACCENT, GRect(0, y1, w, 1));
}

static void hud_update(Layer *layer, GContext *ctx) {
  const int16_t w = layer_get_bounds(layer).size.w;
  if (s_layout == LAYOUT_A) {
    const LayoutA *a = &LAYOUT.a;
    draw_header(ctx, &a->header);
    draw_band(ctx, w, a->band_y0, a->band_y1);
    draw_bins(ctx, a);
  } else {
    const LayoutB *b = &LAYOUT.b;
    draw_header(ctx, &b->header);
    draw_band(ctx, w, b->band_y0, b->band_y1);
  }
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

static void draw_date(FContext *fctx, int px, fixed_t cx, fixed_t cy) {
  fctx_set_fill_color(fctx, COL_DATE);
  fctx_set_text_em_height(fctx, s_michroma, px);
  fctx_set_offset(fctx, FPoint(cx, cy));
  fctx_begin_fill(fctx);
  fctx_draw_string(fctx, s_date, s_michroma, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);
}

static void time_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const fixed_t cx = INT_TO_FIXED(bounds.size.w) / 2;

  FContext fctx;
  if (s_layout == LAYOUT_B) {
    // Black backing box so the field doesn't show through behind the date. Drawn
    // before the main fctx pass so plain graphics calls never overlap an fctx context.
    const LayoutB *b = &LAYOUT.b;
    fctx_init_context(&fctx, ctx);
    fctx_set_text_em_height(&fctx, s_michroma, b->date_px);
    const int16_t w = FIXED_TO_INT(fctx_string_width(&fctx, s_date, s_michroma) + FIX1 - 1);
    fctx_deinit_context(&fctx);
    fill(ctx, GColorBlack, GRect(bounds.size.w / 2 - w / 2 - b->date_box_pad, b->date_box_y,
                                 w + 2 * b->date_box_pad, b->date_box_h));
  }

  fctx_init_context(&fctx, ctx);
  fctx_set_fill_color(&fctx, COL_TIME);

  if (s_layout == LAYOUT_A) {
    const LayoutA *a = &LAYOUT.a;
    fctx_set_text_em_height(&fctx, s_michroma, a->time_px);
    draw_tabular(&fctx, s_time, a->time_px, cx, a->time_cy);
    draw_date(&fctx, a->date_px, cx, a->date_cy);
  } else {
    const LayoutB *b = &LAYOUT.b;
    char hh[3] = {s_time[0], s_time[1], 0};
    fctx_set_text_em_height(&fctx, s_michroma, b->digit_px);
    draw_tabular(&fctx, hh, b->digit_px, cx, b->hours_cy);
    draw_tabular(&fctx, s_time + 3, b->digit_px, cx, b->mins_cy);
    draw_date(&fctx, b->date_px, cx, b->date_cy);
  }

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
  else field_set_color(s_theme.field);
  if (s_field_layer) layer_mark_dirty(s_field_layer);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  const LayoutId old_layout = s_layout;
  if (settings_update_from_message(&s_settings, iter)) {
    settings_save(&s_settings);
    apply_settings(s_layout != old_layout);
  }
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
#ifdef PBL_PLATFORM_BASALT
  s_ss_label = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_8));
#else
  s_ss_label = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_16));
#endif
  s_settings = settings_load();
  apply_settings(true);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){.load = window_load, .unload = window_unload});
  window_stack_push(s_window, false);

  time_t now = time(NULL);
  update_time(localtime(&now));
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_handler(battery_state_service_peek());
  battery_state_service_subscribe(battery_handler);

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);
}

static void deinit(void) {
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
