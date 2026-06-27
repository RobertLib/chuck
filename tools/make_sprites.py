#!/usr/bin/env python3
"""
make_sprites.py – generate blank placeholder BMP sprite sheets for Chuck.

Run from the project root:
    python3 tools/make_sprites.py

This creates the assets/ directory and writes four BMP files filled with a
neutral grey so you can open them in any pixel editor and draw your sprites.
If a file already exists it is NOT overwritten (pass --force to regenerate).

=== Sprite sheet layout ===

assets/tileset.bmp  (192 × 38 px)
  Row 0  y=0,  h=32  –  six 32×32 level tiles (left to right):
    col 0  WALL
    col 1  LADDER
    col 2  DOOR
    col 3  ELEVATOR SHAFT
    col 4  EXIT  inactive  (items still to collect)
    col 5  EXIT  active    (all items collected)
  Row 1  y=32, h=6   –  three 32×6 platform sprites:
    col 0  ELEVATOR moving platform
    col 1  FALLING platform
    col 2  MOVING (horizontal) platform

assets/player.bmp   (64 × 32 px)
  Two 32×32 cells:
    col 0  standing  – draw your sprite within the first 22×28 px
    col 1  crawling  – draw your sprite within the first 22×16 px
  The game flips the sprite horizontally for left-facing direction.

assets/enemy.bmp    (32 × 32 px)
  One 32×32 cell – draw within the first 22×26 px.
  The game flips the sprite horizontally when the enemy faces left.

assets/items.bmp   (160 × 32 px)
  Five 32×32 cells (left to right):
    col 0  CARD key        – draw within 14×18 px
    col 1  GUN / ammo pack – draw within 15×12 px
    col 2  GRENADE pickup  – draw within 13×14 px
    col 3  MINE            – draw within 16×10 px
    col 4  thrown GRENADE  – draw within 10×10 px

All sprite areas start at the top-left corner of their cell.
Pixels outside the stated area are ignored by the renderer.
"""

import os
import struct
import sys


def _write_bmp_buffer(path, width, height, buf, force=False):
  """Write a 24-bit uncompressed top-down BMP using a prefilled BGR buffer."""
  if os.path.exists(path) and not force:
    print(f"  skip  {path}  (already exists; use --force to regenerate)")
    return

  row_bytes = width * 3
  pad = (4 - row_bytes % 4) % 4
  stride = row_bytes + pad
  px_size = stride * height

  os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
  with open(path, "wb") as f:
    f.write(b"BM")
    f.write(struct.pack("<I", 54 + px_size))
    f.write(struct.pack("<HH", 0, 0))
    f.write(struct.pack("<I", 54))
    f.write(struct.pack("<I", 40))
    f.write(struct.pack("<i", width))
    f.write(struct.pack("<i", -height))
    f.write(struct.pack("<H", 1))
    f.write(struct.pack("<H", 24))
    f.write(struct.pack("<I", 0))
    f.write(struct.pack("<I", px_size))
    f.write(struct.pack("<ii", 2835, 2835))
    f.write(struct.pack("<II", 0, 0))
    f.write(buf)

  print(f"  wrote {path}  ({width}×{height})")


def _create_buffer(width, height, fill=(180, 180, 180)):
  r, g, b = fill
  row_bytes = width * 3
  pad = (4 - row_bytes % 4) % 4
  stride = row_bytes + pad
  buf = bytearray(stride * height)
  for y in range(height):
    off = y * stride
    for x in range(width):
      i = off + x * 3
      buf[i + 0] = b
      buf[i + 1] = g
      buf[i + 2] = r
  return buf


def _set_pixel(buf, width, height, x, y, color):
  row_bytes = width * 3
  pad = (4 - row_bytes % 4) % 4
  stride = row_bytes + pad
  if x < 0 or x >= width or y < 0 or y >= height:
    return
  off = y * stride + x * 3
  buf[off + 0] = color[2]
  buf[off + 1] = color[1]
  buf[off + 2] = color[0]


def _draw_rect_outline(buf, width, height, x, y, w, h, color):
  for ix in range(x, x + w):
    _set_pixel(buf, width, height, ix, y, color)
    _set_pixel(buf, width, height, ix, y + h - 1, color)
  for iy in range(y, y + h):
    _set_pixel(buf, width, height, x, iy, color)
    _set_pixel(buf, width, height, x + w - 1, iy, color)


def _fill_rect(buf, width, height, x, y, w, h, color):
  for iy in range(y, y + h):
    for ix in range(x, x + w):
      _set_pixel(buf, width, height, ix, iy, color)


def write_guided_bmp(path, width, height, fill=(180, 180, 180), force=False, kind=None):
  """Write BMP and draw guide boxes for known kinds: 'player','enemy','items','tileset'."""
  buf = _create_buffer(width, height, fill)
  black = (0, 0, 0)
  red = (200, 40, 40)
  blue = (40, 120, 200)

  if kind == 'player':
    cell_w = 32
    cell_h = 32
    for col in range(2):
      _draw_rect_outline(buf, width, height, col * cell_w, 0, cell_w, cell_h, black)
    _draw_rect_outline(buf, width, height, 0, 0, 22, 28, red)
    _draw_rect_outline(buf, width, height, cell_w, 0, 22, 16, red)
  elif kind == 'enemy':
    _draw_rect_outline(buf, width, height, 0, 0, 32, 32, black)
    _draw_rect_outline(buf, width, height, 0, 0, 22, 26, red)
  elif kind == 'items':
    sizes = [(14, 18), (15, 12), (13, 14), (16, 10), (10, 10)]
    cell_w = 32
    cell_h = 32
    for i, (w, h) in enumerate(sizes):
      cx = i * cell_w
      _draw_rect_outline(buf, width, height, cx, 0, cell_w, cell_h, black)
      _draw_rect_outline(buf, width, height, cx, 0, w, h, red)
  elif kind == 'tileset':
    # Fill each tile cell with a distinct colour so they are visually obvious
    tile_colors = [ (200,100,100), (100,200,100), (100,140,200), (200,180,100), (180,100,200), (100,200,180) ]
    for col in range(6):
      cx = col * 32
      _fill_rect(buf, width, height, cx, 0, 32, 32, tile_colors[col % len(tile_colors)])
      _draw_rect_outline(buf, width, height, cx, 0, 32, 32, black)
    plat_colors = [ (120,120,120), (150,150,150), (100,100,140) ]
    for col in range(3):
      cx = col * 32
      _fill_rect(buf, width, height, cx, 32, 32, 6, plat_colors[col % len(plat_colors)])
      _draw_rect_outline(buf, width, height, cx, 32, 32, 6, black)
  else:
    _draw_rect_outline(buf, width, height, 0, 0, width, height, black)

  _write_bmp_buffer(path, width, height, bytes(buf), force=force)


def main():
    force = "--force" in sys.argv
    print("Generating placeholder BMP sprite sheets…")
    write_guided_bmp("assets/tileset.bmp", 192, 38,  fill=(180, 180, 180), force=force, kind='tileset')
    write_guided_bmp("assets/player.bmp",   64, 32,  fill=(180, 180, 180), force=force, kind='player')
    write_guided_bmp("assets/enemy.bmp",    32, 32,  fill=(180, 180, 180), force=force, kind='enemy')
    write_guided_bmp("assets/items.bmp",   160, 32,  fill=(180, 180, 180), force=force, kind='items')
    print("Done.")
    print()
    print("Open the files in a pixel editor (e.g. Aseprite, GIMP, or LibreSprite)")
    print("and draw your sprites according to the layout described in this script.")
    print("The game falls back to procedural rendering for any missing file.")


if __name__ == "__main__":
    main()
