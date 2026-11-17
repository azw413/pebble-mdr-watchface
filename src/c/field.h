// Background number field (spec 4.2): a seeded grid of Silkscreen digits held in
// one 1-bit palettised bitmap. It is rasterised once, the first time it is drawn,
// and then only blitted. Recolouring it means changing palette entry 1.
#pragma once
#include <pebble.h>

void field_init(GFont digit_font);
void field_deinit(void);

// Draw the field. On the first call this also builds the bitmap: it draws the
// digits as text into the framebuffer and captures them.
void field_draw(GContext *ctx, GRect bounds);

void field_set_color(GColor c);
