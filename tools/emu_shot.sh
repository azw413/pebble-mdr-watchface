#!/usr/bin/env bash
# Install the built .pbw on an emulator, pin it to the reference state, screenshot.
# Usage: tools/emu_shot.sh <platform> <out.png> [HH:MM:SS] [battery%]
# Defaults match docs/reference/pebble_time_pixels_v3.png: 10:09, 73%.
set -euo pipefail
plat=$1 out=$2 t=${3:-10:09:00} batt=${4:-73}
quiet() { grep -vE "new (SDK|pebble-tool)|Update with|^\s*$" || true; }
pebble install --emulator "$plat" 2>&1 | quiet
pebble emu-set-time --emulator "$plat" "$t" 2>&1 | quiet
pebble emu-battery --emulator "$plat" --percent "$batt" 2>&1 | quiet
sleep 2
pebble screenshot --emulator "$plat" --no-open "$out" 2>&1 | quiet | tail -1
