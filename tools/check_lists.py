#!/usr/bin/env python3
"""Hold the lists that are written down more than once to each other.

AGENTS.md states the rule this file enforces in one sentence — *a number written
down twice is checked or it is two numbers* — and names the places it has already
been broken: the editor's own campaign length, the manual's drawn sector count,
`THEME_MUSIC` on the wrong side of the SDL line. Every one of those is a number,
and every one of them now has a check.

What had none was a **list**. Four of them now, all currently in agreement and
all kept that way by nothing at all:

- **The store page, twice over.** `itch/page.md` is the description to edit and
  `itch/page.html` is the same text again, because the itch.io editor swallows no
  markdown and offers no source view. "If you change one, change the other" was
  written above them, which is an instruction to a reader rather than a check.
  This is the worst direction of the four: `check_docs.py` holds the *markdown* to
  the campaign, so editing `page.md` alone leaves every gate green and the shop
  showing the old text.

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
from difflib import SequenceMatcher
from html import unescape
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
    # Matched without the return type, which used to be spelled out here and
    # broke this check the day `game_soak_screen` grew a third answer so that
    # `--screen` could stop appending the list of names to refusals that were
    # not about a name. A lint that cannot find what it checks is worse than no
    # lint, so the anchor is the function rather than its signature.
    start = text.index("game_soak_screen(Game *game")
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


def main_screen_message_names() -> set[str]:
    """The names `--screen`'s own refusal prints.

    The third copy of the list, and the one that had nothing holding it: the
    check below compared `game_soak_screen` with `tools/soak.sh` and never
    looked at the sentence the switch prints when it turns a name down. So the
    game accepted `reveal` while the refusal listed the fourteen names without
    it — a message about which names exist, wrong about which names exist, on
    the one switch a caller reads it from.
    """
    text = (SRC / "main.c").read_text()
    match = re.search(r'"--screen expects one of: (.*?)"\);', text, re.S)
    if match is None:
        return set()
    # The sentence is split across string literals for line width, so the
    # quoting and the joining whitespace both have to come back out.
    listed = re.sub(r'"\s*"', "", match.group(1))
    return {name.strip() for name in listed.split(",") if name.strip()}


def tooling_page_screen_names() -> set[str]:
    """The names [docs/tooling.md](../docs/tooling.md) spells out.

    The *fourth* copy, and the one the write-up above did not count. That
    paragraph found the refusal message and called it "the third copy", closing
    with `a list written down twice is usually written down three times` — and
    the page whose whole job is telling a reader which screens exist was sitting
    one directory over, enumerating them by hand and missing two. `reveal` and
    `resume` both arrived after it was typed, both went into `game_soak_screen`,
    `soak.sh` and the refusal message because this script holds those three, and
    neither went here because nothing did.

    Written in prose, so it costs a fixed shape to hold: the names are the
    backticked words between "holds the list" and the em-dash that closes the
    run. That is the same bargain `SECTOR_LIST` strikes in `check_docs.py`, and
    it is cheaper than the list being wrong.
    """
    text = (ROOT / "docs" / "tooling.md").read_text(encoding="utf-8")
    match = re.search(r"holds the\s+list\s+—(.*?)—", text, re.S)
    if match is None:
        return set()
    return set(re.findall(r"`([a-z]+)`", match.group(1)))


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


def store_page_words(path: Path, html: bool) -> list[str]:
    """The store page's copy as a list of words, from either of its two files.

    `itch/page.md` is the description to edit and `itch/page.html` is the same
    text again, because the itch.io editor swallows no markdown and offers no
    source view. Two copies of the most public prose in this project, with
    "if you change one, change the other" written above them and nothing holding
    them to it — which is this file's whole subject. The failure is silent in the
    worst direction: `check_docs.py` holds the *markdown* to the campaign, so
    editing `page.md` alone leaves every gate green and the shop showing the old
    text.

    Compared as words rather than characters, because one is marked up and the
    other is tagged: what has to agree is what a reader reads.
    """
    text = path.read_text(encoding="utf-8")
    if html:
        text = re.sub(r"<style.*?</style>", " ", text, flags=re.S)
        text = re.sub(r"<title>.*?</title>", " ", text, flags=re.S)
        text = re.sub(r"<[^>]+>", " ", text)
        text = unescape(text)
    else:
        text = re.sub(r"^#+ *", "", text, flags=re.M)
        text = re.sub(r"^\|.*$", "", text, flags=re.M)
        text = re.sub(r"^---+$", "", text, flags=re.M)
        # Bullet markers are markup, not words: the HTML spells them as <li>.
        text = re.sub(r"^ *[-*] ", "", text, flags=re.M)
        text = text.replace("**", "").replace("*", "").replace("`", "")
    # Letters and digits only. Punctuation is where the two formats legitimately
    # differ — stripping `</strong>,` leaves a space before the comma that the
    # markdown does not have — and no fact in this copy lives in a comma. A
    # figure like `00:22` still arrives as `00` and `22` and is still compared.
    return re.findall(r"[A-Za-z0-9']+", text)


def preflight_packages() -> set[str]:
    """The Debian packages `preflight_x11` in packaging/build_linux.sh names.

    The rows are ``"header|package"`` pairs inside one `for` list, so the package
    is everything after the pipe.
    """
    text = (ROOT / "packaging" / "build_linux.sh").read_text()
    body = re.search(
        r"^preflight_x11\(\)\n\{(.*?)^\}", text, re.S | re.M
    )
    if body is None:
        return set()
    return set(re.findall(r'"[^"]+\|([a-z0-9.+-]+)"', body.group(1)))


def workflow_packages() -> set[str]:
    """The packages the Linux payload job installs before it builds SDL.

    One `apt-get install` line continued with backslashes; everything after the
    last option is a package name.
    """
    text = (ROOT / ".github" / "workflows" / "payloads.yml").read_text()
    joined = text.replace("\\\n", " ")
    found: set[str] = set()
    for line in joined.splitlines():
        if "apt-get install" not in line:
            continue
        tail = line.split("apt-get install", 1)[1]
        found |= {
            word
            for word in tail.split()
            if not word.startswith("-") and word != "y"
        }
    return found


def staging_paths_scripts_delete() -> set[str]:
    """The intermediate trees the packaging scripts remove before they finish.

    Both `packaging/build_linux.sh` and `packaging/build_windows.sh` end with
    `rm -rf "$dist/stage"` — deliberately, and each says why beside it: left
    standing, the staging tree makes `dist/` hold the payload twice, which is how
    somebody comes to upload the folder instead of the archive.

    The paths come back as the literal fragment a workflow would have to write to
    reach one, so `$dist/stage` is reported as `dist/stage`. That is the whole of
    what this needs to be: the question downstream is a substring one.
    """
    found: set[str] = set()
    for script in sorted((ROOT / "packaging").glob("build_*.sh")):
        for line in script.read_text().splitlines():
            # Line by line and comments dropped, because these scripts explain
            # every `rm -rf` in prose directly above it and one of those
            # sentences quotes the command. Read whole-file, this counted the
            # explanation as the deletion — so commenting the real line out left
            # the check still reporting a tree it was no longer told about, which
            # is the "lint that cannot find what it checks" this function's own
            # empty-set guard exists to refuse.
            bare = line.strip()
            if bare.startswith("#"):
                continue
            for target in re.findall(
                r'rm -rf "\$dist/([A-Za-z0-9_./-]+)"', bare
            ):
                found.add(f"dist/{target.split('/')[0]}")
    return found


def confirm_prompts_spelled_by_hand() -> list[tuple[str, int, str]]:
    """Every `pad_hint` call that spells the confirm keys instead of naming them.

    `PAD_CONFIRM_KEYS` in `src/pad_hint.h` is the one spelling of the two keys
    `state_accepts_confirm` accepts, and the note above it records what happened
    without it: nine prompts reporting one fact in four spellings, two of them
    naming fewer buttons than actually work, on screens a player sees within a
    minute of each other. The seam is gone, so nothing can drift — but a
    *tenth* prompt can still arrive written out by hand, which is how all nine of
    those happened.

    Two shapes are caught and they cover the two ways such a prompt is written:

    - a pad form naming `$START`, which is the confirm button and nothing else,
      so the call is reporting the confirm pair whatever its wording; and
    - a key form naming both ENTER and SPACE, which is that pair spelled out,
      however the pad half is written. The drive's prompts are this shape: their
      pad form is `$Y`, because A and B are the pedals there.

    Deliberately *not* "a key form mentioning ENTER". `PRESS $Y TO ENTER WC` and
    `PRESS $Y TO ENTER DOOR` are the verb, not the key, and a check that cannot
    tell those apart is a check somebody turns off.
    """
    found: list[tuple[str, int, str]] = []
    for source in sorted(SRC.glob("*.c")):
        text = source.read_text()
        for match in re.finditer(r"\bpad_hint\s*\(", text):
            # The call's arguments, to its closing bracket. One nesting level is
            # enough — `sizeof(hint)` is the only call inside any of these.
            depth = 1
            at = match.end()
            while at < len(text) and depth > 0:
                if text[at] == "(":
                    depth += 1
                elif text[at] == ")":
                    depth -= 1
                at += 1
            call = text[match.end() : at - 1]
            if "PAD_CONFIRM_KEYS" in call:
                continue
            literals = re.findall(r'"((?:[^"\\]|\\.)*)"', call)
            spelled = " ".join(literals)
            names_start = "$START" in spelled
            names_pair = re.search(r"\bENTER\b", spelled) and re.search(
                r"\bSPACE\b", spelled
            )
            if names_start or names_pair:
                line = text.count("\n", 0, match.start()) + 1
                found.append((source.name, line, spelled))
    return found


def workflow_release_paths() -> list[tuple[str, str]]:
    """Every `dist/...` path the workflows name, with the file it came from."""
    found: list[tuple[str, str]] = []
    for flow in sorted((ROOT / ".github" / "workflows").glob("*.yml")):
        for line in flow.read_text().splitlines():
            bare = line.strip()
            if bare.startswith("#"):
                continue
            for path in re.findall(r"dist/[A-Za-z0-9_*./-]+", bare):
                found.append((flow.name, path))
    return found


def main() -> int:
    failures = 0
    checks = 0

    # --- a step that reads what the step before it deletes --------------------
    #
    # Not a list, and it is here for the same reason the lists are: it is one
    # fact in two files with nothing holding them together, and it had already
    # cost a release.
    #
    # `payloads.yml` used to start its smoke test with
    # `ls -d dist/stage/Chuck-*-linux-x86_64`, and the last thing
    # `build_linux.sh` does before printing "upload this" is
    # `rm -rf "$dist/stage"`. So the step died on `ls: cannot access` every time
    # it ran, and it took a manual dispatch to find out, because that workflow
    # starts on nothing. AGENTS.md already has this defect written up on the
    # other platform — the macOS zip whose prerequisite opened with
    # `rm -rf "$app"` and destroyed the notarization ticket it was packing — and
    # the lesson recorded there is that the fix is to stop consuming the
    # intermediate, not to guard the order. Both workflows consume the archive
    # now.
    #
    # The direction is one-way on purpose. A workflow naming `dist/` at all is
    # ordinary and most of those names are the archives, which is the point; what
    # cannot be right is a workflow reaching into a tree whose own script removes
    # it. And the check derives the doomed paths from the `rm -rf` lines rather
    # than knowing the word "stage", so a rename moves both halves at once.
    checks += 1
    doomed = staging_paths_scripts_delete()
    referenced = workflow_release_paths()
    if not doomed:
        fail(
            "the staging trees",
            "no `rm -rf \"$dist/...\"` found in packaging/build_*.sh, so this "
            "check read nothing\n"
            "a lint that cannot find what it checks is worse than no lint",
        )
        failures += 1
    else:
        reaching = sorted(
            {
                (flow, path)
                for flow, path in referenced
                for dead in doomed
                if path == dead or path.startswith(dead + "/")
            }
        )
        if reaching:
            detail = "\n".join(
                f"  {flow} reads {path}" for flow, path in reaching
            )
            fail(
                "the staging trees",
                f"a workflow consumes an intermediate the packaging script "
                f"deletes ({sorted(doomed)!r}):\n{detail}\n"
                "the step cannot ever have run — consume the archive instead, "
                "which is what a player downloads anyway",
            )
            failures += 1

    # --- how a player says yes, in the one place it is spelled ---------------
    #
    # The keys are a `#define` now rather than nine literals, so the drift that
    # happened cannot happen again — this is the guard against a *tenth* prompt
    # arriving spelled by hand, which is how all nine of the first ones did.
    #
    # It is one-way and it has to be: a prompt is free to name whatever button
    # its own screen answers, and most of them do. What cannot be right is a
    # prompt reporting the *confirm* pair in words of its own while the file whose
    # whole job is spelling that pair sits one include away.
    checks += 1
    by_hand = confirm_prompts_spelled_by_hand()
    if by_hand:
        detail = "\n".join(
            f"  {name}:{line}: {spelled}" for name, line, spelled in by_hand
        )
        fail(
            "the confirm prompt",
            f"a prompt spells the confirm keys instead of naming "
            f"PAD_CONFIRM_KEYS:\n{detail}\n"
            "nine prompts once reported that pair in four spellings — see the "
            "note above PAD_CONFIRM_KEYS in src/pad_hint.h",
        )
        failures += 1

    # --- what SDL needs installed, in its two copies -------------------------
    #
    # `packaging/build_linux.sh` looks for the headers before it spends the clone
    # and names the apt package for each one it cannot find;
    # `.github/workflows/payloads.yml` installs them. Two copies of one fact, and
    # the fact itself belongs to neither file — it is what SDL's cmake demands,
    # which lives in somebody else's tree and cannot be checked from here.
    #
    # What *can* be checked is that these two agree, and the direction that
    # matters is one-way. A package the preflight names and the workflow does not
    # install is a job that dies on a missing header after the preflight has told
    # it exactly which one — the preflight working and the release still not
    # happening. The reverse is fine and deliberate: the workflow installs
    # Wayland, mesa, ALSA and Pulse as well, none of which the preflight has an
    # opinion about.
    #
    # This exists because `libxtst-dev` was in neither list and the job died on
    # `Couldn't find dependency package for XTEST`, and `libxrender-dev` was in
    # neither either while being present the whole time as a dependency of
    # `libxcursor-dev` — a break waiting on an apt resolution nobody controls.
    checks += 1
    needed = preflight_packages()
    installed = workflow_packages()
    if not needed or not installed:
        fail(
            "what SDL needs installed",
            "one of the two lists came back empty, so this check read nothing\n"
            "a lint that cannot find what it checks is worse than no lint",
        )
        failures += 1
    else:
        uninstalled = needed - installed
        if uninstalled:
            fail(
                "what SDL needs installed",
                f".github/workflows/payloads.yml does not install "
                f"{sorted(uninstalled)!r}, which preflight_x11 in "
                f"packaging/build_linux.sh says SDL will not configure "
                f"without\n"
                "the payload job fails at the preflight, which is the check "
                "working and the release still not happening",
            )
            failures += 1

    # --- the store page, in its two copies -----------------------------------
    checks += 1
    page_md = ROOT / "itch" / "page.md"
    page_html = ROOT / "itch" / "page.html"
    if not page_md.is_file() or not page_html.is_file():
        fail("the store page", "itch/page.md or itch/page.html is missing")
        failures += 1
    else:
        words_md = store_page_words(page_md, html=False)
        words_html = store_page_words(page_html, html=True)
        if not words_md or not words_html:
            fail(
                "the store page",
                "one of the two copies came back empty, so this check read "
                "nothing",
            )
            failures += 1
        else:
            diff = [
                (tag, " ".join(words_md[i1:i2]), " ".join(words_html[j1:j2]))
                for tag, i1, i2, j1, j2 in SequenceMatcher(
                    None, words_md, words_html
                ).get_opcodes()
                if tag != "equal"
            ]
            if diff:
                detail = "\n".join(
                    f"  {tag}: page.md {md[:70]!r} vs page.html {ht[:70]!r}"
                    for tag, md, ht in diff[:5]
                )
                fail(
                    "the store page",
                    "itch/page.md and itch/page.html no longer say the same "
                    "thing\n" + detail,
                )
                failures += 1

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
        # And the sentence the switch prints when it refuses a name, which is
        # the copy a caller actually reads and the copy nothing checked.
        listed = main_screen_message_names()
        if not listed:
            fail(
                "the soak screens",
                "--screen's refusal message could not be read out of main.c, so "
                "this check read nothing",
            )
            failures += 1
        elif listed != known:
            fail(
                "the soak screens",
                f"--screen's refusal names {sorted(listed)!r} and the game "
                f"answers to {sorted(known)!r}\n"
                "a message about which names exist that is wrong about which "
                "names exist is worse than no message",
            )
            failures += 1
        # And the page that tells a reader which screens exist, which is the
        # fourth copy and was stale by two the day it was first looked at.
        written = tooling_page_screen_names()
        if not written:
            fail(
                "the soak screens",
                "docs/tooling.md's list of screen names could not be read, so "
                "this check read nothing",
            )
            failures += 1
        elif written != known:
            fail(
                "the soak screens",
                f"docs/tooling.md names {sorted(written)!r} and the game "
                f"answers to {sorted(known)!r}\n"
                "the page a reader goes to for the list is the one copy of it "
                "nobody greps",
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
        # "across two" for as long as there were three, and then four. The
        # count is the whole subject of this line, so it is counted rather than
        # typed: the game's dispatch, the sweep's array, the switch's own
        # refusal, and the page a reader goes to for the list.
        f"{len(known)} soak screens across four, "
        f"{len(table)} command-line switches, "
        f"{len(needed)} SDL build packages across a script and a workflow, "
        f"the store page in both of its copies, "
        f"the two designated tables no test can reach, "
        f"no workflow reading any of the {len(doomed)} staging tree(s) "
        f"its own script deletes, "
        f"and no prompt spelling the confirm keys by hand — all agree"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
