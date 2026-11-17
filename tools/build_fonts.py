#!/usr/bin/env python3
"""Convert Michroma (TTF) to a pebble-fctx .ffont resource.

fctx-compiler only reads SVG fonts, and FontForge is not required: this script
writes a minimal SVG font from the TTF outlines with fontTools, then runs the
locally installed compiler on it.

Michroma has a single (Regular) weight. For legibility on the watch the outlines
are thickened at build time (synthetic bold, FreeType's FT_Outline_Embolden
algorithm), so there is no runtime cost and the curves stay curves.

Usage (from the project root, after `npm install`):
    python3 tools/build_fonts.py [--embolden UNITS]

Output: resources/fonts/michroma.ffont
        src/c/michroma_metrics.h  (ink bounds for tabular digit slots)
"""
import argparse
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
from xml.sax.saxutils import quoteattr

from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._g_l_y_f import GlyphCoordinates

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, 'resources', 'fonts')
COMPILER = os.path.join(ROOT, 'node_modules', '.bin', 'fctx-compiler')
SOURCE = 'Michroma-Regular.ttf'
FONT_ID = 'michroma'
# Time digits + colon, and uppercase letters for the date (e.g. "TUE 17").
CHARS = '0123456789: ABCDEFGHIJKLMNOPQRSTUVWXYZ'
TABULAR = '0123456789:'

# Extra stroke thickness in font units (upem 2048). Michroma's own strokes are
# 160 (horizontal bars) to 192 (vertical stems) units.
EMBOLDEN_DEFAULT = 144  # chosen from emulator comparisons: bold and punchy, still Michroma


def embolden_contour(pts, r):
    """Push a closed TrueType contour outward by r units (FT_Outline_EmboldenXY).

    TrueType contours keep the ink on the right of the direction of travel, so
    the outward normal of an edge (dx, dy) is (-dy, dx). Every point (on- or
    off-curve) moves along the bisector of its two edges' normals, scaled so
    that both edges move by r. Sharp U-turns are left alone, and the shift is
    capped by the shorter edge so small features don't fold over.
    """
    n = len(pts)
    out_pts = []
    for i in range(n):
        p = pts[i]
        # Nearest distinct neighbours (duplicate points have no direction).
        j = (i - 1) % n
        while j != i and pts[j] == p:
            j = (j - 1) % n
        k = (i + 1) % n
        while k != i and pts[k] == p:
            k = (k + 1) % n
        if j == i or k == i:
            out_pts.append(p)
            continue
        ix, iy = p[0] - pts[j][0], p[1] - pts[j][1]
        ox, oy = pts[k][0] - p[0], pts[k][1] - p[1]
        l_in, l_out = math.hypot(ix, iy), math.hypot(ox, oy)
        ix, iy, ox, oy = ix / l_in, iy / l_in, ox / l_out, oy / l_out
        d = ix * ox + iy * oy
        if d <= -0.9375:  # turn sharper than ~160 degrees: don't move
            out_pts.append(p)
            continue
        d += 1
        sx, sy = -(iy + oy), ix + ox
        q = -(ox * iy - oy * ix)
        lim = min(l_in, l_out)
        scale = r / d if r * q <= lim * d else lim / q
        out_pts.append((p[0] + sx * scale, p[1] + sy * scale))
    return out_pts


def load_font(strength):
    """The source font, with the CHARS glyphs emboldened by `strength` units."""
    f = TTFont(os.path.join(FONTS, SOURCE))
    if strength:
        glyf, cmap = f['glyf'], f.getBestCmap()
        for name in {cmap[ord(ch)] for ch in CHARS}:
            g = glyf[name]
            if g.isComposite():
                sys.exit('composite glyph %s: decompose before emboldening' % name)
            if g.numberOfContours <= 0:
                continue
            coords, ends, _ = g.getCoordinates(glyf)
            pts, new, start = list(coords), [], 0
            for end in ends:
                new.extend(embolden_contour(pts[start:end + 1], strength / 2))
                start = end + 1
            g.coordinates = GlyphCoordinates([(round(x), round(y)) for x, y in new])
            g.recalcBounds(glyf)
    return f


def svg_font(f):
    gs = f.getGlyphSet()
    cmap = f.getBestCmap()
    head, hhea, os2 = f['head'], f['hhea'], f['OS/2']
    cap = getattr(os2, 'sCapHeight', 0) or int(head.unitsPerEm * 0.7)
    out = [
        '<?xml version="1.0" standalone="no"?>',
        '<svg xmlns="http://www.w3.org/2000/svg"><defs>',
        '<font id=%s horiz-adv-x="%d">' % (quoteattr(FONT_ID), head.unitsPerEm),
        # cap-height stays the source value: emboldening grows the caps by the
        # same amount above and below, so their centre (the anchor) is unchanged.
        '<font-face font-family=%s units-per-em="%d" ascent="%d" descent="%d" cap-height="%d"/>'
        % (quoteattr(FONT_ID), head.unitsPerEm, hhea.ascent, hhea.descent, cap),
    ]
    for ch in CHARS:
        name = cmap.get(ord(ch))
        if name is None:
            sys.exit('%s has no glyph for %r' % (SOURCE, ch))
        pen = SVGPathPen(gs)
        gs[name].draw(pen)
        d = pen.getCommands()
        attrs = 'unicode=%s glyph-name=%s horiz-adv-x="%d"' % (
            quoteattr(ch), quoteattr(name), gs[name].width)
        out.append('<glyph %s%s/>' % (attrs, ' d=%s' % quoteattr(d) if d else ''))
    out += ['</font>', '</defs></svg>']
    return '\n'.join(out), dict(upem=head.unitsPerEm, ascent=hhea.ascent,
                                descent=hhea.descent, cap=cap)


def metrics_header(f, strength):
    """Emit ink centres of the tabular glyphs so C can centre each in its slot."""
    gs, cmap = f.getGlyphSet(), f.getBestCmap()
    lines = [
        '// Generated by tools/build_fonts.py from %s (emboldened by %d units).'
        % (SOURCE, strength),
        '// Do not edit.',
        '#pragma once',
        '#define MICHROMA_UPEM %d' % f['head'].unitsPerEm,
        '#define MICHROMA_CAP_HEIGHT %d' % f['OS/2'].sCapHeight,
        '#define MICHROMA_EMBOLDEN %d' % strength,
        '// Tabular slot widths (spec 3): every digit gets the 0.951 em "0" advance.',
        '#define MICHROMA_DIGIT_SLOT %d' % gs[cmap[ord('0')]].width,
        '#define MICHROMA_COLON_SLOT %d' % gs[cmap[ord(':')]].width,
        '// Ink centre x (font units, from the glyph origin) for "0123456789:".',
        'static const int16_t MICHROMA_INK_CX[11] = {',
    ]
    vals = []
    for ch in TABULAR:
        pen = BoundsPen(gs)
        gs[cmap[ord(ch)]].draw(pen)
        x0, _, x1, _ = pen.bounds
        vals.append('%d' % round((x0 + x1) / 2))
    lines.append('  ' + ', '.join(vals))
    lines.append('};')
    return '\n'.join(lines) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--embolden', type=int, default=EMBOLDEN_DEFAULT,
                    help='extra stroke thickness in font units (default %(default)s)')
    args = ap.parse_args()
    if not os.path.exists(COMPILER):
        sys.exit('fctx-compiler not found; run `npm install` first')
    f = load_font(args.embolden)
    svg, m = svg_font(f)
    # Run in an empty dir with no resources/ subdir so the compiler writes to cwd.
    with tempfile.TemporaryDirectory() as tmp:
        svg_path = os.path.join(tmp, FONT_ID + '.svg')
        with open(svg_path, 'w') as fh:
            fh.write(svg)
        regex = '[' + re.escape(CHARS) + ']'
        subprocess.run([COMPILER, svg_path, '-r', regex], cwd=tmp, check=True,
                       stdout=subprocess.DEVNULL)
        dest = os.path.join(FONTS, FONT_ID + '.ffont')
        shutil.copyfile(os.path.join(tmp, FONT_ID + '.ffont'), dest)
    print('%s: %d glyphs, %d bytes, embolden %d  (upem %d, ascent %d, descent %d, cap %d)'
          % (os.path.relpath(dest, ROOT), len(CHARS), os.path.getsize(dest), args.embolden,
             m['upem'], m['ascent'], m['descent'], m['cap']))
    hdr = os.path.join(ROOT, 'src', 'c', 'michroma_metrics.h')
    with open(hdr, 'w') as fh:
        fh.write(metrics_header(f, args.embolden))
    print(os.path.relpath(hdr, ROOT))


if __name__ == '__main__':
    main()
