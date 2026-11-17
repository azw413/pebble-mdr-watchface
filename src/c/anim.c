#include "anim.h"

// ---------------------------------------------------------------------------
// Q16 helpers

static int32_t clamp_q(int32_t v) { return v < 0 ? 0 : v > Q16_ONE ? Q16_ONE : v; }
static int32_t mul_q(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }
static int32_t ratio_q(int32_t num, int32_t den) { return (int32_t)((int64_t)num * Q16_ONE / den); }

// easeInOutCubic(v) = v < .5 ? 4v^3 : 1 - (-2v + 2)^3 / 2
static int32_t ease_q(int32_t v) {
  v = clamp_q(v);
  if (v < Q16_ONE / 2) return 4 * mul_q(mul_q(v, v), v);
  const int32_t w = 2 * Q16_ONE - 2 * v;
  return Q16_ONE - mul_q(mul_q(w, w), w) / 2;
}

// sin of an angle in Pebble trig units (TRIG_MAX_ANGLE = one turn), as Q16.
static int32_t sin_q(int32_t angle) {
  return (int32_t)((int64_t)sin_lookup(angle & (TRIG_MAX_ANGLE - 1)) * Q16_ONE / TRIG_MAX_RATIO);
}

// sin(pi * v) for v in Q16 [0, 1]: the 0 -> 1 -> 0 hump used for swell and pulse.
static int32_t hump_q(int32_t v) { return sin_q(mul_q(TRIG_MAX_ANGLE / 2, clamp_q(v))); }

// The prototypes use radians: t/70, t/55 and phase 1.3*j. In trig units
// (65536 per 2*pi radians) that is t*149.8, t*190.7 and 13 557*j.
#define WOB_PER_MS 150
#define BOB_PER_MS 191
#define PHASE_STEP 13557

static int32_t lerp(int32_t a, int32_t b, int32_t u) { return a + mul_q(b - a, u); }

// Emery reference value (px, may be fractional) scaled by k, as fctx fixed point.
static int32_t scaled16(int32_t emery_px16, int32_t k_q16) { return mul_q(emery_px16, k_q16); }

// ---------------------------------------------------------------------------

void anim_randomise(Anim *a) {
  a->bin = rand() % 4;
  for (int i = 0; i < 4; i++) a->order[i] = i;
  for (int i = 3; i > 0; i--) {  // Fisher-Yates
    const int j = rand() % (i + 1);
    const uint8_t tmp = a->order[i];
    a->order[i] = a->order[j];
    a->order[j] = tmp;
  }
  for (int i = 0; i < 4; i++) {
    // drift in [-16, 16] emery px
    const int32_t d = (rand() % (2 * 16 * 16 + 1)) - 16 * 16;
    a->drift16[i] = scaled16(d, a->geom.k_q16);
  }
}

// Agitate phase (spec 5.1 step 1): swell and bob, out of step per glyph.
// away = 0: swell about the cap centre and bob both ways (the prototype).
// away = -1 / +1 (B's hours / minutes rows): swell from the edge next to the gap
// between the rows and bob only away from it, so the two rows never touch. With
// the prototype's motion, "10 / 10" briefly read as "18 / 18".
static void agitate(const Anim *a, int j, int32_t swell_q, int32_t bob16, int away,
                    FPoint home, AnimGlyph *g) {
  const int32_t t = a->t;
  const int32_t ph = PHASE_STEP * j;
  const int32_t bump = hump_q(ratio_q(t, 700));
  const int32_t wob = Q16_ONE * 3 / 4 + sin_q(t * WOB_PER_MS + ph) / 4;
  const int32_t osc = sin_q(t * BOB_PER_MS + ph);
  g->em16 = a->geom.fs16 + mul_q(a->geom.fs16, mul_q(swell_q, mul_q(bump, wob)));
  if (away == 0) {
    g->pos = FPoint(home.x, home.y + mul_q(bob16, mul_q(bump, osc)));
    return;
  }
  // Caps are 0.75 em, so holding one edge moves the cap centre by 3/8 of the growth.
  const int32_t grow = (g->em16 - a->geom.fs16) * 3 / 8;
  const int32_t bob = mul_q(bob16, mul_q(bump, (Q16_ONE + osc) / 2));
  g->pos = FPoint(home.x, home.y + away * (grow + bob));
}

// New digit k (spec 5.1 step 3): pulse in its cell, then glide and grow into its slot.
static bool emerge(const Anim *a, int k, FPoint slot, AnimGlyph *g) {
  const int32_t t = a->t;
  if (t < 700) return false;
  const AnimGeom *ge = &a->geom;
  const FPoint src = ge->cell[k];
  const int32_t st = 900 + 90 * k;
  // Pulse 8 -> 13 -> 8 px in the prototype: peak is 13/8 of the start size.
  const int32_t peak16 = ge->start_em16 * 13 / 8;
  if (t < st) {
    const int32_t q = clamp_q(ratio_q(t - 700, st - 700));
    g->pos = src;
    g->em16 = ge->start_em16 + mul_q(peak16 - ge->start_em16, hump_q(q));
    g->color = GLYPH_HIGHLIGHT;
    return true;
  }
  const int32_t pr = clamp_q(ratio_q(t - st, 1100));
  const int32_t e = ease_q(pr);
  g->pos = FPoint(lerp(src.x, slot.x, e), lerp(src.y, slot.y, e));
  g->em16 = lerp(ge->start_em16, ge->fs16, e);
  g->color = pr >= Q16_ONE ? GLYPH_TIME : GLYPH_HIGHLIGHT;
  return true;
}

static void frame_a(const Anim *a, AnimFrame *f) {
  const AnimGeom *ge = &a->geom;
  const int32_t t = a->t;
  // Old "HH:MM" (colon included): agitate, then a quadratic Bezier into the bin.
  for (int j = 0; j < 5; j++) {
    const int32_t st = 700 + 70 * j;
    const int32_t pr = clamp_q(ratio_q(t - st, 800));
    if (t >= st && pr >= Q16_ONE) continue;  // inside the bin
    AnimGlyph *g = &f->glyphs[f->n_glyphs++];
    g->ch = a->from[j];
    g->color = GLYPH_TIME;
    if (t < st) {
      agitate(a, j, Q16_ONE * 16 / 100, scaled16(56, ge->k_q16), 0, ge->slot[j], g);  // 0.16, 3.5px
    } else {
      const int32_t u = ease_q(pr), v = Q16_ONE - u;
      const FPoint s = ge->slot[j];
      const FPoint e = FPoint(ge->bin_mouth.x + (j - 2) * scaled16(48, ge->k_q16),  // 3px apart
                              ge->bin_mouth.y);
      const FPoint m = FPoint((s.x + e.x) / 2, ge->slot[0].y - scaled16(36 * 16, ge->k_q16));
      const int32_t vv = mul_q(v, v), uv2 = 2 * mul_q(u, v), uu = mul_q(u, u);
      g->pos = FPoint(mul_q(s.x, vv) + mul_q(m.x, uv2) + mul_q(e.x, uu),
                      mul_q(s.y, vv) + mul_q(m.y, uv2) + mul_q(e.y, uu));
      g->em16 = lerp(ge->fs16, scaled16(9 * 16, ge->k_q16), u);  // shrink to 9px
    }
  }
  // New digits into H, H, M, M; the colon returns at 2000.
  static const uint8_t SLOT_OF[4] = {0, 1, 3, 4};
  for (int k = 0; k < 4; k++) {
    AnimGlyph *g = &f->glyphs[f->n_glyphs];
    g->ch = a->to[SLOT_OF[k]];
    if (emerge(a, k, ge->slot[SLOT_OF[k]], g)) f->n_glyphs++;
  }
  if (t >= 2000) {
    f->glyphs[f->n_glyphs++] = (AnimGlyph){.ch = ':', .pos = ge->slot[2], .em16 = ge->fs16,
                                           .color = GLYPH_TIME};
  }
  f->lid = ease_q(ratio_q(t - 600, 250)) - ease_q(ratio_q(t - 1750, 250));
  f->bin_flash = t >= 1500 && t < 2000;
  f->bin_bumped = t >= 1600;
}

static void frame_b(const Anim *a, AnimFrame *f) {
  const AnimGeom *ge = &a->geom;
  const int32_t t = a->t;
  const char from[4] = {a->from[0], a->from[1], a->from[3], a->from[4]};
  const char to[4] = {a->to[0], a->to[1], a->to[3], a->to[4]};
  // Old digits: agitate, then fall off the bottom in a random order.
  for (int j = 0; j < 4; j++) {
    const int32_t st = 700 + 110 * a->order[j];
    AnimGlyph g = {.ch = from[j], .color = GLYPH_TIME};
    if (t < st) {
      agitate(a, j, Q16_ONE * 12 / 100, scaled16(4 * 16, ge->k_q16), j < 2 ? -1 : 1,
              ge->slot[j], &g);
    } else {
      const int32_t u = ratio_q(t - st, 900);  // linear time: natural gravity
      const int32_t up16 = scaled16(26 * 16, ge->kh_q16), g16 = scaled16(300 * 16, ge->kh_q16);
      g.pos = FPoint(ge->slot[j].x + mul_q(a->drift16[j], u),
                     ge->slot[j].y - mul_q(up16, u) + mul_q(g16, mul_q(u, u)));
      g.em16 = ge->fs16;
      // Hidden once fully below the screen (cap top past the bottom edge).
      if (g.pos.y - g.em16 * 2 / 5 >= INT_TO_FIXED(ge->screen_h)) continue;
    }
    f->glyphs[f->n_glyphs++] = g;
  }
  for (int k = 0; k < 4; k++) {
    AnimGlyph *g = &f->glyphs[f->n_glyphs];
    g->ch = to[k];
    if (emerge(a, k, ge->slot[k], g)) f->n_glyphs++;
  }
}

void anim_frame(const Anim *a, AnimFrame *f) {
  const int32_t t = a->t;
  *f = (AnimFrame){0};
  // Band folds to its centre line over 250-700 and unfolds over 1900-2400.
  f->band_open = t < 1900 ? Q16_ONE - ease_q(ratio_q(t - 250, 450))
                          : ease_q(ratio_q(t - 1900, 500));
  f->field_lit = t > 600 && t < 2100;
  f->cells_hidden = t >= 700;
  if (a->layout == LAYOUT_A) frame_a(a, f);
  else frame_b(a, f);
}
