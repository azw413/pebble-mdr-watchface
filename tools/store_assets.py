#!/usr/bin/env python3
"""Make app-store screenshots and the animation GIF from the emulators.

Usage (from the project root, after `npm install`):
    python3 tools/store_assets.py screenshots [PLATFORM ...]
    python3 tools/store_assets.py gif [PLATFORM ...] [--step MS]

screenshots: Standard and Large at 10:09, 73% battery, default cyan theme.
gif:         the Standard refinement run (10:10 -> 10:15), captured frame by
             frame from an MDR_CAPTURE build (each emulator tap steps the run by
             --step design ms) and played back at the watch's real speed.

Output goes to store/<platform>/ at the watch's native resolution. pebble-tool
screenshots carry an LCD colour profile; every image is snapped back to the
Pebble 64-colour palette, so the assets show the app's true colours.
Needs Pillow. Rebuilds the app; run a plain `pebble build` afterwards.
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORE = os.path.join(ROOT, 'store')
PLATFORMS = ['basalt', 'emery', 'gabbro']
LEVELS = (0, 85, 170, 255)
ANIM_DESIGN_MS = 2600   # src/c/anim.h ANIM_DURATION_MS
ANIM_SLOWDOWN = 2       # src/c/anim.h ANIM_SLOWDOWN


def run(cmd, env=None, timeout=240):
    r = subprocess.run(cmd, cwd=ROOT, env=env, timeout=timeout,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        sys.exit('failed: %s\n%s' % (' '.join(cmd), r.stdout[-2000:]))
    return r.stdout


def build(**flags):
    env = {k: v for k, v in os.environ.items() if not k.startswith('MDR_')}
    env.update(flags)
    run(['pebble', 'build'], env=env, timeout=600)


def emu(plat, *args, timeout=120):
    return run(['pebble'] + list(args) + ['--emulator', plat], timeout=timeout)


def snap(path):
    """Load a screenshot and snap every channel to the 64-colour palette levels."""
    im = Image.open(path).convert('RGB')
    return im.point(lambda v: min(LEVELS, key=lambda l: abs(l - v)))


def screenshot(plat, tmpdir, name):
    path = os.path.join(tmpdir, name + '.png')
    emu(plat, 'screenshot', '--no-open', path)
    return snap(path)


def install(plat, clock, battery=73):
    emu(plat, 'install', timeout=240)
    emu(plat, 'emu-set-time', clock)
    emu(plat, 'emu-battery', '--percent', str(battery))
    time.sleep(2)


def screenshots(platforms):
    with tempfile.TemporaryDirectory() as tmp:
        for name, layout in (('standard', 'A'), ('large', 'B')):
            build(MDR_LAYOUT=layout)
            for plat in platforms:
                install(plat, '10:09:00')
                out = os.path.join(STORE, plat, name + '.png')
                os.makedirs(os.path.dirname(out), exist_ok=True)
                screenshot(plat, tmp, '%s_%s' % (plat, name)).save(out)
                print(os.path.relpath(out, ROOT))


def palette_image():
    """A P-mode image holding the 64 Pebble colours, for exact GIF quantising."""
    pal = []
    for r in LEVELS:
        for g in LEVELS:
            for b in LEVELS:
                pal += [r, g, b]
    img = Image.new('P', (1, 1))
    img.putpalette(pal + [0] * (768 - len(pal)))
    return img


def gif(platforms, step):
    steps = -(-ANIM_DESIGN_MS // step)  # ceil: the last tap finishes the run
    frame_ms = step * ANIM_SLOWDOWN     # real playback time per frame
    build(MDR_CAPTURE=str(step))
    pal = palette_image()
    with tempfile.TemporaryDirectory() as tmp:
        for plat in platforms:
            # The clock reads 10:15 once the run ends, so no real refinement starts.
            install(plat, '10:15:10')
            frames = [screenshot(plat, tmp, '%s_000' % plat)]
            for i in range(1, steps + 1):
                emu(plat, 'emu-tap')
                time.sleep(0.4)
                frames.append(screenshot(plat, tmp, '%s_%03d' % (plat, i)))
                print('\r%s: frame %d/%d' % (plat, i, steps), end='', flush=True)
            print()
            # Hold the old time before the run and the new one after it.
            durations = [1200] + [frame_ms] * (len(frames) - 2) + [2000]
            q = [f.quantize(palette=pal, dither=Image.Dither.NONE)
                 if hasattr(Image, 'Dither') else f.quantize(palette=pal, dither=0)
                 for f in frames]
            out = os.path.join(STORE, plat, 'standard-refinement.gif')
            os.makedirs(os.path.dirname(out), exist_ok=True)
            q[0].save(out, save_all=True, append_images=q[1:], duration=durations,
                      loop=0, optimize=False, disposal=1)
            print('%s: %d frames, %d KB' % (os.path.relpath(out, ROOT), len(q),
                                            os.path.getsize(out) // 1024))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('what', choices=['screenshots', 'gif'])
    ap.add_argument('platforms', nargs='*', default=PLATFORMS)
    ap.add_argument('--step', type=int, default=40, help='design ms per GIF frame')
    a = ap.parse_args()
    bad = [p for p in a.platforms if p not in PLATFORMS]
    if bad:
        sys.exit('unknown platform: %s' % ', '.join(bad))
    if a.what == 'screenshots':
        screenshots(a.platforms)
    else:
        gif(a.platforms, a.step)


if __name__ == '__main__':
    main()
