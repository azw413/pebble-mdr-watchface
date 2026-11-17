#!/usr/bin/env python3
"""Make the store banner and icons from the watchface's own visual elements.

Usage (from the project root, after tools/store_assets.py has made the
screenshots and GIFs):
    python3 tools/store_art.py

Output: store/banner-720x320.png, store/icon-144.png, store/icon-48.png

Uses Pillow and the bundled Michroma TTF. Michroma is drawn with a stroke to
approximate the watch's emboldened weight. Colours come from the default cyan
theme (src/c/theme.c).
"""
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORE = os.path.join(ROOT, 'store')
MICHROMA = os.path.join(ROOT, 'resources', 'fonts', 'Michroma-Regular.ttf')

BLACK = (0, 0, 0)
BRIGHT = (0xAA, 0xFF, 0xFF)   # Celeste: time
ACCENT = (0x55, 0xAA, 0xAA)   # Cadet Blue: date, lines
FIELD = (0x00, 0x55, 0x55)    # Midnight Green: number field

# The watch's thin 5x7 field digits (src/c/field.c), one int per row, bit 4 = left.
GLYPHS = [
    [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E], [0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E],
    [0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F], [0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E],
    [0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02], [0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E],
    [0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E], [0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
    [0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E], [0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C],
]


def field(im, seed=7, cell=14, colour=FIELD):
    """Cover the image with the seeded 5x7 digit grid, as on the watch."""
    w, h = im.size
    cols, rows = w // cell, h // cell + 1
    x0 = (w - cols * cell) // 2
    px = im.load()
    s = seed
    for r in range(rows):
        for c in range(cols):
            s = s * 16807 % 2147483647
            g = GLYPHS[s % 10]
            gx, gy = x0 + c * cell + (cell - 5) // 2, r * cell + (cell - 7) // 2
            for y in range(7):
                for x in range(5):
                    if g[y] & (0x10 >> x) and 0 <= gx + x < w and 0 <= gy + y < h:
                        px[gx + x, gy + y] = colour


def band(im, y0, y1):
    d = ImageDraw.Draw(im)
    d.rectangle([0, y0, im.width - 1, y1], fill=BLACK)
    d.line([0, y0, im.width - 1, y0], fill=ACCENT)
    d.line([0, y1, im.width - 1, y1], fill=ACCENT)


def bold_text(im, xy, text, px, colour, anchor='lm'):
    """Michroma with a stroke of the fill colour: roughly the watch's +144 weight."""
    font = ImageFont.truetype(MICHROMA, px)
    stroke = max(1, round(px * 72 / 2048))  # 72 units each side = 144 total
    ImageDraw.Draw(im).text(xy, text, font=font, fill=colour, anchor=anchor,
                            stroke_width=stroke, stroke_fill=colour)


def text_width(text, px):
    font = ImageFont.truetype(MICHROMA, px)
    return font.getlength(text) + 2 * max(1, round(px * 72 / 2048))


def gif_frame(path, index):
    im = Image.open(path)
    im.seek(index)
    return im.convert('RGB')


def framed(screen, pad=6):
    """A screenshot on a black plate with a 1px accent outline (a simple bezel)."""
    w, h = screen.size
    plate = Image.new('RGB', (w + 2 * pad, h + 2 * pad), BLACK)
    ImageDraw.Draw(plate).rectangle([0, 0, w + 2 * pad - 1, h + 2 * pad - 1], outline=ACCENT)
    plate.paste(screen, (pad, pad))
    return plate


def banner():
    W, H = 720, 320
    im = Image.new('RGB', (W, H), BLACK)
    field(im, seed=11)
    band(im, 96, 223)
    # Title block on the band.
    bold_text(im, (40, 140), 'REFINEMENT', 30, BRIGHT)
    ImageDraw.Draw(im).text((42, 178), 'A legible watchface for Pebble',
                            font=ImageFont.truetype(MICHROMA, 13), fill=ACCENT, anchor='lm')
    ImageDraw.Draw(im).text((42, 200), 'Time  ·  Time 2  ·  Round 2',
                            font=ImageFont.truetype(MICHROMA, 11), fill=ACCENT, anchor='lm')
    # One emery screen at native size, mid-refinement (the still screens are in
    # the screenshot slots).
    moving = framed(gif_frame(os.path.join(STORE, 'emery', 'standard-refinement.gif'), 36))
    x = W - moving.width - 40
    assert 32 + text_width('REFINEMENT', 30) < x - 16, 'title would run under the screen'
    im.paste(moving, (x, (H - moving.height) // 2))
    return im


def icon(size):
    im = Image.new('RGB', (size, size), BLACK)
    if size >= 96:
        field(im, seed=7)
        band(im, size * 30 // 100, size * 70 // 100)
        px = size * 30 // 144
        while text_width('10:10', px) > size - 12:
            px -= 1
        bold_text(im, (size // 2, size // 2), '10:10', px, BRIGHT, anchor='mm')
    else:
        # Too small for the field: a band and two bold digits.
        band(im, size // 5, size - 1 - size // 5)
        bold_text(im, (size // 2, size // 2), '10', size * 9 // 20, BRIGHT, anchor='mm')
    return im


def main():
    os.makedirs(STORE, exist_ok=True)
    outputs = {
        'banner-720x320.png': banner(),
        'icon-144.png': icon(144),
        'icon-48.png': icon(48),
    }
    for name, im in outputs.items():
        path = os.path.join(STORE, name)
        im.save(path)
        print('%s %dx%d' % (os.path.relpath(path, ROOT), im.width, im.height))


if __name__ == '__main__':
    main()
