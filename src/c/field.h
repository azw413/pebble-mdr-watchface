// Background number field (spec 4.2): a seeded grid of thin 5x7 pixel digits held
// in one 1-bit palettised bitmap. It is built once at init and then only blitted.
// Recolouring it means changing palette entry 1.
#pragma once
#include <pebble.h>

// y0: grid y origin, per layout (LayoutA/LayoutB.field_y0).
void field_init(GSize screen, int16_t y0);
void field_deinit(void);
void field_draw(GContext *ctx, GRect bounds);
void field_set_color(GColor c);
