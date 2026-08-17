#!/usr/bin/env python3
"""Hold the lists that are written down more than once to each other.

AGENTS.md states the rule this file enforces in one sentence — *a number written
down twice is checked or it is two numbers* — and names the places it has already
been broken: the editor's own campaign length, the manual's drawn sector count,
`THEME_MUSIC` on the wrong side of the SDL line. Every one of those is a number,
and every one of them now has a check.

What had none was a **list**. Three of them, all currently in agreement and all
kept that way by nothing at all:

- **The map legend, three times over.** `levels/LEGEND.md` documents 51
  characters, `editor/editor_legend.c` paints them, and the parser in
  `src/level.c` reads them. LEGEND.md itself says "a character added here has to
  be added there as well, or it is a character the editor cannot paint" — which
  is true, and was an instruction to a reader rather than a check. The failure it
  describes is silent in the worst direction: `level.c` turns an unrecognised
  character into air, so a map painted with a symbol the parser does not know
  loads with a hole in it and says nothing.

- **The screens the soak sweep walks.** `game_soak_screen` in `src/game.c` knows
  thirteen names and `tools/soak.sh` lists them again. A name in the script that
  the game does not know fails loudly, because `--screen` refuses it — but the
  other direction is exactly the hole `soak.sh` was written to close: a screen
  added to the game and not to the script is sanitizer-compiled, never
  sanitizer-executed, and the sweep still reports a clean run. That is the defect
  AGENTS.md calls this project's own recurring one, *a check reporting coverage it
  does not have*, one layer further out than the last time it was found.

- **The command line.** `src/main.c` parses `--level`, `--soak` and `--screen` in
  three separate functions and then lists all three again in `SWITCHES[]` so that
  `warn_about_unknown_arguments` can tell the author about a typo. A fourth switch
  parsed but left out of that table is a flag the game accepts and reports as
  unknown in the same breath.

None of these belongs in `make test`: the suite links no SDL, so it cannot see
`game.c` or `main.c` at all, and a shell script is not a translation unit. They are
questions about source text, which is what `make lint` is for — the same reasoning
that puts `check_palette.py` and `check_docs.py` there.

This script deliberately derives nothing about *meaning*. It reads each list where
it is written and compares the sets, because the bug in every case above is a
missing entry rather than a wrong one.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
EDITOR = ROOT / "editor"
LEVELS = ROOT / "levels"
TOOLS = ROOT / "tools"


def fail(what: str, detail: str) -> None:
    print(f"lists: {what}")
    for line in detail.splitlines():
        print(f"      {line}")


def legend_characters() -> set[str]:
    """Every character documented in levels/LEGEND.md.

    The entries are bullets of the form ``- `X` : meaning``, with one written as
    ``- (space) :`` because a bullet cannot carry a backticked blank. That one is
    translated back here rather than being an exception the other two lists have
    to carry.
    """
    text = (LEVELS / "LEGEND.md").read_text()
    found = set()
    for match in re.finditer(r"^- (?:`(.+?)`|\(space\))\s*:", text, re.M):
        found.add(match.group(1) if match.group(1) is not None else " ")
    return found


def editor_palette_characters() -> set[str]:
    """Every character the editor can paint.

    `ED_SYMBOLS` is a table of literal-initialised structs whose first member is
    the symbol, so the first character literal of each row is the entry.
    """
    text = (EDITOR / "editor_legend.c").read_text()
    return {
        m.group(1).replace("\\", "")
        for m in re.finditer(r"\{\s*'(\\?.)'\s*,", text)
    }


def parser_characters() -> set[str]:
    """Every character src/level.c's grid parser recognises.

    Scoped to the parser's own `switch (c)` rather than to the whole file: a
    `case` elsewhere in `level.c` is not a map character, and a check that
    collected those would drift the moment somebody wrote one.
    """
    text = (SRC / "level.c").read_text()
    start = text.index("switch (c)")
    # The switch ends at the first line that closes it at the same indent as the
    # `switch` keyword itself, which is what the tree's Allman bracing gives us.
    indent = " " * (start - text.rindex("\n", 0, start) - 1)
    end = text.index(f"\n{indent}}}", start)
    body = text[start:end]
    found = {
        m.group(1).replace("\\", "")
        for m in re.finditer(r"case '(\\?.)':", body)
    }
    # A blank has no `case` of its own: the switch ends in `default:` laying air,
    # which is what makes a *blank* legal and every unrecognised character
    # silently legal with it. That default is the reason this check exists, so it
    # cannot also be read as the parser knowing every symbol — only the one
    # character LEGEND.md documents as air is credited to it.
    if re.search(r"^\s*default:", body, re.M):
        found.add(" ")
    return found


def soak_screen_names() -> set[str]:
    """The names `game_soak_screen` answers to."""
    text = (SRC / "game.c").read_text()
    start = text.index("bool game_soak_screen(")
    end = text.index("\n}", start)
    body = text[start:end]
    return {m.group(1) for m in re.finditer(r'SDL_strcmp\(name, "([^"]+)"\)', body)}


def soak_script_screen_names() -> set[str]:
    """The names tools/soak.sh actually walks."""
    text = (TOOLS / "soak.sh").read_text()
    match = re.search(r"screens=\(([^)]*)\)", text, re.S)
    if match is None:
        return set()
    return set(match.group(1).split())


def main_switch_names() -> tuple[set[str], set[str]]:
    """The switches main.c parses, and the ones it lists as known.

    The first set is every `--flag` compared against `argv` anywhere in the file,
    which is what the parsers do; the second is `SWITCHES[]`, which is what the
    unknown-argument warning reads.
    """
    text = (SRC / "main.c").read_text()
    parsed = {
        m.group(1)
        for m in re.finditer(r'SDL_strcmp\(argv\[i\], "(--[a-z-]+)"\)', text)
    }
    table = set()
    match = re.search(r"SWITCHES\[\]\s*=\s*\{([^}]*)\}", text, re.S)
    if match is not None:
        table = {m.group(1) for m in re.finditer(r'"(--[a-z-]+)"', match.group(1))}
    # A switch's own parser compares against its name, and so does the table, so
    # the table's entries appear in `parsed` as well. Subtracting them is what
    # makes the two sets answer different questions.
    return parsed - table, table


def enum_members(header: str, prefix: str) -> set[str]:
    """Every `PREFIX_*` name in a header's enum, minus its `_COUNT` terminator.

    Read out of the header rather than out of the table, because the enum is the
    side that decides how long the table has to be.
    """
    text = (SRC / header).read_text()
    found = {
        m.group(1)
        for m in re.finditer(rf"^\s*({prefix}_[A-Z0-9_]+)\s*(?:=[^,]*)?,", text, re.M)
    }
    return {name for name in found if not name.endswith("_COUNT")}


def designated_rows(source: str, table: str, prefix: str) -> set[str]:
    """The enum members a designated-initialiser table actually has a row for.

    Scoped to the table's own initialiser so that a `[LEVEL_THEME_X]` written
    anywhere else in the file is not counted as a row.
    """
    text = (SRC / source).read_text()
    # Past the opening brace, so that the declaration's own `[PREFIX_COUNT]` is
    # not read as a row of the table it sizes.
    start = text.index("{", text.index(table))
    end = text.index("\n};", start)
    body = text[start:end]
    return {
        m.group(1)
        for m in re.finditer(rf"\[\s*({prefix}_[A-Z0-9_]+)\s*\]\s*=", body)
    }


def main() -> int:
    failures = 0
    checks = 0

    # --- the map legend, in its three copies ---------------------------------
    checks += 1
    legend = legend_characters()
    palette = editor_palette_characters()
    parser = parser_characters()
    if not legend or not palette or not parser:
        fail(
            "the map legend",
            "one of the three lists came back empty, so this check read "
            "nothing\n"
            "a lint that cannot find what it checks is worse than no lint",
        )
        failures += 1
    else:
        for name, other, missing in (
            ("editor/editor_legend.c", "levels/LEGEND.md", legend - palette),
            ("levels/LEGEND.md", "editor/editor_legend.c", palette - legend),
            ("src/level.c's parser", "levels/LEGEND.md", legend - parser),
            ("levels/LEGEND.md", "src/level.c's parser", parser - legend),
        ):
            if missing:
                fail(
                    "the map legend",
                    f"{name} is missing {sorted(missing)!r}, which "
                    f"{other} has\n"
                    "the parser turns an unknown character into air, so a map "
                    "using one loads with a hole in it and says nothing",
                )
                failures += 1

    # --- the screens the sweep walks ----------------------------------------
    checks += 1
    known = soak_screen_names()
    walked = soak_script_screen_names()
    if not known or not walked:
        fail(
            "the soak screens",
            "one of the two lists came back empty, so this check read nothing",
        )
        failures += 1
    else:
        unreached = known - walked
        if unreached:
            fail(
                "the soak screens",
                f"tools/soak.sh does not walk {sorted(unreached)!r}, which "
                "game_soak_screen answers to\n"
                "that screen is compiled under the sanitizers and never "
                "executed by them, and the sweep still reports a clean run",
            )
            failures += 1
        unknown = walked - known
        if unknown:
            fail(
                "the soak screens",
                f"tools/soak.sh walks {sorted(unknown)!r}, which the game does "
                "not answer to",
            )
            failures += 1

    # --- the two designated-initialiser tables nothing else can reach -------
    #
    # These are the shape the unsized-array trick cannot fix. A positional table
    # can be written `[]` and measured against its enum by a `_Static_assert`,
    # which is what every one of them in the tree now does. A table indexed by
    # designator has no such length to measure: `[LEVEL_THEME_VAULT] = {...}` in
    # a table whose largest designator is the last theme is exactly as long with
    # a middle row missing as with it present, and the gap zero-fills.
    #
    # Both of them also sit on the far side of the SDL boundary, so `make test`
    # cannot reach either — which is what left them as the last two enum-indexed
    # tables in the tree with nothing holding them. `THEME_MUSIC` was the third
    # and moved to `level.c` to get a test; these two cannot move, because a wall
    # palette and a synthesiser plan are not level data.
    #
    # What a missing row costs, in each case:
    #   `THEME_ART`   — wall style 0 and black for every colour, so the sector
    #                   draws as an unlit void. It loads, it plays, and no check
    #                   in the tree says a word: a soak draws it and a black
    #                   frame is not an error.
    #   `MUSIC_PLANS` — `ensure_music_track` reads a null progression as "not
    #                   planned" and returns false, so the sector is silent.
    #                   That branch is deliberate and is what `MUSIC_INTRO` uses,
    #                   the title theme being hand-sequenced; every other track
    #                   reaching it is a floor that lost its score.
    for source, table, header, prefix, exempt, cost in (
        (
            "level_art.c",
            "THEME_ART[LEVEL_THEME_COUNT]",
            "level.h",
            "LEVEL_THEME",
            set(),
            "the sector draws as an unlit void and nothing reports it",
        ),
        (
            "audio.c",
            "MUSIC_PLANS[MUSIC_TRACK_COUNT]",
            "music_id.h",
            "MUSIC",
            # The title theme is hand-sequenced rather than planned, which the
            # table says out loud; `synth_music_intro` is its row.
            {"MUSIC_INTRO"},
            "the track is silent, because a null progression means unplanned",
        ),
    ):
        checks += 1
        members = enum_members(header, prefix) - exempt
        rows = designated_rows(source, table, prefix)
        if not members or not rows:
            fail(
                f"{table.split('[')[0]}",
                f"one of the two lists came back empty, so this check read "
                f"nothing\na lint that cannot find what it checks is worse "
                f"than no lint",
            )
            failures += 1
            continue
        missing = members - rows
        if missing:
            fail(
                f"{table.split('[')[0]}",
                f"src/{source} has no row for {sorted(missing)!r}\n"
                f"a designated initialiser zero-fills instead of failing to "
                f"build, so {cost}",
            )
            failures += 1
        stray = rows - members - exempt
        if stray:
            fail(
                f"{table.split('[')[0]}",
                f"src/{source} has a row for {sorted(stray)!r}, which is not "
                f"in src/{header}",
            )
            failures += 1

    # --- the command line ---------------------------------------------------
    checks += 1
    unlisted, table = main_switch_names()
    if not table:
        fail(
            "the command line",
            "SWITCHES[] came back empty, so this check read nothing",
        )
        failures += 1
    elif unlisted:
        fail(
            "the command line",
            f"src/main.c parses {sorted(unlisted)!r} without listing it in "
            "SWITCHES[]\n"
            "the game accepts the flag and reports it as unknown in the same "
            "breath",
        )
        failures += 1

    if failures:
        print(f"lists: {failures} list(s) disagree with their other copy")
        return 1

    print(
        f"lists: {checks} list(s) written down twice or more — "
        f"{len(legend)} legend characters across three files, "
        f"{len(known)} soak screens across two, "
        f"{len(table)} command-line switches, "
        f"and the two designated tables no test can reach — all agree"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
