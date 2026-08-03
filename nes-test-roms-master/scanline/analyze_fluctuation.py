#!/usr/bin/env python3
"""Ostatni bialy piksel w wierszu per klatka, dla 3 stref scanline_white.nes.

Format: strefa y=N: x0 x1 x2 x3 x4 x5 x6 x7
Uzycie: python analyze_fluctuation.py [--no-build]
"""

import argparse
import subprocess
import sys
import os
from collections import Counter

BASE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILD = os.path.join(BASE, "build", "release")
NES_TEST = os.path.join(BUILD, "nes_test.exe")
ROM = os.path.join(BASE, "nes-test-roms-master", "scanline", "scanline_white.nes")

WIDTH = 256
HEIGHT = 240

ZONES = [
    ("test1_$2001_D3", 48, 95),
    ("test2_$2000_D4", 120, 167),
    ("test3_$2005_$2006", 192, 223),
]


def build():
    r = subprocess.run(["ninja", "-C", BUILD], capture_output=True, text=True)
    if r.returncode != 0:
        print("BUILD FAILED:", r.stderr[-500:])
        sys.exit(2)


def parse_pixels(text):
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("----") or line.startswith("[nes_test]"):
            continue
        pixels = line.split()
        if len(pixels) >= WIDTH:
            rows.append(pixels[:WIDTH])
    return rows


def run_capture(rom_path):
    args = [NES_TEST, rom_path, "frames:60"]
    for _ in range(8):
        args.append("frames:1")
        args.append(f"pixels:0:0:{WIDTH}:{HEIGHT}")

    p = subprocess.run(args, capture_output=True, text=True, timeout=120)
    out = p.stdout + p.stderr
    frames_raw = []
    current = []
    in_pixels = False

    for line in out.splitlines():
        if line.startswith("---- pixels"):
            if current:
                frames_raw.append(current)
                current = []
            in_pixels = True
            continue
        if in_pixels:
            stripped = line.strip()
            if stripped and not stripped.startswith("[nes_test]"):
                current.append(stripped)
            elif not stripped and current:
                frames_raw.append(current)
                current = []
                in_pixels = False
    if current:
        frames_raw.append(current)

    return [parse_pixels("\n".join(f)) for f in frames_raw if f]


def find_bg(frames, y0, y1):
    c = Counter()
    for f in frames:
        for y in range(y0, min(y1 + 1, len(f))):
            for x in range(WIDTH):
                c[f[y][x]] += 1
    return c.most_common(1)[0][0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true")
    args = ap.parse_args()

    if not args.no_build:
        build()

    if not os.path.exists(ROM):
        print(f"ROM nie istnieje: {ROM}")
        sys.exit(1)

    frames = run_capture(ROM)
    n_frames = len(frames)
    h = len(frames[0])

    print("kazda liczba = X ostatniego bialego piksela w jednej klatce (0..7)")
    print()

    for zname, y_start, y_end in ZONES:
        bg = find_bg(frames, y_start, y_end)
        mid = (y_start + min(y_end, h - 1)) // 2
        y0 = max(y_start, mid - 4)
        ys = list(range(y0, min(y0 + 8, h)))

        print(f"{zname}:")
        for y in ys:
            xs = []
            for f in range(n_frames):
                row = frames[f][y]
                for x in range(WIDTH - 1, -1, -1):
                    if row[x] != bg:
                        xs.append(x)
                        break
            print(f"  y={y}: " + " ".join(str(x) for x in xs))

    print("DONE")


if __name__ == "__main__":
    main()
