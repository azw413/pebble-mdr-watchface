// The 5-minute "refinement" animation (docs/DESIGN_SPEC.md section 5).
// The maths follows renderVals() in docs/reference/animation-layout-{a,b}.dc.html.
// The prototype values are for emery; other platforms scale them (see AnimGeom).
// Everything is integer: ratios are Q16 (65536 = 1.0); positions are fctx fixed
// point (1/16 px).
#pragma once
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include "layout.h"

// Anim.t runs on the prototypes' timeline ("design ms", 0-2600). The watch plays
// it ANIM_SLOWDOWN times slower in real time: the same motion over a longer
// time, so the refinement is easier to follow (5.2 s in total).
#define ANIM_DURATION_MS 2600
#define ANIM_SLOWDOWN 2
#define ANIM_FRAME_MS 33   // real ms between frames (~30 fps)
#define ANIM_MAX_GLYPHS 10
#define Q16_ONE 65536

typedef enum { GLYPH_TIME, GLYPH_HIGHLIGHT } GlyphColor;

typedef struct {
  char ch;
  FPoint pos;        // ink centre x, cap centre y
  int32_t em16;      // em size, 1/16 px
  GlyphColor color;
} AnimGlyph;

// Geometry fixed at the start of a run, worked out by the caller from the layout.
typedef struct {
  int32_t fs16;           // resting em size of the time digits, 1/16 px
  FPoint slot[5];         // resting positions. A: H H : M M. B: H H M M (row-major)
  FPoint cell[4];         // centres of the four source field cells
  FPoint bin_mouth;       // A: where old digits end up inside the chosen bin
  int32_t start_em16;     // new digits start at the field digit size
  int32_t k_q16;          // size scale against emery (time px / 46, or B digit px / 84)
  int32_t kh_q16;         // screen height scale against emery (h / 228)
  int16_t screen_h;
} AnimGeom;

typedef struct {
  bool active;
  int32_t t;              // design ms since start (real ms / ANIM_SLOWDOWN)
  LayoutId layout;
  char from[6], to[6];    // "HH:MM"
  uint8_t bin;            // A: the bin that opens
  uint8_t cell_c[4], cell_r[4];
  uint8_t order[4];       // B: drop order
  int32_t drift16[4];     // B: sideways drift, 1/16 px
  AnimGeom geom;
} Anim;

// Everything a frame needs to draw.
typedef struct {
  AnimGlyph glyphs[ANIM_MAX_GLYPHS];
  uint8_t n_glyphs;
  int32_t band_open;      // Q16: 1 = fully open, 0 = folded to the centre line
  bool field_lit;         // field shows its brighter colour
  bool cells_hidden;      // source cells are blank (their digits are in flight)
  int32_t lid;            // A: chosen bin's lid opening, Q16 (1 = 110 degrees)
  bool bin_flash;         // A: chosen bin's outline uses the bright colour
  bool bin_bumped;        // A: chosen bin's fill shows its +6%
} AnimFrame;

// Pick the random parts of a run (bin, drop order, drift) into *a. The caller
// fills a->geom, the cells and from/to.
void anim_randomise(Anim *a);
void anim_frame(const Anim *a, AnimFrame *out);
