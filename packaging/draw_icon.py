#!/usr/bin/env python3
"""Draw the app icon the way the game draws everything else: procedurally, out
of the fx.h palette, with no external asset and no image library.

The art is authored on a 128x128 pixel grid — the game is pixel art and an icon
that pretends otherwise would be a different game's icon — and blown up to the
1024x1024 master Apple asks for. Only the rounded-square plate is drawn at the
full resolution, as a distance field, because that edge is the one curve in the
picture and a stair-stepped one reads as a mistake rather than as a style.

Usage: draw_icon.py <out.png>
"""

import struct
import sys
import zlib

GRID = 128           # the authoring grid
SCALE = 8            # 128 * 8 = the 1024 master
OUT = GRID * SCALE

# fx.h, so the icon is lit by the same palette as the game.
INK = (5, 7, 12)
NIGHT = (10, 14, 23)
SHADOW = (17, 23, 35)
BASE = (27, 35, 49)
MID = (41, 52, 68)
STEEL_DK = (49, 60, 74)
STEEL = (70, 84, 99)
STEEL_LT = (104, 121, 137)
PALE = (156, 173, 186)
CREAM = (236, 238, 224)
AMBER = (248, 188, 74)
RED = (232, 74, 62)
LAMP = (150, 206, 214)
WARM = (240, 190, 112)
SODIUM = (182, 116, 62)
HERO = (40, 108, 148)
HERO_LT = (70, 156, 180)
SKIN = (216, 160, 110)


def lerp(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


class Canvas:
    def __init__(self, w, h, fill):
        self.w = w
        self.h = h
        self.px = [list(fill) for _ in range(w * h)]

    def put(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = list(c)

    def get(self, x, y):
        return self.px[y * self.w + x]

    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.put(xx, yy, c)

    def blend(self, x, y, c, a):
        if not (0 <= x < self.w and 0 <= y < self.h):
            return
        p = self.px[y * self.w + x]
        for i in range(3):
            p[i] = int(round(p[i] + (c[i] - p[i]) * a))

    def glow(self, cx, cy, radius, c, strength):
        r = int(radius) + 1
        for yy in range(cy - r, cy + r + 1):
            for xx in range(cx - r, cx + r + 1):
                d = ((xx - cx) ** 2 + (yy - cy) ** 2) ** 0.5
                if d > radius:
                    continue
                self.blend(xx, yy, c, strength * (1.0 - d / radius) ** 2)


class Rng:
    """Fixed seed: the icon must come out the same on every machine."""

    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF

    def next(self):
        self.s = (self.s * 1664525 + 1013904223) & 0xFFFFFFFF
        return self.s

    def chance(self, pct):
        return self.next() % 100 < pct


def draw_art():
    c = Canvas(GRID, GRID, NIGHT)
    rng = Rng(0x0C4C0B)

    # Sky: night at the top, the city's own glow warming the bottom.
    for y in range(GRID):
        t = y / (GRID - 1.0)
        row = lerp(NIGHT, (30, 30, 42), t * t)
        c.rect(0, y, GRID, 1, row)

    # The moon, and the haze around it. It is a disc lit from the same side as
    # everything else, not a pale blob: the terminator runs across it rather
    # than around one corner.
    c.glow(100, 22, 26, (60, 74, 96), 0.55)
    for y in range(-6, 7):
        for x in range(-6, 7):
            d = (x * x + y * y) ** 0.5
            if d > 5.6:
                continue
            face = lerp(CREAM, STEEL_LT, min(1.0, max(0.0, (x + y * 0.5 + 3) / 8.0)))
            c.blend(100 + x, 22 + y, face, min(1.0, (5.6 - d) * 1.6))

    # Two skylines behind the tower, each a step lighter than the one behind it,
    # so the picture has depth before anything is drawn in front of it.
    for layer, (colour, top, span) in enumerate(
        ((lerp(NIGHT, SHADOW, 0.6), 78, 9), (SHADOW, 88, 13))
    ):
        x = -2
        while x < GRID + 4:
            w = 6 + rng.next() % 12
            h = span + rng.next() % span
            c.rect(x, top + (span - h // 2), w, GRID - top, colour)
            for wy in range(top + span - h // 2 + 3, GRID - 6, 5):
                for wx in range(x + 2, x + w - 2, 4):
                    if rng.chance(22 + layer * 10):
                        c.rect(wx, wy, 1, 2, lerp(colour, SODIUM, 0.55))
            x += w + 1 + rng.next() % 3

    # The tower. Left flank lit by the moon, right flank falling into shade —
    # the same three passes a wall tile gets in level_art.c, at icon scale.
    x0, x1, roof = 46, 82, 26
    c.rect(x0, roof, x1 - x0, GRID - roof, BASE)
    c.rect(x0, roof, 4, GRID - roof, STEEL_DK)
    c.rect(x0, roof, 1, GRID - roof, STEEL)
    c.rect(x1 - 5, roof, 5, GRID - roof, SHADOW)
    c.rect(x1 - 1, roof, 1, GRID - roof, INK)

    # A storey band every eight rows: the module that says how big the building
    # is. Without it the window grid is wallpaper.
    for y in range(roof + 8, GRID, 8):
        c.rect(x0, y, x1 - x0 - 1, 1, lerp(BASE, MID, 0.8))
        c.rect(x0, y + 1, x1 - x0 - 1, 1, lerp(BASE, INK, 0.35))

    # Windows: mostly dark, a handful alight. One floor near the top is lit
    # right across — that is the floor the night is about.
    story = 0
    for y in range(roof + 3, GRID - 6, 8):
        story += 1
        for x in range(x0 + 6, x1 - 7, 6):
            if story == 3:
                tint = WARM
            elif rng.chance(34):
                tint = AMBER if rng.chance(70) else LAMP
            else:
                c.rect(x, y, 4, 4, lerp(SHADOW, NIGHT, 0.4))
                c.rect(x, y, 4, 1, lerp(SHADOW, INK, 0.5))
                continue
            c.rect(x, y, 4, 4, tint)
            c.rect(x, y + 3, 4, 1, lerp(tint, SODIUM, 0.55))
            c.glow(x + 2, y + 2, 5, tint, 0.16)

    # Roof slab, mast and the beacon. The helicopter comes for this roof at
    # 01:00, and the red light is the only thing in the frame that says so.
    c.rect(x0 - 2, roof - 2, x1 - x0 + 4, 3, MID)
    c.rect(x0 - 2, roof - 2, x1 - x0 + 4, 1, STEEL_LT)
    c.rect(63, roof - 10, 2, 8, STEEL_DK)
    c.glow(64, roof - 12, 9, RED, 0.7)
    c.rect(63, roof - 13, 3, 3, RED)
    c.rect(64, roof - 12, 1, 1, CREAM)

    # Chuck, on the flank, four hundred feet up. Legs a long way under the
    # jacket, the way the whole cast is drawn.
    fx, fy = 41, 74
    parts = [
        (fx + 1, fy + 5, 2, 5, lerp(HERO, INK, 0.65)),        # legs
        (fx + 3, fy + 8, 2, 2, lerp(HERO, INK, 0.65)),        # boot
        (fx, fy + 2, 5, 5, HERO),                             # jacket
        (fx, fy + 2, 1, 5, HERO_LT),                          # lit flank
        (fx + 1, fy, 3, 3, SKIN),                             # head
        (fx + 1, fy, 3, 1, lerp(SKIN, INK, 0.6)),             # hair
        (fx + 4, fy + 1, 2, 1, SKIN),                         # reaching arm
        (fx + 4, fy + 6, 2, 1, SKIN),                         # holding arm
    ]
    # Outline first, part by part rather than as one box: the flank he is
    # climbing sits at his own value, and a figure with no outline against it
    # is a smudge. A box would be a shadow he is not casting.
    for x, y, w, h, _ in parts:
        c.rect(x - 1, y - 1, w + 2, h + 2, INK)
    for x, y, w, h, colour in parts:
        c.rect(x, y, w, h, colour)
    c.glow(fx + 2, fy + 4, 12, LAMP, 0.10)

    # The street the tower stands in, and the sodium pool on it.
    c.rect(0, GRID - 7, GRID, 7, lerp(NIGHT, INK, 0.5))
    c.rect(0, GRID - 7, GRID, 1, lerp(STEEL_DK, SODIUM, 0.35))
    c.glow(64, GRID - 4, 40, SODIUM, 0.30)

    return c


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
    edge = 6                      # the lit top edge / dark base of the plate

    out = bytearray(OUT * OUT * 4)
    for y in range(OUT):
        gy = y // SCALE
        for x in range(OUT):
            a = rounded_rect_alpha(x + 0.5, y + 0.5, inset, radius)
            i = (y * OUT + x) * 4
            if a <= 0.0:
                continue
            r, g, b = art.get(x // SCALE, gy)
            # The plate is a thing in the light, not a sticker: its top edge
            # catches the same moon the tower does and its base sits in shade.
            depth = rounded_rect_alpha(x + 0.5, y + 0.5 - edge, inset, radius)
            if depth < a:
                r, g, b = lerp((r, g, b), CREAM, 0.30 * (a - depth))
            rise = rounded_rect_alpha(x + 0.5, y + 0.5 + edge, inset, radius)
            if rise < a:
                r, g, b = lerp((r, g, b), INK, 0.55 * (a - rise))
            out[i] = r
            out[i + 1] = g
            out[i + 2] = b
            out[i + 3] = int(round(a * 255))

    write_png(sys.argv[1], OUT, OUT, out)


if __name__ == "__main__":
    main()
