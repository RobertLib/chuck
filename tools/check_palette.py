#!/usr/bin/env python3
"""Hold every renderer to the palette in src/fx.h.

The rule this enforces is written down in docs/art-and-audio.md: a renderer may
keep a colour of its own only if it names it once with a reason, and *a literal that
reproduces an fx.h value is that constant misspelt*. It was a rule with nothing
behind it, and a rule nothing checks is a rule that drifts — measured before
this script existed, four literals reproduced a palette colour exactly and
another eleven landed within two units of one, which is a difference no eye can
see and every future reader has to re-derive.

Two thresholds, and the gap between them is deliberate.

An **exact** match is unambiguous: the author meant that colour, so the file
should say its name. Within EXACT_FAIL_DISTANCE the same holds — one or two
units on a single channel is invisible on screen, so a literal that close is a
misremembered constant rather than a decision. Both fail the build.

Further out the answer stops being obvious. A dark two shades off FX_NIGHT may
genuinely be a plane sitting behind another plane, which is exactly what the
art direction asks for. Those are reported as notes and never fail, because
rewriting them is an art decision and this script does not get to make one.

The suite cannot do this itself: it links no SDL, and this is a question about
source text rather than about behaviour. So it runs from `make lint`, which
`make test` depends on.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# One or two units on a channel is below the threshold of vision; anything that
# close to a palette entry is that entry, spelled from memory.
EXACT_FAIL_DISTANCE = 2
# Far enough out to be a deliberate shade, near enough to be worth a glance.
NOTE_DISTANCE = 8

COLOR_LITERAL = re.compile(
    r"\{\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\}"
)
PALETTE_ENTRY = re.compile(
    r"^#define\s+(FX_[A-Z0-9_]+)_RGBA\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)",
    re.M,
)


def read_palette(path: Path) -> dict[str, tuple[int, int, int, int]]:
    text = path.read_text(encoding="utf-8")
    palette = {
        m.group(1): tuple(int(m.group(i)) for i in range(2, 6))
        for m in PALETTE_ENTRY.finditer(text)
    }
    if not palette:
        raise SystemExit(f"{path}: no FX_*_RGBA entries found — has fx.h moved?")
    return palette


def nearest(colour, palette):
    """The palette entry closest to `colour`, and how far off it is."""
    best_name, best_distance = None, None
    for name, value in palette.items():
        if colour[3] != value[3]:
            continue  # A different alpha is a different intent, not a typo.
        distance = sum(abs(a - b) for a, b in zip(colour[:3], value[:3]))
        if best_distance is None or distance < best_distance:
            best_name, best_distance = name, distance
    return best_name, best_distance


def main() -> int:
    palette = read_palette(ROOT / "src" / "fx.h")

    sources = sorted(
        p
        for p in list((ROOT / "src").glob("*.c")) + list((ROOT / "editor").glob("*.c"))
    )

    failures: list[str] = []
    notes: list[str] = []

    for path in sources:
        rel = path.relative_to(ROOT)
        for number, line in enumerate(path.read_text(encoding="utf-8").split("\n"), 1):
            for match in COLOR_LITERAL.finditer(line):
                colour = tuple(int(g) for g in match.groups())
                if any(channel > 255 for channel in colour):
                    continue  # Not a colour; some other four-field struct.
                name, distance = nearest(colour, palette)
                if name is None or distance is None:
                    continue
                where = f"{rel}:{number}"
                spelled = ", ".join(str(c) for c in colour)
                if distance <= EXACT_FAIL_DISTANCE:
                    how = "is" if distance == 0 else f"is {distance} off"
                    failures.append(
                        f"{where}: {{{spelled}}} {how} {name} — "
                        f"name it, or use {{{name}_RGBA}} in a static table"
                    )
                elif distance <= NOTE_DISTANCE:
                    notes.append(f"{where}: {{{spelled}}} is {distance} from {name}")

    if notes:
        print(f"palette: {len(notes)} literal(s) near a palette colour (advisory)")
        for note in notes[:12]:
            print(f"  note: {note}")
        if len(notes) > 12:
            print(f"  note: ... and {len(notes) - 12} more")

    if failures:
        print(f"palette: {len(failures)} literal(s) reproduce a palette colour",
              file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    print(f"palette: {len(sources)} sources checked against "
          f"{len(palette)} fx.h colours, none misspelt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
