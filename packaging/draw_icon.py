#!/usr/bin/env python3
"""Draw the app icon the way the game draws everything else: as a BBC Micro in
MODE 2 would have drawn it, with no external asset and no image library.

Eight colours at full strength and fat pixels twice as wide as they are tall,
exactly as in the game (see docs/art-and-audio.md). The art is authored on a grid
of 64 by 128 BBC pixels and blown up to the 1024x1024 master Apple asks for, so a
BBC pixel is sixteen pixels across and eight down — the proportions of every
pixel the player will see once the icon is clicked.

Chuck on the flank of the tower is not drawn here: he is read out of the cast's
own sprite in src/bbc_cast.c (`SPR_CHUCK_CLIMB`), because an icon with its own
Chuck is a second drawing of the one figure the game insists is drawn once.

Only the rounded-square plate is drawn at full resolution, as a distance field,
and only in alpha: it is the shape the operating system cuts every icon to, not
part of the picture, and a stair-stepped one reads as a mistake. Every opaque
pixel is still one of the eight colours.

Usage: draw_icon.py <out.png>
"""

import re
import struct
import sys
import zlib
from pathlib import Path

COLS, ROWS = 64, 128   # the authoring grid, in BBC pixels
CELL_W, CELL_H = 16, 8  # one BBC pixel on the 1024 master
OUT = COLS * CELL_W
assert ROWS * CELL_H == OUT

ROOT = Path(__file__).resolve().parent.parent

# The eight, numbered as the hardware numbers them — the same table as BBC_RGB
# in src/bbc_art.c.
K, R, G, Y, B, M, C, W = range(8)
RGB = [(0, 0, 0), (255, 0, 0), (0, 255, 0), (255, 255, 0),
       (0, 0, 255), (255, 0, 255), (0, 255, 255), (255, 255, 255)]
SPRITE_CODES = {"K": K, "R": R, "G": G, "Y": Y, "B": B, "M": M, "C": C, "W": W}


class Grid:
    def __init__(self):
        self.cells = [[K] * COLS for _ in range(ROWS)]

    def put(self, x, y, c):
        if 0 <= x < COLS and 0 <= y < ROWS:
            self.cells[y][x] = c

    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.put(xx, yy, c)

    def check(self, x, y, w, h, a, b):
        """Two colours on a checkerboard: the BBC's mid-tone."""
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.put(xx, yy, a if (xx + yy) % 2 == 0 else b)

    def disc(self, cx, cy, radius, c):
        """Round on the glass, so twice as many rows as columns."""
        for yy in range(int(cy - radius * 2) - 1, int(cy + radius * 2) + 2):
            for xx in range(int(cx - radius) - 1, int(cx + radius) + 2):
                dx = xx + 0.5 - cx
                dy = (yy + 0.5 - cy) / 2.0
                if dx * dx + dy * dy <= radius * radius:
                    self.put(xx, yy, c)

    def sprite(self, rows, x, y, scale=1, mirror=False):
        for ry, row in enumerate(rows):
            for rx, ch in enumerate(reversed(row) if mirror else row):
                if ch in SPRITE_CODES:
                    self.rect(x + rx * scale, y + ry * scale, scale, scale,
                              SPRITE_CODES[ch])


def cast_sprite(name):
    """One sprite out of src/bbc_cast.c, rows as written."""
    source = (ROOT / "src" / "bbc_cast.c").read_text(encoding="utf-8")
    m = re.search(r"BBC_SPRITE_EXTERN\(\s*" + re.escape(name) + r"\s*,(.*?)\);",
                  source, re.S)
    if m is None:
        sys.exit(f"draw_icon.py: no {name} in src/bbc_cast.c")
    return re.findall(r'"([^"]*)"', m.group(1))


class Rng:
    def __init__(self, seed):
        self.state = seed & 0xFFFFFFFF

    def next(self):
        self.state = (self.state * 1103515245 + 12345) & 0x7FFFFFFF
        return self.state >> 8


def draw_art():
    g = Grid()
    rng = Rng(0x0C4C0B)

    # Stars: single pixels, a few of them cyan, none of them near the moon.
    for _ in range(70):
        x, y = rng.next() % COLS, rng.next() % 70
        if (x - 44) ** 2 + ((y - 30) / 2) ** 2 < 60:
            continue
        g.put(x, y, C if rng.next() % 5 == 0 else W)

    # The moon: white, a cyan limb on the side away from the light.
    g.disc(44, 30, 4.0, W)
    for yy in range(20, 40):
        for xx in range(39, 50):
            dx, dy = xx + 0.5 - 44, (yy + 0.5 - 30) / 2.0
            if dx * dx + dy * dy <= 16 and dx + dy * 0.6 > 2.0:
                g.put(xx, yy, C)
    g.put(43, 28, C)
    g.put(42, 31, C)

    # The city behind: blue blocks on black with a scatter of lit windows,
    # yellow and the odd red one.
    x = 0
    while x < COLS:
        w = 4 + rng.next() % 6
        top = 82 + rng.next() % 18
        g.check(x, top, w, ROWS - top, B, K)
        for wy in range(top + 3, ROWS - 10, 4):
            for wx in range(x + 1, x + w - 1, 2):
                if rng.next() % 4 == 0:
                    g.put(wx, wy, Y if rng.next() % 6 else R)
        x += w + 1

    # The tower: solid blue, a cyan arris on the moon side, a storey band every
    # eight rows — the module that says how big the building is.
    x0, x1, roof = 14, 34, 40
    g.rect(x0, roof, x1 - x0, ROWS - roof, B)
    g.rect(x0, roof, 1, ROWS - roof, C)
    g.rect(x1 - 1, roof, 1, ROWS - roof, K)
    for y in range(roof + 8, ROWS, 8):
        g.rect(x0, y, x1 - x0 - 1, 1, C)

    # Windows: black panes, a few lit, and one floor near the top lit right
    # across in yellow — the floor the night is about.
    story = 0
    for y in range(roof + 2, ROWS - 6, 8):
        story += 1
        for wx in range(x0 + 2, x1 - 2, 4):
            lit = story == 3 or rng.next() % 7 == 0
            g.rect(wx, y + 1, 2, 4, Y if lit else K)
        if story == 3:
            # Two figures against the light: somebody up there is not alone.
            g.rect(x0 + 7, y + 2, 1, 3, K)
            g.rect(x0 + 11, y + 2, 1, 3, K)

    # The roof: a parapet, the plant and the mast with its red light.
    g.rect(x0 - 1, roof - 2, x1 - x0 + 2, 2, W)
    g.rect(x0 + 4, roof - 6, 6, 4, B)
    g.rect(x0 + 4, roof - 6, 6, 1, C)
    g.rect(x0 + 13, roof - 18, 1, 16, W)
    g.rect(x0 + 12, roof - 20, 3, 2, R)

    # The street and its kerb.
    g.rect(0, ROWS - 6, COLS, 6, K)
    g.rect(0, ROWS - 6, COLS, 1, W)
    g.check(0, ROWS - 5, COLS, 1, B, K)

    # Chuck on the flank, going up, at the size he is in the game: the icon is
    # one screen of it.
    chuck = cast_sprite("SPR_CHUCK_CLIMB")
    g.sprite(chuck, x1 - 1, 62)

    return g


def rounded_rect_alpha(x, y, inset, radius):
    """Signed-distance coverage for Apple's icon grid: an 824pt rounded square
    in a 1024pt canvas, antialiased at the master resolution."""
    lo = inset
    hi = OUT - inset
    cx = min(max(x, lo + radius), hi - radius)
    cy = min(max(y, lo + radius), hi - radius)
    d = (((x - cx) ** 2 + (y - cy) ** 2) ** 0.5) - radius
    if x < lo or x > hi or y < lo or y > hi:
        d = max(d, 1.0)
    return max(0.0, min(1.0, 0.5 - d))


def write_png(path, w, h, rgba):
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    head = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    blob = (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", head)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(blob)


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: draw_icon.py <out.png>")

    art = draw_art()
    inset = 100.0                 # (1024 - 824) / 2
    radius = 185.4
    frame = 24                    # the plate's blue rule, inside its edge

    out = bytearray(OUT * OUT * 4)
    for y in range(OUT):
        row = art.cells[y // CELL_H]
        for x in range(OUT):
            a = rounded_rect_alpha(x + 0.5, y + 0.5, inset, radius)
            if a <= 0.0:
                continue
            c = row[x // CELL_W]
            # A one-pixel blue rule just inside the plate's edge, the way a BBC
            # screen framed itself: snapped to whole BBC pixels like the rest.
            gx, gy = (x // CELL_W) * CELL_W, (y // CELL_H) * CELL_H
            inner = rounded_rect_alpha(gx + CELL_W / 2, gy + CELL_H / 2,
                                       inset + frame, radius - frame)
            if 0.0 < rounded_rect_alpha(gx + CELL_W / 2, gy + CELL_H / 2,
                                        inset + frame - CELL_W, radius - frame
                                        + CELL_W) and inner <= 0.0:
                c = B
            i = (y * OUT + x) * 4
            out[i], out[i + 1], out[i + 2] = RGB[c]
            out[i + 3] = int(round(a * 255))

    write_png(sys.argv[1], OUT, OUT, out)


if __name__ == "__main__":
    main()
