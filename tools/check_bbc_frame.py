"""Hold a captured frame to the eight colours a BBC Micro could put on the glass.

Every picture in this game is drawn as MODE 2 would draw it — see
docs/art-and-audio.md — and the rule that makes that true is not a convention a
renderer can be trusted to remember: one `SDL_BLENDMODE_BLEND` left in a corner
of a cutscene, one gradient nobody converted, and the frame is a modern frame
with an old palette on most of it. Reading the source cannot settle that,
because a colour that is not one of the eight is made at run time, by blending
two that are. So this reads the *output*: a BMP written by `--shot`, every
pixel of it, and fails on the first colour that is not a corner of the RGB
cube.

It reports a second figure and never fails on it: how many 4x2 cells of the
frame are not one colour. Text is drawn at one logical pixel to a font pixel on
purpose and is most of that number; the rest is a renderer drawing off the
grid, which is worth knowing about and is an art decision rather than a defect
this script can tell apart from a letter.

A region can be named with `--region X,Y,W,H` (logical pixels) to check only
that part of each frame.

`--sweep` checks only the palette, as fast as it can, and prints one line for a
whole set of frames; it is what tools/soak.sh runs.

Usage: check_bbc_frame.py [--region X,Y,W,H | --sweep] FRAME.bmp [FRAME.bmp ...]
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

PALETTE = {
    (r, g, b)
    for r in (0, 255)
    for g in (0, 255)
    for b in (0, 255)
}

CELL_W = 4
CELL_H = 2


def read_bmp(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{path}: not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    top_down = height < 0
    height = abs(height)
    if bpp not in (24, 32):
        raise ValueError(f"{path}: {bpp} bits per pixel is not supported")

    masks = (0x00FF0000, 0x0000FF00, 0x000000FF)
    if compression == 3:
        masks = struct.unpack_from("<III", data, 14 + 40)
    elif compression != 0:
        raise ValueError(f"{path}: compression {compression} is not supported")

    def channel(value: int, mask: int) -> int:
        if mask == 0:
            return 0
        shift = (mask & -mask).bit_length() - 1
        top = mask >> shift
        return ((value & mask) >> shift) * 255 // top

    stride = ((width * bpp + 31) // 32) * 4
    pixels: list[tuple[int, int, int]] = []
    for row in range(height):
        src_row = row if top_down else height - 1 - row
        base = offset + src_row * stride
        for col in range(width):
            if bpp == 24:
                b, g, r = data[base + col * 3: base + col * 3 + 3]
                pixels.append((r, g, b))
            else:
                (value,) = struct.unpack_from("<I", data, base + col * 4)
                pixels.append(
                    (
                        channel(value, masks[0]),
                        channel(value, masks[1]),
                        channel(value, masks[2]),
                    )
                )
    return width, height, pixels


def palette_only(path: Path) -> bool | None:
    """The fast half, for a sweep: a 32-bit frame is clean exactly when every
    colour byte is 0 or 255, which `bytes.translate` answers without a Python
    loop over pixels. None when the file is not the shape this shortcut reads,
    and the full check has to run instead."""
    data = path.read_bytes()
    if data[:2] != b"BM":
        return None
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    table = bytes(0 if b in (0, 255) else 1 for b in range(256))
    if bpp == 24:
        # No alpha to skip, and a row's padding is zeros, which are clean.
        stride = ((width * 24 + 31) // 32) * 4
        return not any(data[offset: offset + stride * abs(height)].translate(table))
    if bpp != 32:
        return None
    body = data[offset: offset + width * abs(height) * 4]
    flagged = body.translate(table)
    masks = (0x00FF0000, 0x0000FF00, 0x000000FF)
    if struct.unpack_from("<I", data, 30)[0] == 3:
        masks = struct.unpack_from("<III", data, 14 + 40)
    lanes = [((m & -m).bit_length() - 1) // 8 for m in masks]
    return not any(any(flagged[lane::4]) for lane in lanes)


def check(path: Path, region: tuple[int, int, int, int] | None = None) -> bool:
    width, height, pixels = read_bmp(path)
    rx, ry, rw, rh = region if region else (0, 0, width, height)
    rx0, ry0 = max(rx, 0), max(ry, 0)
    rx1, ry1 = min(rx + rw, width), min(ry + rh, height)
    off = [
        (x, y, pixels[y * width + x])
        for y in range(ry0, ry1)
        for x in range(rx0, rx1)
        if pixels[y * width + x] not in PALETTE
    ]

    mixed = 0
    cells = 0
    for cy in range(ry0 - ry0 % CELL_H, ry1 - CELL_H + 1, CELL_H):
        for cx in range(rx0 - rx0 % CELL_W, rx1 - CELL_W + 1, CELL_W):
            cells += 1
            first = pixels[cy * width + cx]
            if any(
                pixels[(cy + dy) * width + cx + dx] != first
                for dy in range(CELL_H)
                for dx in range(CELL_W)
            ):
                mixed += 1

    name = path.name + (f" [{rx},{ry},{rw},{rh}]" if region else "")
    if off:
        colours: dict[tuple[int, int, int], int] = {}
        for _, _, p in off:
            colours[p] = colours.get(p, 0) + 1
        worst = sorted(colours.items(), key=lambda kv: -kv[1])[:6]
        x, y, p = off[0]
        print(
            f"{name}: {len(off)} pixel(s) outside the BBC palette, "
            f"first at ({x}, {y}) = {p}; "
            f"commonest {', '.join(f'{c}x{n}' for c, n in worst)}"
        )
        return False
    print(
        f"{name}: {width}x{height}, eight colours only; "
        f"{mixed} of {cells} cells off the 4x2 grid ({100.0 * mixed / max(cells, 1):.1f}%)"
    )
    return True


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__.strip().splitlines()[-1])
        return 2
    ok = True
    region = None
    args = argv[1:]
    if args and args[0] == "--sweep":
        # Every file named, palette only, one line for the lot unless one fails
        # — which then gets the full report, because the full report is what
        # says where to look.
        clean = 0
        for arg in args[1:]:
            fast = palette_only(Path(arg))
            if fast is True:
                clean += 1
            elif not check(Path(arg)):
                ok = False
            else:
                clean += 1
        print(f"bbc: {clean} of {len(args) - 1} frame(s) in the eight colours")
        return 0 if ok else 1
    if len(args) >= 2 and args[0] == "--region":
        try:
            region = tuple(int(v) for v in args[1].split(","))
            if len(region) != 4:
                raise ValueError
        except ValueError:
            print("--region expects X,Y,W,H")
            return 2
        args = args[2:]
    for arg in args:
        try:
            ok = check(Path(arg), region) and ok
        except (OSError, ValueError, struct.error) as exc:
            print(f"{arg}: {exc}")
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
