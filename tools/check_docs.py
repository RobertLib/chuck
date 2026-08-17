#!/usr/bin/env python3
"""Hold the prose to the maps it describes.

`make test` already keeps every table *in code* honest about the campaign:
`INTEL_ARC_SECTORS` is compared to the maps, the manual's mission sheet reads
`CAMPAIGN_SECTORS` rather than a literal, and a sheet of words is measured
against the frame it is drawn in. What none of that reaches is a sentence. The
docs state facts about the campaign in English — which sectors carry a camera,
which carry a heavy, which lay out two medkits — and a sentence is not a table,
so nothing compared any of them to `levels/*.txt`.

They drifted, all in the same direction and all in one go. The campaign grew
from fifteen sectors to seventeen; the vault gained a pair of cameras and four
heavies and the roof gained four more, and three sector lists went on naming the
old floors. One of them named sector 15, which had become a facade in the same
commit and therefore carries no men at all — a claim that was not merely stale
but impossible, sitting in the page a reader goes to in order to find out how
the enemy works.

The fix is not another table. A table of "which sectors have cameras" would be
read by nothing in the game, so it would be a second copy of the maps kept in
step by a test — the arrangement that causes this class of bug rather than the
one that ends it. Instead the truth is derived from `levels/*.txt` here, and the
prose is asked whether it agrees.

Each check names a doc, an anchor phrase that identifies the claim, and how to
count the thing being claimed. The anchor is deliberately a fragment of the
sentence's *reasoning* rather than of its list, so that rewording the list is
caught and rewording the reason fails loudly with "anchor not found" instead of
passing on a sentence this script can no longer see. A lint that cannot find
what it checks has to say so; silently checking nothing is how the sentences got
here.

**"The prose" is every page and every comment**, which took three sweeps to
learn. It read `docs/` alone until `levels/LEGEND.md` was found naming a facade
in its `F` panel list and `README.md` was found miscounting terminals; it read
those three until a comment in `gameplay_interaction.c` was found naming the same
facade as a floor with two medkits on it, and a comment in `level_art.c` quoting a
wall clock four minutes out. A sentence does not become checked by being in a
`.c` file, and the class of bug does not change either: the campaign moved and
the sentence describing it did not. Anything under `src/` that states a fact
about the campaign is fair game for a check here.

Runs from `make lint`, which `make test` depends on, for the reason
`check_palette.py` does: this is a question about text rather than behaviour, and
the suite links no SDL and reads no docs.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEVELS = ROOT / "levels"
DOCS = ROOT / "docs"
SRC = ROOT / "src"
TESTS = ROOT / "tests"

# Small numbers as the prose spells them. Both the docs and the manual's straps
# write these out in words, so a check that only compared digits would pass a
# page that had gone stale in every sentence a reader actually reads.
WORDS = [
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
    "sixteen", "seventeen", "eighteen", "nineteen", "twenty", "twenty-one",
    "twenty-two", "twenty-three", "twenty-four",
]


def spelled(value: int) -> str:
    if not 0 <= value < len(WORDS):
        raise SystemExit(f"check_docs: no spelling for {value}")
    return WORDS[value]


def define_of(name: str, path: Path) -> int:
    """The value of a `#define NAME <int>`, read from the header that owns it.

    The same trick CI uses on the pinned SDL version, and for the
    same reason: a weight copied into this file is a weight that stays at the
    old number the day it is tuned, which would leave this script confidently
    checking the prose against arithmetic the game no longer does.
    """
    match = re.search(
        rf"^#define {name}\s+(\d+)", path.read_text(), re.MULTILINE
    )
    if match is None:
        raise SystemExit(f"check_docs: could not read {name} from {path.name}")
    return int(match.group(1))


def bind_row_count() -> int:
    """How many controls `CHUCK_BIND_LIST` names.

    Not a `#define`, so it is counted rather than read: the list is an X-macro,
    which is exactly the arrangement that keeps the enum, the table and the row
    count from disagreeing *in code*. What it does nothing about is the prose,
    and this one number is written out in words in **eleven** places across six
    files — the README twice, `docs/screens.md`, `keybind.h` twice,
    `settings.h` twice, `settings.c` three times and the comment in `game.c`
    that sizes the settings buffer around it.

    Every one of them was correct and none of them was held to anything, which
    is the same position the camera and medkit sector lists were in before this
    script existed. A tenth control is one edit to the macro and eleven
    sentences that quietly become wrong, one of them the argument for the size
    of a buffer.
    """
    lines = (SRC / "keybind.h").read_text().splitlines()
    for i, line in enumerate(lines):
        if not line.startswith("#define CHUCK_BIND_LIST"):
            continue
        count = 0
        # The macro is one backslash-continued block; the first line without a
        # trailing backslash is its last.
        for row in lines[i + 1:]:
            count += row.count("ROW(")
            if not row.rstrip().endswith("\\"):
                break
        if count == 0:
            raise SystemExit("check_docs: CHUCK_BIND_LIST names no rows")
        return count
    raise SystemExit("check_docs: could not find CHUCK_BIND_LIST in keybind.h")


def float_define_of(name: str, path: Path) -> float:
    """The value of a `#define NAME <float>f`, for the clock's own three."""
    match = re.search(
        rf"^#define {name}\s+([0-9]+(?:\.[0-9]+)?)f?", path.read_text(),
        re.MULTILINE,
    )
    if match is None:
        raise SystemExit(f"check_docs: could not read {name} from {path.name}")
    return float(match.group(1))


def dial(sector: int) -> str:
    """What the wall clock in `sector` reads, as the pages write it.

    `draw_wall_clock` in game_render.c is one line —
    `NIGHT_CLOCK_FIRST_MINUTE + level_index * NIGHT_CLOCK_MINUTES_PER_SECTOR`,
    on a 0-based index — and this is that line with the same three constants
    read out of the header. The hand is not rounded anywhere, so the reading a
    player sees is the minute it has passed: truncated, not nearest.

    **This exists because three pages and one renderer comment quoted `00:47`
    for sector 11 and `00:55` for sector 14, and both were the fifteen-sector
    night.** Dividing the same thirty-eight minutes seventeen ways moved every
    dial in the building by up to four minutes, and the one constraint the
    climbs are under — that a facade is pinned to the minute by the interiors on
    both sides of it, so `FACADE_MOON` may not be a sunrise — is written down as
    exactly those readings. A number in prose that is arithmetic on a constant
    is the same defect as a colour spelled from memory.
    """
    minutes = FIRST_MINUTE + sector_index(sector) * MINUTES_PER_SECTOR
    return f"00:{int(minutes):02d}"


def step_spelled() -> str:
    """How long one sector is worth on the dial, as the page spells it.

    The two ends of the night are checked above and every dial between them is
    derived, but the *step* between two dials was prose alone — and it was
    wrong: `story.md` called it "a shade over two and a quarter minutes" when
    thirty-eight minutes over seventeen sectors is 2:14.1, a second under two
    and a quarter rather than over it. Written as a figure the page has to
    spell, it stops being a thing anybody has to hold in their head.
    """
    seconds = TOTAL_MINUTES * 60.0 / NIGHT_SECTORS
    return f"{spelled(int(seconds // 60))} minutes and {spelled(int(seconds % 60))} seconds"


def minutes_left_at(sector: int) -> int:
    """Whole minutes from `sector`'s dial to the helicopter at 01:00.

    The intel table's sector 11 row is the one line in the game that states a
    remaining duration, and it is arithmetic on the same constants the dials
    are. It said TEN when the night divided fifteen ways and the row was read
    at 00:50; it is read at 00:46 now. Nothing caught it because the row is one
    of the eleven a window suppresses — measured for width by the suite,
    reachable by no player, and true of a campaign that no longer exists.

    Derived from the *dial* rather than from the raw minute, for the reason
    `dial` gives: the hand is truncated, so a player standing under sector 12's
    wall clock reads 00:46 and the only arithmetic they can check is against
    that. 13.4 minutes actually remain; the line has to agree with the prop.
    """
    minutes = FIRST_MINUTE + sector_index(sector) * MINUTES_PER_SECTOR
    return int(FIRST_MINUTE + TOTAL_MINUTES) - int(minutes)


def sector_index(sector: int) -> int:
    return sector - 1


# The two hazard weights that have names of their own, read once. The rest of
# the formula is literals in `level_hazard_budget`; see `hazard_budget` below.
HEAVY_WEIGHT = define_of("ENEMY_HEAVY_HAZARD_WEIGHT", SRC / "game_config.h")
CAMERA_WEIGHT = define_of("CAMERA_HAZARD_WEIGHT", SRC / "game_config.h")
MANUAL_SHEETS = define_of("MANUAL_PAGE_COUNT", SRC / "manual_pages.h")
BIND_ROWS = bind_row_count()

# The dial, from the three constants `draw_wall_clock` reads.
FIRST_MINUTE = float_define_of("NIGHT_CLOCK_FIRST_MINUTE", SRC / "game_config.h")
TOTAL_MINUTES = float_define_of(
    "NIGHT_CLOCK_TOTAL_MINUTES", SRC / "game_config.h"
)
NIGHT_SECTORS = define_of("NIGHT_CLOCK_SECTORS", SRC / "game_config.h")
MINUTES_PER_SECTOR = TOTAL_MINUTES / NIGHT_SECTORS

# What a guard pays, and what a whole par handed back pays.
#
# Both are quoted as digits in prose that argues about the *balance* between
# them — "speed is a genuine alternative to clearing a floor" is an arithmetic
# claim, and stripping the numbers out of it would leave a sentence asserting a
# ratio it no longer shows. So they are derived here instead, the way the dials
# are, rather than removed from the sentence.
#
# The par figure is why this pair is worth the trouble. `game_config.h` and
# `docs/story.md` carried the same sentence; the header's copy was corrected to
# 2680 when the night was divided seventeen ways instead of fifteen, and the
# page's kept saying 3000 — the ceiling of a campaign that no longer exists,
# sitting in the paragraph that explains what the score is *for*. One sentence,
# two copies, one of them fixed and nothing comparing them: exactly the
# arrangement this script exists to make impossible.
#
# `SECTOR_PAR_SECONDS` is itself derived from the night clock and truncated to
# whole seconds, so the truncation is repeated here rather than the value read;
# see the comment on it in `game_config.h`.
ENEMY_SCORE = define_of("ENEMY_SCORE", SRC / "game_config.h")
PAR_SECONDS = int(MINUTES_PER_SECTOR * 60.0)
PAR_BONUS = PAR_SECONDS * define_of(
    "SECTOR_TIME_BONUS_PER_SECOND", SRC / "game_config.h"
)

# "sectors 10, 12, 14 and 16", "sectors 10, 12, 14, 16 and 17", "sector 9".
# Stops at the first thing that is not part of a list of numbers, so the em dash
# and the clause after it are never swallowed.
SECTOR_LIST = re.compile(
    r"sectors?\s+(\d+(?:\s*,\s*\d+)*(?:\s*,?\s*and\s+\d+)?)", re.IGNORECASE
)


def sector_grids() -> dict[int, str]:
    """Every campaign sector's grid, keyed by sector number.

    The grid is everything before the first blank line; `SPAWNS`, `THEME` and
    `MODE` come after it. Reading the whole file instead would count the `M` in
    `THEME` as a guard, which is exactly the sort of answer a check like this
    must not quietly give.
    """
    grids: dict[int, str] = {}
    for path in LEVELS.glob("level*.txt"):
        number = int(re.search(r"level(\d+)\.txt$", path.name).group(1))
        grids[number] = path.read_text().split("\n\n", 1)[0]
    if not grids:
        raise SystemExit("check_docs: found no levels/level*.txt to read")
    return grids


def is_facade(grid_and_rest: str) -> bool:
    return "MODE FACADE" in grid_and_rest


def facade_sectors() -> set[int]:
    out = set()
    for path in LEVELS.glob("level*.txt"):
        number = int(re.search(r"level(\d+)\.txt$", path.name).group(1))
        if is_facade(path.read_text()):
            out.add(number)
    return out


def sectors_where(grids: dict[int, str], char: str, predicate) -> list[int]:
    return sorted(n for n, grid in grids.items() if predicate(grid.count(char)))


def decoration_characters() -> str:
    """Every character `level.c` lays a prop for, read out of `level.c`.

    Written down here as a literal for as long as this helper existed, which made
    it a fourth copy of a list that already lives in three places — and it fell
    behind the moment the plant set was added: `a` `e` `j` `l` were props the
    counting function did not count, so a sentence about how heavily a floor is
    dressed would have gone on quoting the campaign as it was before the set
    existed. Derived instead, out of the same parser `check_lists.py` compares to
    the legend, so a fifth prop is counted by having been parsed.
    """
    text = (SRC / "level.c").read_text()
    found = []
    for match in re.finditer(
        r"case '(.)':\s*\n[^\n]*\n\s*place_decoration\(", text
    ):
        found.append(match.group(1))
    if not found:
        raise SystemExit(
            "docs: no props found in level.c's parser; this check reads nothing"
        )
    return "".join(found)


def decorations(sector: int) -> int:
    """How many props a sector lays out.

    Every character `place_decoration` is reached for in `level.c`, counted off
    the grid. The loader drops an unsupported one, so this is the map's claim
    rather than the level's — which is the right number for a sentence about
    how heavily a floor is *dressed*, and the editor is what reports a prop
    standing on nothing.
    """
    grid = sector_grids()[sector]
    return sum(grid.count(char) for char in decoration_characters())


def interior_dressing_range() -> tuple[int, int, int]:
    """The interiors, and the least and most dressed of them.

    The claim this feeds used to be two lists of themes — one furnished, one bare
    — and the argument it was making was that the bare five were missing a prop
    *set* rather than a map. The set exists now, so what has to stay true is the
    narrower thing: that no interior is an outlier any more. A sixth bare room
    fails this rather than fitting a list nobody updated.
    """
    grids = sector_grids()
    counts = [
        decorations(n) for n in sorted(grids) if n not in facade_sectors()
    ]
    return len(counts), min(counts), max(counts)


def hazard_budget(grid: str, facade: bool) -> int:
    """What `level_hazard_budget` would say about this map.

    **This is a second copy of that function and it is one on purpose**, which
    is the opposite of the rule at the top of this file, so it is worth saying
    why. The numbers the prose quotes are the *output* of the C function, and
    nothing in the tree could compare the two: `make test` computes them and
    checks only that they rise, and this script reads the docs but links no C.
    The alternative to a second copy here is no check at all, which is what the
    campaign had while `levels/LEGEND.md` quoted a sector 12 that beat the floor
    below it "by two" long after the maps had made it seven.

    The coupling is kept as small as it can be: the two weights that have names
    are read out of the header that defines them, and the character meanings
    come from one `switch` in `level.c`. If the formula in `level_route.c`
    changes, this and the sentence it checks both have to move — which is a
    third place to edit, and still cheaper than a page of numbers nobody holds.
    """
    if facade:
        # `r` throws, `v` flies; see `place_facade_hazard` in level.c.
        return 3 * grid.count("r") + 2 * grid.count("v")
    # `M` and `W` are guards, `W` bringing a dog; `Q` is the heavy. `X` mines,
    # `^` spikes, `O` fans, `I` cameras.
    men = 3 * (grid.count("M") + grid.count("W"))
    men += HEAVY_WEIGHT * grid.count("Q")
    return (
        men
        + 2 * grid.count("W")
        + 2 * grid.count("X")
        + grid.count("^")
        + CAMERA_WEIGHT * grid.count("I")
        + grid.count("O")
    )


def budgets() -> tuple[dict[int, int], list[int], list[int], list[int]]:
    """Every sector's budget, and the three sequences the prose quotes."""
    facades = facade_sectors()
    grids = sector_grids()
    per_sector = {
        n: hazard_budget(grid, n in facades) for n, grid in grids.items()
    }
    inside = [per_sector[n] for n in sorted(per_sector) if n not in facades]
    walls = [per_sector[n] for n in sorted(per_sector) if n in facades]
    steps = [b - a for a, b in zip(inside, inside[1:])]
    return per_sector, inside, walls, steps


def joined(values: list[int]) -> str:
    """`6, 14, 20` — the way the page writes a run of them."""
    return ", ".join(str(v) for v in values)


def joined_and(values: list[int]) -> str:
    """`8, 6, 4 and 12` — the way the page ends one."""
    if len(values) < 2:
        return joined(values)
    return f"{joined(values[:-1])} and {values[-1]}"


def terminal_facts(grids: dict[int, str]) -> dict[str, int]:
    """What the README says about terminals, derived from the maps.

    A sector leaves by a window if it has a `Y`; that is
    `try_finish_current_level`'s own test, and it is what decides both the
    report and — the reason this check exists — whether the sentence in the
    README is true.
    """
    windows = sorted(n for n, g in grids.items() if g.count("Y"))
    stairs = sorted(n for n in grids if n not in windows)
    stair_terminals = [grids[n].count("T") for n in stairs]
    with_terminals = [n for n in windows if grids[n].count("T")]
    return {
        "windows": len(windows),
        "window_without": len(windows) - len(with_terminals),
        "stair_min": min(stair_terminals),
        "stair_max": max(stair_terminals),
        "exception": with_terminals[0] if len(with_terminals) == 1 else -1,
        "exception_terminals": (
            grids[with_terminals[0]].count("T")
            if len(with_terminals) == 1
            else -1
        ),
    }


def flattened(path: Path) -> str:
    """The file as one line of prose, with a C comment's furniture removed.

    Every claim below is matched against whitespace-collapsed text so a reflow of
    the paragraph is not a failure and only a change of words is. In a `.c` file
    that is not enough on its own: collapsing ` *` line leaders leaves an
    asterisk in the middle of every sentence that wraps, so an anchor spanning
    two lines could never match and the check would report "anchor not found"
    for a sentence that is sitting right there. Stripping the leader first is
    what lets a comment be held to the same claim a page is.
    """
    text = path.read_text()
    if path.suffix in (".c", ".h"):
        text = re.sub(r"^[ \t]*\*[ \t]?", " ", text, flags=re.MULTILINE)
    return re.sub(r"\s+", " ", text)


def claimed_sectors(text: str, anchor: str) -> tuple[list[int] | None, str]:
    """The sector list the prose gives for `anchor`.

    Taken as the *last* list before the anchor, because a paragraph may mention
    other sectors earlier — "sector 12 carries two grenades, sectors 10, 12, 16
    and 17 two medkits apiece" is one sentence with two of them in it.
    """
    at = text.find(anchor)
    if at < 0:
        return None, f"anchor not found: {anchor!r}"
    matches = list(SECTOR_LIST.finditer(text, 0, at))
    if not matches:
        return None, f"no sector list before anchor {anchor!r}"
    return [int(n) for n in re.findall(r"\d+", matches[-1].group(1))], ""


def main() -> int:
    grids = sector_grids()
    facades = facade_sectors()
    interiors = {n: g for n, g in grids.items() if n not in facades}

    checks = [
        (
            "cameras",
            DOCS / "gameplay.md",
            # Reworded from "each pair" when the mechanic stopped arriving in
            # pairs: sectors 5, 6 and 8 carry a single lens apiece, because
            # introducing the one watcher the quiet answers do not work on is a
            # different job from pressuring a floor with two of them.
            "is worth\n`CAMERA_HAZARD_WEIGHT` of that sector's budget",
            sectors_where(grids, "I", lambda n: n > 0),
        ),
        (
            "heavies",
            DOCS / "gameplay.md",
            "More of them the closer the roof gets",
            sectors_where(grids, "Q", lambda n: n > 0),
        ),
        (
            "sectors with two medkits",
            DOCS / "gameplay.md",
            "two medkits apiece",
            sectors_where(interiors, "K", lambda n: n == 2),
        ),
        # Which floors carry the one answer to having already been seen. It was
        # prose nowhere at all until sector 16 was found without a charge — the
        # second-hardest interior in the campaign and the only late floor with no
        # escape on it — so the list is written down now, which means it owes this
        # script an entry like every other sector list.
        (
            "flash charges",
            DOCS / "gameplay.md",
            "where a sector can go wrong",
            sectors_where(grids, "!", lambda n: n > 0),
        ),
        # Which floors bank a checkpoint on doors and medkits alone. A window
        # sector's stair door is welded, so there is nothing for a card or a
        # terminal to unlock and neither is laid out — which is also why three of
        # them ended up with every bank they owned off the route entirely. The
        # route half of that claim is `make test`'s
        # (`test_no_sector_asks_for_a_long_walk_with_nothing_banked`, which needs
        # the route model); the half this script can see is which sectors are in
        # the position at all, and that is a fact about the maps.
        (
            "window sectors banking on doors and medkits alone",
            DOCS / "gameplay.md",
            "because a welded stair door leaves nothing for either\nof them to open",
            sorted(
                n
                for n, grid in interiors.items()
                if grid.count("Y") > 0
                and grid.count("C") == 0
                and grid.count("T") == 0
            ),
        ),
        # `levels/LEGEND.md` was outside this script until the `F` list in it was
        # found naming sector 15 — a facade, which has no slab to cut a panel
        # into, so the claim was not stale but impossible. Exactly the shape the
        # three checks above exist for, on the one page they did not cover.
        (
            "fall-through panels",
            LEVELS / "LEGEND.md",
            "a climb has no slab to cut a panel into",
            sectors_where(grids, "F", lambda n: n > 0),
        ),
        # And a sector list in a *comment* is prose as much as one in a page.
        # `gameplay_interaction.c` argues for the wasted-pickup rule by naming
        # the maps it matters on, and that list said `10, 12 and 15` long after
        # 15 had become a facade with one medkit on it — the same claim
        # `docs/gameplay.md` above got right, because that one was checked and
        # this one was in a file this script did not read.
        (
            "sectors with two medkits, said in the source",
            SRC / "gameplay_interaction.c",
            "every restroom hands out the grenade the campaign's own budget",
            sectors_where(interiors, "K", lambda n: n == 2),
        ),
        # And a sector list in a test's comment is prose for the same reason one
        # in a source comment is. These two say which floors the mechanic the
        # test covers is actually on, which is the argument for the test
        # existing — `P` and `F` had no test at all until `make coverage` was
        # written, so the lists are new and start out held rather than being
        # found stale later, which is how every other entry on this list got
        # here.
        (
            "sectors with a moving platform",
            TESTS / "test_main.c",
            "and not one of them had ever moved under a test",
            sectors_where(grids, "P", lambda n: n > 0),
        ),
        (
            "sectors with a falling panel",
            TESTS / "test_main.c",
            "and the same was true of every one of those",
            sectors_where(grids, "F", lambda n: n > 0),
        ),
        # The `J` list, which was true and unpinned: it is the one sentence in
        # `story.md` that decides where a visual-only NPC may stand, and the
        # difference between it and the receptionist's rule is the whole
        # paragraph. Nothing was holding either half to the maps.
        (
            "sectors with a janitor",
            DOCS / "story.md",
            "which is a civilian still working at",
            sectors_where(grids, "J", lambda n: n > 0),
        ),
        # The restrooms, which is the sector list the README has always carried
        # and nothing has ever held. It is correct today. So were the camera, the
        # heavy and the medkit lists on the day before the campaign grew — and
        # this one is load-bearing in the same way, because a `U` moved or added
        # changes which floors offer the detour the README goes on to describe.
        (
            "sectors with a restroom",
            ROOT / "README.md",
            "chosen by the floor's own theme",
            sectors_where(grids, "U", lambda n: n > 0),
        ),
        # And the *other* half of the wasted-pickup sentence. The medkit list in
        # it has been checked since this script learned to read a `.c` file; the
        # grenade clause in front of it never was, because `claimed_sectors`
        # takes the last list before the anchor and that is the medkit one. Two
        # claims in one sentence need two anchors, or the check silently covers
        # whichever half it happens to reach.
        (
            "the sector with two grenades",
            SRC / "gameplay_interaction.c",
            "carries two grenades",
            sectors_where(interiors, "N", lambda n: n == 2),
        ),
        (
            "the sector with two grenades, said in the docs",
            DOCS / "gameplay.md",
            "carries two grenades",
            sectors_where(interiors, "N", lambda n: n == 2),
        ),
    ]

    per_sector, inside, walls, steps = budgets()
    # How many floors carry a platform of either kind. A count over two lists,
    # which `SECTOR_LIST` cannot see: one sector carries both, so adding the two
    # lists up gives eight where the answer is seven — and eight is exactly what
    # the sentence said before this was written.
    platform_floors = len(set(sectors_where(grids, "P", lambda n: n > 0)) |
                          set(sectors_where(grids, "F", lambda n: n > 0)))
    terminals = terminal_facts(grids)

    # Claims that are a number in a sentence rather than a list of sectors, so
    # `claimed_sectors` cannot see them. Each entry is an anchor that proves the
    # sentence is still there, and the phrases that sentence has to contain.
    # A phrase that has gone missing is reported with what the maps say instead.
    phrase_checks = [
        # The docket's size, spelled as a figure on the page that argues for the
        # collection and printed as one on both screens between sectors. It is
        # one sheet to an interior and none on a climb
        # (`test_every_interior_lays_out_exactly_one_docket_sheet`), so the
        # number is the interior count and belongs to the maps rather than to
        # whoever last typed it.
        (
            "the docket's size",
            DOCS / "story.md",
            "prints `DOCKET",
            [f"`DOCKET n/{len(interiors)}` under the score"],
        ),
        # The Makefile's note on why `make coverage` exists quotes how many floors
        # carry a platform at all, which is a *count* over two lists and so
        # invisible to the parser above. It was written as eight and the maps say
        # seven — one sector carries both — which is the arithmetic slip this
        # half of the script exists for.
        (
            "how many floors carry a platform",
            ROOT / "Makefile",
            "which is the fallback that keeps a reinforcement",
            [f"`P` and `F` are on {spelled(platform_floors)} shipped floors"],
        ),
        (
            "hazard budget sequence",
            LEVELS / "LEGEND.md",
            "These are measured rather than written down",
            [
                f"runs {joined(inside)} inside",
                f"and {joined(walls)} on the walls",
                f"steps of {joined_and(steps)}",
            ],
        ),
        (
            "the finale's step",
            LEVELS / "LEGEND.md",
            "the largest step in the run on purpose",
            [
                f"{inside[-1]} against the vault's {inside[-2]} is a step of "
                f"{spelled(inside[-1] - inside[-2])}",
                f"sector 9's {spelled(per_sector[9] - per_sector[8])}",
            ],
        ),
        (
            "sector 12's step",
            LEVELS / "LEGEND.md",
            "clearing the rule is not the same as keeping it",
            [
                f"stands at {per_sector[12]} against sector 10's "
                f"{per_sector[10]} now, a step of "
                f"{spelled(per_sector[12] - per_sector[10])}",
            ],
        ),
        # The sentence that said "the eight that leave by a window carry none"
        # while ten did and one of those ten carried three. It broke in the same
        # edit that took the report off sector 14, and nothing could see it: the
        # claim is counts in prose, not a list this script already parsed.
        (
            "terminals against the way out",
            ROOT / "README.md",
            "because a window has no lock to pick",
            [
                f"carries {spelled(terminals['stair_min'])} or "
                f"{spelled(terminals['stair_max'])} terminals",
                f"{spelled(terminals['window_without'])} of the "
                f"{spelled(terminals['windows'])} that leave by a window",
                f"Sector {terminals['exception']} is the one exception and "
                f"keeps {spelled(terminals['exception_terminals'])}",
            ],
        ),
        # The cameras, counted rather than listed — and the one claim in the
        # tree this script was already checking on one page and missing on
        # another. `docs/gameplay.md` names the sectors, so the list parser
        # above has held it since this file existed; the README says only how
        # many there are, which `SECTOR_LIST` cannot see, and it said **three**
        # while four sectors carried eight cameras between them. It went stale
        # when the vault arrived with a pair of its own and nothing looked at
        # the sentence, because nothing could.
        #
        # That is the second half of the lesson already written at the top of
        # `phrase_checks`: a page that spells a figure instead of listing the
        # sectors behind it needs an entry here, and the two halves of one fact
        # need one entry each.
        (
            "how many sectors carry a camera",
            ROOT / "README.md",
            "the one watcher none of the quiet answers below work on",
            [
                f"{spelled(len(sectors_where(grids, 'I', lambda n: n > 0)))}"
                f" sectors also watch the ceiling".capitalize(),
            ],
        ),
        # And *where the lens arrives*, which is the half of the same fact this
        # script was checking on one page and missing on another — the exact
        # shape the entry above was written to close, one file over.
        #
        # `docs/gameplay.md` names the sectors that carry a camera and the list
        # parser has held it since this file existed, so that page was correct
        # the whole time. `levels/LEGEND.md` states the same fact as an
        # **ordinal** — which sector is the first — and an ordinal is not a
        # sector list, so `SECTOR_LIST` could never see it. The plan table said
        # sector 10 was "the first sector with cameras on the ceiling" while
        # sectors 5, 6 and 8 each carried one, and it had been false since the
        # edit that put them there: that edit rewrote the paragraph in
        # `gameplay.md` which *argues* for moving the mechanic down — "the list
        # starts at five rather than at ten" — and left the table asserting the
        # thing the move had just made untrue. The page explaining the change
        # and the page contradicting it shipped in the same commit.
        #
        # Both numbers are derived, because both are claims: the sector the
        # single lens starts in, and the sector the pair starts in. A rule that
        # only pinned the first would let "a pair" drift to whichever floor
        # gained a second camera next.
        (
            "where the camera arrives, on the plan table",
            LEVELS / "LEGEND.md",
            "which is the floor the monitor wall belongs to",
            [
                f"A single lens has been overhead since sector "
                f"{min(sectors_where(grids, 'I', lambda n: n > 0))}",
                f"sector {min(sectors_where(grids, 'I', lambda n: n >= 2))}"
                f" is the first with a pair of them",
            ],
        ),
        # And the climbs, which are the same shape on the same page: a count in
        # front of the list it counts. This one is correct today and has never
        # been held to anything, which is exactly what the camera sentence was
        # the day before the vault was drawn.
        (
            "how many sectors are climbed",
            ROOT / "README.md",
            "gravity and ladders are replaced by four-way movement",
            [
                f"{spelled(len(sorted(facades))).capitalize()} sectors are not"
                f" walked at all",
                "Levels " + joined_and(sorted(facades)) + " are climbed",
            ],
        ),
        # The par itself, in the four places that state how long a floor gets.
        #
        # All four said "two and a half minutes", which is 150 seconds and was
        # the slot when the night was divided fifteen ways. Seventeen ways is
        # 134, so every one of them was describing a game that no longer
        # shipped — including the comment inside the renderer that draws the
        # stopwatch, and the sentence in `story.md` that repeats the figure
        # twice in one breath to make the point that the dial and the score
        # cannot disagree. Written as a figure rather than as words for the
        # reason the dials are: `SECTOR_PAR_SECONDS` is arithmetic on the night
        # clock, and prose that is arithmetic on a constant is checkable or it
        # is a number somebody will have to remember.
        (
            "how long a floor gets, on the README",
            ROOT / "README.md",
            "every second of that you hand back is worth points",
            [f"The night clock gives each floor {PAR_SECONDS} seconds"],
        ),
        (
            "how long a floor gets, on the story page",
            DOCS / "story.md",
            "the par it measures against is the night clock's own",
            [
                f"the {PAR_SECONDS} seconds the dial upstairs gives a floor "
                f"are the {PAR_SECONDS} seconds the score gives it",
            ],
        ),
        (
            "how long a floor gets, on the screens page",
            DOCS / "screens.md",
            "they exist because the game had been asking for them all along",
            [f"The night clock gives every floor {PAR_SECONDS} seconds"],
        ),
        (
            "how long a floor gets, in the renderer that draws the clock",
            SRC / "cutscene.c",
            "giving them nothing to be fast against",
            [f"{PAR_SECONDS} seconds is what the *night* allows"],
        ),
        # The balance between the two ways to play a sector, stated as
        # arithmetic in two files. See `PAR_BONUS` above for which of the two
        # was found wrong.
        (
            "what speed pays against what a floor pays",
            DOCS / "story.md",
            "speed is a genuine alternative to clearing a floor",
            [
                f"a full par under is {PAR_BONUS}",
                f"eight or so men at {ENEMY_SCORE} apiece",
            ],
        ),
        (
            "the same balance, in the header that sets the rates",
            SRC / "game_config.h",
            "The rates are set so a fast, clean floor is worth about what its",
            [
                f"the whole par is {PAR_BONUS} at "
                f"`SECTOR_TIME_BONUS_PER_SECOND`",
                f"eight or so men at {ENEMY_SCORE} apiece",
            ],
        ),
        (
            "the manual's sheet count",
            ROOT / "README.md",
            "field manual",
            [
                f"is {spelled(MANUAL_SHEETS)} illustrated sheets",
            ],
        ),
        # The same count on the page that describes the same screen. It was
        # checked on the README and not here, which is how one of two sentences
        # about one number stays right while the other goes stale.
        (
            "the manual's sheet count, on the screens page",
            DOCS / "screens.md",
            "opens `STATE_MANUAL`",
            [
                f"{spelled(MANUAL_SHEETS)} sheets in",
            ],
        ),
        #
        # The bindable controls, in the six files that spell the number out.
        #
        # `CHUCK_BIND_LIST` is an X-macro precisely so the enum, the table and
        # the count cannot disagree — and then the count was written out in words
        # eleven times in prose, where the macro cannot reach it. One of those
        # eleven is the argument for the size of the buffer `game_save_settings`
        # writes the sheet into, which is the copy that would turn a tenth
        # control into a settings file silently missing its last rows: the exact
        # failure the comment there describes having already had once.
        #
        # Grouped one entry per file rather than eleven entries, because the
        # anchor's job is to prove the sentence is still there and a file's worth
        # of them stands or falls together.
        (
            "the bindable controls, on the README",
            ROOT / "README.md",
            "can be rebound, on the keyboard and on a controller",
            [
                f"All {spelled(BIND_ROWS)} of the sector controls below",
                f"Rebind any of the {spelled(BIND_ROWS)} sector controls",
            ],
        ),
        (
            "the bindable controls, on the screens page",
            DOCS / "screens.md",
            "The plate is sized from its rows",
            [
                f"{spelled(BIND_ROWS)} controls with two keys apiece",
            ],
        ),
        (
            "the bindable controls, in the header that lists them",
            SRC / "keybind.h",
            "What is bindable and what is not",
            [
                f"The {spelled(BIND_ROWS)} below are the controls of a sector",
                f"The keyboard has had {spelled(BIND_ROWS)} rows and two slots",
            ],
        ),
        (
            "the bindable controls, on the struct that holds them",
            SRC / "settings.h",
            "the reason ESC, ENTER and BACKSPACE are not among the keys",
            [
                f"the {spelled(BIND_ROWS)} sector controls, two slots each",
                f"keeps {spelled(BIND_ROWS)} near-identical enum values",
            ],
        ),
        (
            "the bindable controls, on the sheet that draws them",
            SRC / "settings.c",
            "A binding row carries no detail line of its own",
            [
                f"The second page: the {spelled(BIND_ROWS)} sector controls",
                f"{spelled(BIND_ROWS)} sentences repeating it would push",
                f"its {spelled(BIND_ROWS)} `bind_*` rows apply",
            ],
        ),
        # The buffer sized around the number. This is the one where a stale
        # sentence costs something at runtime rather than only misinforming a
        # reader: it is the reasoning that says 2048 bytes is enough for the
        # file, and `settings_serialize` truncates cleanly, so being wrong here
        # looks like the options sheet forgetting a row.
        (
            "the bindable controls, in the buffer sized for them",
            SRC / "game.c",
            "Big enough for the whole sheet with room to spare",
            [
                f"{spelled(BIND_ROWS)} lines of `bind_weapon_next LSHIFT "
                f"RSHIFT`",
            ],
        ),
        # The dials. Every reading below is arithmetic on `NIGHT_CLOCK_*`, and
        # every one of them was written for a night divided fifteen ways: the
        # climbs' one hard constraint — a facade is pinned to the minute by the
        # interiors either side of it, which is why `FACADE_MOON` is not a
        # sunrise — was stated as `00:47` in three places and one renderer
        # comment, and the answer is now 00:44 with 00:42 and 00:46 beside it.
        # A wall clock is the one thing in the game that states the fiction's
        # clock out loud, so a page that misquotes it is a page contradicting the
        # thing the player can see.
        (
            "the night's own two ends",
            DOCS / "story.md",
            "which is also what sector one's wall clock reads",
            [
                f"**{dial(1)}** the SUV reaches the tower",
                f"**{dial(1)}-{dial(NIGHT_SECTORS)}** the "
                f"{spelled(NIGHT_SECTORS)} sectors",
            ],
        ),
        (
            "the dials either side of the moon climb",
            LEVELS / "LEGEND.md",
            "Those two minutes are derived rather than remembered",
            [f"whose dials read {dial(10)} and {dial(12)}"],
        ),
        (
            "the moon climb's own hour",
            DOCS / "levels.md",
            "The composition survived the fix intact",
            [
                f"in sector 11, which sits at {dial(11)} between dials reading "
                f"{dial(10)} and {dial(12)}",
            ],
        ),
        (
            "the dials either side of the moon climb, said in the source",
            SRC / "level_art.c",
            "so the next campaign that grows moves this comment with it",
            [f"side of it read {dial(10)} and {dial(12)}"],
        ),
        # The receptionist's rule, which is an argument about what a *time* makes
        # unbelievable, so the time is the claim. `00:55` here was sector 14's
        # dial when the night divided fifteen ways; it is sector 16's now.
        (
            "where the front desk stops being believable",
            DOCS / "story.md",
            "which put a civilian calmly working a counter",
            [f"three sectors below the roof at {dial(14)}"],
        ),
        (
            "the same hour on the legend's own page",
            LEVELS / "LEGEND.md",
            "A staffed counter is a post",
            [f"somebody standing at it at {dial(14)} is"],
        ),
        # How far one sector moves the hand. Between the two ends of the night,
        # which are checked above, and every dial, which is derived — this was
        # the one figure on that line nothing held, and it was wrong.
        (
            "how far a sector moves the hand",
            DOCS / "story.md",
            "A change to any one of them is a change to the two cutscene",
            [f"climbing the dial at {step_spelled()} a sector"],
        ),
        # How heavily the floors are dressed. This used to be seven theme counts
        # and five more in one sentence, arguing that the five bare sectors were
        # a missing prop *set* rather than a missing map. The set is `a` `e` `j`
        # `l` now, so the claim worth holding is the one that says the argument
        # was settled: every interior inside one band, no outlier left. The
        # historical figures in that paragraph are deliberately *not* checked —
        # they are what the campaign used to be, and a check that dragged them
        # forward would erase the reason the set exists.
        (
            "the band every interior's dressing sits inside",
            LEVELS / "LEGEND.md",
            "The right answer was a third vocabulary",
            [
                f"All {spelled(interior_dressing_range()[0])} interiors now "
                f"carry {spelled(interior_dressing_range()[1])} to "
                f"{spelled(interior_dressing_range()[2])} props apiece",
            ],
        ),
        # The intel table's one duration. A row a window suppresses is still a
        # row that has to be true the day a map gives it back.
        (
            "what the climb's own row says is left",
            SRC / "intel.c",
            "FLIGHT CASES CAME UP THIS SHAFT",
            [
                f"THE SETTLEMENT CLOCK IS RUNNING. "
                f"{spelled(minutes_left_at(12)).upper()} MINUTES.",
            ],
        ),
    ]

    failures = 0
    for what, doc, anchor, truth in checks:
        if not doc.exists():
            print(f"docs: {doc.relative_to(ROOT)} is missing")
            failures += 1
            continue
        # See `flattened`: a reflow of the paragraph is not a failure, only a
        # change of words is, and a C comment's line leaders are furniture.
        flat = flattened(doc)
        claimed, problem = claimed_sectors(flat, re.sub(r"\s+", " ", anchor))
        if claimed is None:
            print(f"docs: {doc.relative_to(ROOT)}: {what}: {problem}")
            print("      the sentence moved or was reworded; update this check")
            failures += 1
            continue
        if claimed != truth:
            print(f"docs: {doc.relative_to(ROOT)}: {what}")
            print(f"      prose says sectors {claimed}")
            print(f"      maps  say sectors {truth}")
            failures += 1

    phrases_checked = 0
    for what, doc, anchor, phrases in phrase_checks:
        if not doc.exists():
            print(f"docs: {doc.relative_to(ROOT)} is missing")
            failures += 1
            continue
        flat = flattened(doc)
        if re.sub(r"\s+", " ", anchor) not in flat:
            print(f"docs: {doc.relative_to(ROOT)}: {what}: "
                  f"anchor not found: {anchor!r}")
            print("      the sentence moved or was reworded; update this check")
            failures += 1
            continue
        for phrase in phrases:
            phrases_checked += 1
            if re.sub(r"\s+", " ", phrase) not in flat:
                print(f"docs: {doc.relative_to(ROOT)}: {what}")
                print("      derived from the source of truth, the sentence "
                      "should contain:")
                print(f"        {phrase!r}")
                # Deliberately not "a campaign no longer in levels/", which is
                # what this said while every check was a map claim. Half of them
                # are not: the bindable-control count comes out of
                # `CHUCK_BIND_LIST` and the sheet count out of a `#define`, and
                # a failure message that names the wrong source of truth sends
                # the next reader to look in the wrong file.
                print("      it does not; the prose has fallen behind what it "
                      "describes")
                failures += 1

    if failures:
        print(f"docs: {failures} claim(s) disagree with what they describe")
        return 1

    print(
        f"docs: {len(checks)} sector claim(s) and {phrases_checked} derived "
        f"figure(s) checked against {len(grids)} maps and the headers that "
        f"define them, all agree"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
