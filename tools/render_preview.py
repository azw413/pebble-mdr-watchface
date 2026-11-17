# Usage: python3 render_preview.py Michroma-Regular.ttf Silkscreen-Regular.ttf out.png
# Rasterises basalt (144x168) layouts A/B at true pixels in the Pebble 64-colour palette, enlarged 4x.
import sys, random
from PIL import Image, ImageDraw, ImageFont
MICH, SILK, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
W, H = 144, 168
C = lambda h: tuple(int(h[i:i+2], 16) for i in (1, 3, 5))
TIME, DATE, ACC, GRID, K = C('#AAFFFF'), C('#55AAAA'), C('#55AAAA'), C('#005555'), (0, 0, 0)
q = lambda im: im.point(lambda v: round(v / 85) * 85)
M = lambda s: ImageFont.truetype(MICH, s); S8 = ImageFont.truetype(SILK, 8)
def big(im, s, sz, col, cy):  # anti-aliased Michroma, cap-centred on cy
    lay = Image.new('L', (W, H), 0); d = ImageDraw.Draw(lay); d.fontmode = 'L'; fo = M(sz)
    x0, _, x1, _ = d.textbbox((0, 0), s, font=fo); cap = sz * 0.75
    d.text(((W - (x1 - x0)) / 2 - x0, cy - cap / 2 - (fo.getmetrics()[0] - cap)), s, font=fo, fill=255)
    im.paste(Image.new('RGB', (W, H), col), (0, 0), lay)
def small(d, xy, s, col):  # 1-bit Silkscreen at native 8px
    d.fontmode = '1'; d.text(xy, s, font=S8, fill=col)
def screen(v, batt=73):
    im = Image.new('RGB', (W, H), K); d = ImageDraw.Draw(im); rnd = random.Random(7)
    for r in range(14):
        for c in range(12): small(d, (c * 12 + 4, r * 12 - 2), str(rnd.randint(0, 9)), GRID)
    d.rectangle([5, 5, 138, 19], fill=K, outline=ACC)
    small(d, (9, 6), '%d%%' % batt, TIME)
    bx0, bx1 = 37, 131
    d.rectangle([bx0, 9, bx1, 15], outline=ACC); d.rectangle([bx0 + 2, 11, bx0 + 2 + int((bx1 - bx0 - 4) * batt / 100), 13], fill=ACC if batt > 20 else C('#FF5555'))
    d.rectangle([132, 11, 133, 13], fill=ACC)
    if v == 'A':
        d.rectangle([0, 52, 143, 127], fill=K); d.line([0, 52, 143, 52], fill=ACC); d.line([0, 127, 143, 127], fill=ACC)
        big(im, '10:09', 32, TIME, 78); big(im, 'TUE 17', 18, DATE, 112)
        for i, p in enumerate([62, 35, 88, 14]):
            x = 5 + i * 34; d.rectangle([x, 136, x + 29, 161], fill=K, outline=ACC)
            w = d.textlength('0%d' % (i + 1), font=S8); small(d, (x + 15 - w // 2, 136), '0%d' % (i + 1), TIME)
            d.rectangle([x + 4, 153, x + 25, 155], fill=GRID); d.rectangle([x + 4, 153, x + 4 + int(21 * p / 100), 155], fill=ACC)
    else:
        d.rectangle([0, 26, 143, 141], fill=K); d.line([0, 26, 143, 26], fill=ACC); d.line([0, 141, 143, 141], fill=ACC)
        big(im, '10', 60, TIME, 56.5); big(im, '09', 60, TIME, 111.5)
        w = M(17).getlength('TUE 17'); d.rectangle([72 - w / 2 - 4, 145, 72 + w / 2 + 4, 164], fill=K)
        big(im, 'TUE 17', 17, DATE, 155)
    return q(im)
Sc, pad, lab = 4, 24, 30
panels = [('A', 73, 'A  MDR'), ('B', 73, 'B  ultra legible'), ('B', 100, 'B  battery 100%'), ('A', 12, 'A  battery 12% (low)')]
sheet = Image.new('RGB', (pad + 4 * (W * Sc + pad), pad + H * Sc + lab + pad), (13, 17, 20)); sd = ImageDraw.Draw(sheet)
for i, (v, b, n) in enumerate(panels):
    x = pad + i * (W * Sc + pad); sheet.paste(screen(v, b).resize((W * Sc, H * Sc), Image.NEAREST), (x, pad))
    sd.text((x, pad + H * Sc + 10), n, font=M(14), fill=(201, 214, 219))
sheet.save(OUT)
