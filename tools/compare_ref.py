#!/usr/bin/env python3
"""Compare an emulator screenshot with a reference raster, side by side.

Usage: python3 tools/compare_ref.py reference.png shot.png out.png [--panel N]

The reference may be docs/reference/pebble_time_pixels_v3.png. In that case
--panel picks the 4x-enlarged screen to use (0 = A, 1 = B), sampled back to 1x.
Output panels: reference | shot | diff. In the diff, red marks pixels lit only in
the reference and green marks pixels lit only in the shot. Number-field pixels
are ignored, because the seeded digits differ between generators.
"""
import argparse
from PIL import Image

FIELD = {(0, 85, 85), (0, 170, 170)}  # Midnight Green, Tiffany Blue


def load_ref(path, panel, size):
    im = Image.open(path).convert('RGB')
    if im.size == size:
        return im
    w, h = size  # contact sheet from render_preview.py: pad 24, scale 4
    x0 = 24 + panel * (w * 4 + 24)
    out = Image.new('RGB', size)
    for y in range(h):
        for x in range(w):
            out.putpixel((x, y), im.getpixel((x0 + x * 4 + 2, 24 + y * 4 + 2)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ref'), ap.add_argument('shot'), ap.add_argument('out')
    ap.add_argument('--panel', type=int, default=0)
    ap.add_argument('--scale', type=int, default=4)
    a = ap.parse_args()
    # pebble-tool screenshots carry an LCD colour profile; snap each channel back
    # to the 64-colour palette levels (0, 85, 170, 255).
    shot = Image.open(a.shot).convert('RGB').point(lambda v: min((0, 85, 170, 255), key=lambda l: abs(l - v)))
    ref = load_ref(a.ref, a.panel, shot.size)
    w, h = shot.size
    diff = Image.new('RGB', shot.size)
    lit = lambda p: p != (0, 0, 0) and p not in FIELD
    n_ref = n_shot = 0
    for y in range(h):
        for x in range(w):
            r, s = lit(ref.getpixel((x, y))), lit(shot.getpixel((x, y)))
            if r and not s:
                diff.putpixel((x, y), (255, 60, 60)); n_ref += 1
            elif s and not r:
                diff.putpixel((x, y), (60, 255, 60)); n_shot += 1
            elif r and s:
                diff.putpixel((x, y), (70, 70, 70))
    k, pad = a.scale, 8
    sheet = Image.new('RGB', (3 * w * k + 4 * pad, h * k + 2 * pad), (20, 20, 20))
    for i, im in enumerate((ref, shot, diff)):
        sheet.paste(im.resize((w * k, h * k), Image.NEAREST), (pad + i * (w * k + pad), pad))
    sheet.save(a.out)
    print('only in reference: %d px, only in shot: %d px' % (n_ref, n_shot))


if __name__ == '__main__':
    main()
