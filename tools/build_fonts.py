#!/usr/bin/env python3
"""Convert Michroma (TTF) to a pebble-fctx .ffont resource.

fctx-compiler only reads SVG fonts, and FontForge is not required: this script
writes a minimal SVG font from the TTF outlines with fontTools, then runs the
locally installed compiler on it.

Usage (from the project root, after `npm install`):
    python3 tools/build_fonts.py

Output: resources/fonts/michroma.ffont
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from xml.sax.saxutils import quoteattr

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, 'resources', 'fonts')
COMPILER = os.path.join(ROOT, 'node_modules', '.bin', 'fctx-compiler')

# (source ttf, output id, characters to include)
# Time digits + colon, and uppercase letters for the date (e.g. "TUE 17").
JOBS = [
    ('Michroma-Regular.ttf', 'michroma', '0123456789: ABCDEFGHIJKLMNOPQRSTUVWXYZ'),
]


def svg_font(ttf_path, font_id, chars):
    f = TTFont(ttf_path)
    gs = f.getGlyphSet()
    cmap = f.getBestCmap()
    head, hhea, os2 = f['head'], f['hhea'], f['OS/2']
    cap = getattr(os2, 'sCapHeight', 0) or int(head.unitsPerEm * 0.7)
    out = [
        '<?xml version="1.0" standalone="no"?>',
        '<svg xmlns="http://www.w3.org/2000/svg"><defs>',
        '<font id=%s horiz-adv-x="%d">' % (quoteattr(font_id), head.unitsPerEm),
        '<font-face font-family=%s units-per-em="%d" ascent="%d" descent="%d" cap-height="%d"/>'
        % (quoteattr(font_id), head.unitsPerEm, hhea.ascent, hhea.descent, cap),
    ]
    for ch in chars:
        name = cmap.get(ord(ch))
        if name is None:
            sys.exit('%s has no glyph for %r' % (ttf_path, ch))
        pen = SVGPathPen(gs)
        gs[name].draw(pen)
        d = pen.getCommands()
        attrs = 'unicode=%s glyph-name=%s horiz-adv-x="%d"' % (
            quoteattr(ch), quoteattr(name), gs[name].width)
        out.append('<glyph %s%s/>' % (attrs, ' d=%s' % quoteattr(d) if d else ''))
    out += ['</font>', '</defs></svg>']
    return '\n'.join(out), dict(upem=head.unitsPerEm, ascent=hhea.ascent,
                                descent=hhea.descent, cap=cap)


def main():
    if not os.path.exists(COMPILER):
        sys.exit('fctx-compiler not found; run `npm install` first')
    for ttf, font_id, chars in JOBS:
        svg, m = svg_font(os.path.join(FONTS, ttf), font_id, chars)
        # Run in an empty dir with no resources/ subdir so the compiler writes to cwd.
        with tempfile.TemporaryDirectory() as tmp:
            svg_path = os.path.join(tmp, font_id + '.svg')
            with open(svg_path, 'w') as fh:
                fh.write(svg)
            regex = '[' + re.escape(chars) + ']'
            subprocess.run([COMPILER, svg_path, '-r', regex], cwd=tmp, check=True,
                           stdout=subprocess.DEVNULL)
            produced = os.path.join(tmp, font_id + '.ffont')
            dest = os.path.join(FONTS, font_id + '.ffont')
            shutil.copyfile(produced, dest)
        print('%s: %d glyphs, %d bytes  (upem %d, ascent %d, descent %d, cap %d)'
              % (os.path.relpath(dest, ROOT), len(chars), os.path.getsize(dest),
                 m['upem'], m['ascent'], m['descent'], m['cap']))


if __name__ == '__main__':
    main()
