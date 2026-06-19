"""Hold every renderer to the BBC Micro's eight colours, at the source.

The rule is docs/art-and-audio.md's first one: every pixel in the game is drawn
as MODE 2 would draw it — eight colours, a 4x2 grid, opaque — and none of it is
a filter. src/bbc.h is the one way a pixel reaches the frame, and this script is
what keeps it the one way. Two things fail the build:

  - **A colour that is not a `BbcColour`.** `SDL_Color` and `SDL_FColor` are
    the types of every colour this tree used to invent — a dark two shades off
    another dark, a lamp, a wash — and outside the layer there is no colour left
    that needs one. The eight are spelled exactly once, as numbers, in `BBC_RGB`
    in src/bbc_art.c; everywhere else a colour is one of eight names.
  - **An SDL draw call outside the layer.** `SDL_SetRenderDrawColor`,
    `SDL_RenderFillRect(s)`, `SDL_RenderLine(s)`, `SDL_RenderPoint(s)`,
    `SDL_RenderGeometry`, `SDL_RenderDebugText` and `SDL_SetRenderDrawBlendMode`
    are what every gradient, glow and translucent wash in this tree used to be
    made of. They belong to src/bbc.c and nothing else. The one exception is
    clearing the frame to black before anything is drawn, which is spelled
    `SDL_SetRenderDrawColor(<renderer>, 0, 0, 0, 255)` and is allowed anywhere.

The frame-level check is tools/check_bbc_frame.py, which tools/soak.sh runs over
a capture of every staged screen: this script cannot see a colour made at run
time by blending, and that one cannot see a call nothing has executed yet. They
are two halves of one rule.

It runs from `make lint`, which `make test` depends on.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

LAYER = {ROOT / "src" / "bbc.c"}

COLOUR_TYPE = re.compile(r"\bSDL_F?Color\b")
DRAW_CALL = re.compile(
    r"\b(SDL_SetRenderDrawColor|SDL_RenderFillRects?|SDL_RenderLines?|"
    r"SDL_RenderPoints?|SDL_RenderGeometry|SDL_RenderDebugText|"
    r"SDL_SetRenderDrawBlendMode|SDL_RenderRects?)\s*\("
)
BLACK_CLEAR = re.compile(
    r"SDL_SetRenderDrawColor\s*\(\s*[^,()]+(?:->[^,()]+)?\s*,\s*0\s*,\s*0\s*,\s*0\s*,\s*255\s*\)"
)


def strip_comments(text: str) -> str:
    """Comments may quote a call or a colour while explaining why it is gone;
    only code is held. Line structure is kept so reports name the right line."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if c == "\n" else " " for c in text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text[i] in "\"'":
            # A string or a character literal, escapes and all: a '"' in code
            # must not open a string that swallows the rest of the file.
            quote = text[i]
            j = i + 1
            while j < n and text[j] != quote and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i: j + 1])
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def main() -> int:
    sources = sorted(
        p
        for d in ("src", "editor")
        for p in (ROOT / d).glob("*.[ch]")
    )
    failures = []
    for path in sources:
        code = strip_comments(path.read_text(encoding="utf-8"))
        for number, line in enumerate(code.splitlines(), start=1):
            rel = path.relative_to(ROOT)
            if path in LAYER:
                continue
            for m in COLOUR_TYPE.finditer(line):
                failures.append(
                    f"{rel}:{number}: {m.group(0)} — a colour here is one of "
                    f"the eight (BbcColour)"
                )
            for m in DRAW_CALL.finditer(line):
                if m.group(1) == "SDL_SetRenderDrawColor" and BLACK_CLEAR.search(
                    line, m.start()
                ):
                    continue
                failures.append(
                    f"{rel}:{number}: {m.group(1)} outside src/bbc.c — draw "
                    f"through bbc.h"
                )
    if failures:
        for f in failures:
            print(f)
        print(f"palette: {len(failures)} pixel(s) that do not go through the BBC layer")
        return 1
    print(
        f"palette: {len(sources)} sources checked, every pixel goes through "
        f"src/bbc.h in the eight colours"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
