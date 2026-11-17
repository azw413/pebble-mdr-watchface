// Step 1 smoke test: proves the toolchain (fctx + Michroma .ffont, Silkscreen
// bitmap fonts) builds and renders on every target. Replaced in step 2 by the
// real layout code.
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>

static Window *s_window;
static Layer *s_canvas;
static FFont *s_michroma;
static GFont s_silk;

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  FContext fctx;
  fctx_init_context(&fctx, ctx);
  fctx_set_fill_color(&fctx, GColorCeleste);
  fctx_set_offset(&fctx, FPointI(b.size.w / 2, b.size.h / 2));
  fctx_set_text_em_height(&fctx, s_michroma, PBL_IF_RECT_ELSE(b.size.w < 180 ? 32 : 46, 58));
  fctx_begin_fill(&fctx);
  fctx_draw_string(&fctx, "10:09", s_michroma, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);
  fctx_deinit_context(&fctx);

  graphics_context_set_text_color(ctx, GColorCeleste);
  graphics_draw_text(ctx, "73% MDR 0123456789", s_silk,
                     GRect(0, b.size.h / 2 + 30, b.size.w, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *w) { layer_destroy(s_canvas); }

int main(void) {
  s_michroma = ffont_create_from_resource(RESOURCE_ID_FFONT_MICHROMA);
#ifdef PBL_PLATFORM_BASALT
  s_silk = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_8));
#else
  s_silk = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_16));
#endif
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){.load = window_load, .unload = window_unload});
  window_stack_push(s_window, false);
  app_event_loop();
  window_destroy(s_window);
  fonts_unload_custom_font(s_silk);
  ffont_destroy(s_michroma);
}
