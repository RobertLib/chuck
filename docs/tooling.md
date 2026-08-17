# The editor and the shipped app

## The level editor

`make editor` builds `./chuck-editor` from [editor/](../editor/). It is a separate
binary, but deliberately not a separate idea of what a level is: it links
[level.c](../src/level.c) to parse the map, [level_art.c](../src/level_art.c) to draw
it, and [level_route.c](../src/level_route.c) to judge it. What the canvas shows is
what the game will show, and what the report says is what `make test` will say.
An editor with its own parser and its own opinion of "solvable" would be a
second source of truth about the campaign, and the one that is wrong would be
the one being used.

Four modules, and the split is by what needs SDL:

- [editor_doc.c](../editor/editor_doc.c) — the document: a map _as characters_,
  not as a parsed `LevelMap`. A file says things a `LevelMap` cannot say back —
  a space against a `.`, a decoration the loader drops, an absent `THEME` line —
  so the editor keeps the text and hands it to `level_load_data` to find out
  what it means. Undo is two stacks of whole-grid snapshots.
- [editor_legend.c](../editor/editor_legend.c) — every character in
  [levels/LEGEND.md](../levels/LEGEND.md) as a table: name, the sentence the legend
  gives it, colour, which mode it belongs to. **Both files change together**;
  a character in one and not the other is either an unpaintable tile or a typo
  the editor calls an error — and `check_lists.py` is what says so now rather than
  that sentence, because the legend, this table and the parser in
  [level.c](../src/level.c) are three copies of one list and were held together by
  nothing but a reader's diligence.
  The `group` field is also load-bearing rather than a filing detail: a symbol's
  bin is what decides which palette it is painted from, and the prop bins are what
  `editor_validate.c` counts against `MAX_DECORATIONS`. That sum used to be
  fifteen `count_of` calls — a fifth copy of the prop list, already behind the
  plant set the day it was added — so a floor painted past the ceiling in pallets
  was one the editor called clean while the loader dropped the overflow. It reads
  the bins now, so a sixth prop set is counted by having been given one.
- [editor_validate.c](../editor/editor_validate.c) — the report. Structure the
  loader insists on, the caps in [game_config.h](../src/game_config.h), the
  authoring rules in the legend, the route model, and the campaign-wide rules
  `test_all_embedded_levels_parse` and
  `test_campaign_levels_are_distinct_and_solvable` pin.
- [editor_app.c](../editor/editor_app.c), [editor_ui.c](../editor/editor_ui.c),
  [editor_render.c](../editor/editor_render.c) — SDL: state and input, the chrome,
  the canvas.

The first three have no SDL in them, so `TEST_SOURCES` links them and the suite
pins two things the editor cannot be allowed to get wrong. `test_editor_round_trips_every_map_file`
requires that loading and saving every shipped map leaves the file byte
identical — the moment saving reflows a map, editing one sector rewrites it
wholesale and buries the actual change in the diff. `test_editor_report_reads_the_campaign`
requires that the editor reports zero errors for every sector already in the
tree, which is what keeps its rules and the test suite's rules the same rules;
`test_the_editor_has_nothing_to_say_about_the_shipped_campaign` goes further and
requires zero **warnings** as well, since a warning is precisely the class
nothing else can see — "it loads, but it will not play the way it reads".

**And a save goes through a temporary and is moved onto the map**, which is the
one thing in this tree nobody can regenerate. `editor_doc_save` opened the target
with `fopen(path, "wb")` for as long as it existed, and that truncates before the
first byte is written: a save interrupted by a full disk, a lost volume or a
crash left the author holding a *shorter* map than the one they opened, and the
whole point of the tool is that the map is somebody's afternoon. It writes
`<path>.tmp` and renames it now — atomic inside a directory, so the file is
either the map that was there or the whole of the new one — and removes the
temporary on every failing path, including a failed rename, because a stale
half-map sitting in `levels/` is a map as far as the editor, the embed step and
`check_docs.py` can tell. The settings and the progress files are written the
blunt way still, and stay that way: those are what the game can work out again.
`test_editor_resizes_deletes_and_survives_a_real_file` saves twice over the same
path, reopens it, and requires the second map and no leftovers.

**Both of those tests have to hand the editor the map's path**, and one of them
did not. `editor_validate` takes the sector number off `doc->path`, because that
is how the editor knows which slot of the campaign the map on screen is, and
`editor_doc_parse` memsets the whole document — so a test that parses and
validates without setting the path is asking about a map that belongs to no
sector, and the entire cross-sector block is skipped. That is what the
warnings-and-notes test was doing over all seventeen maps, with a comment above
the loop saying otherwise. Two things had been sitting in the skipped half, both
of them notes: the editor's own campaign constants had stayed at fifteen sectors
and four climbs, so the tool told every author that the shipped campaign
disagreed with the tests, and sector 14's flash charge sat on a partition five
rows above its floor where the route model cannot reach it. The constants read
`CAMPAIGN_SECTORS` and `CAMPAIGN_CLIMB_SECTOR_COUNT` out of the game now, which
is why `manual_pages.c` is in `EDITOR_SOURCES`, and an unreachable pickup is a
warning rather than a note.

`F5` saves, runs `make` and launches `./chuck --level N`. That switch
([main.c](../src/main.c)) and `game_start_at_level` are the whole of the game-side
change; the debug level picker calls the same entry point.

**The build runs beside the editor, not inside it.** `make` takes seconds, and
it used to be run straight from the keypress that asked for it — so the window
stopped answering the moment the author pressed playtest, drew nothing, and
collected a spinning cursor; the status line said `Building...` and was the one
thing on screen that could not be repainted to say it. It is a thread now
(`build_thread` / `ed_update_playtest` in
[editor/editor_app.c](../editor/editor_app.c)), and two things about the split
are deliberate. The read stays *blocking*, on that thread: draining the pipe as
`make` writes it is what stops a build failing with a screenful of errors from
filling the buffer and wedging against a reader that is off drawing a frame.
And the thread touches nothing of the app's — it fills an `EdBuild` and sets an
atomic as its last act, and the main loop joins it and does everything that
reaches the screen or launches anything, because a status line written from two
threads is a status line that is sometimes half of each.

## The headless soak

`make soak` runs a built game across the title screen, every campaign sector,
every screen that is reached by a choice rather than by play, and the level
editor, headless, and fails on anything any of them says.
[tools/soak.sh](../tools/soak.sh) is the whole of it; `SOAK_BINARY` chooses what
to run, `SOAK_EDITOR` the editor beside it, and `SOAK_SECONDS` how long to hold
each screen.

`SOAK_MODE` is the fifth knob and it is there because the sweep was being run
twice — once under the sanitizers and once on macOS — and only one of those two
runs was paying for what the length of it buys. The expensive half is the timed
sequences: the drive, the two prologue beats, the report, the countdown, the
ending and the roll of names are held for as long as the game's own constants say
they last, which is most of the sweep's five minutes, and the argument for that
is a **coverage** argument — it is how `cutscene.c` and `chase_render.c` are
executed rather than merely linked, measured once in a coverage build. That
argument does not double when the same code is drawn again on another platform.
So `full` is the sweep, `smoke` walks everything that holds still — the title
screen, all seventeen sectors, every manual sheet, every aftermath pose, every
room and card, and the editor — and holds no timed sequence at all. It prints the
sequences it skipped by name on the way out, and the summary line counts the
screens it *walked* rather than the length of the list it was handed, because a
mode that quietly covers less than the mode beside it is the defect this whole
page is about.

**And it is worth knowing what the macOS run does and does not cover**, because
the reason it was there said Cocoa and Cocoa is the one thing it never touches:
the script exports `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy` for every
run on every platform, so both jobs draw through the software renderer and neither
opens a window or an audio device. What the macOS sweep uniquely covers is
Homebrew's SDL3 dylib and the whole tree as Apple clang compiles it for arm64 —
which is worth a sweep, and is what `smoke` is.

**It exists because the sanitizers were not reaching the half of the tree they
were named for.** `make sanitize` rebuilt everything with ASan and UBSan and then
ran `core_tests` — which links no SDL, by design, because the gameplay core is
meant to be testable without one. So the sanitized *game* was compiled and never
started. The CI job could not have started it either: it builds SDL with
`-DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_UNIX_CONSOLE_BUILD=ON`, deliberately, since
nothing in that job had ever opened a window. Between them, those two reasonable
decisions left [game_render.c](../src/game_render.c),
[level_art.c](../src/level_art.c), [cutscene.c](../src/cutscene.c),
[render_figures.c](../src/render_figures.c), [audio.c](../src/audio.c),
[intro.c](../src/intro.c), [manual.c](../src/manual.c) and
[chase_render.c](../src/chase_render.c) — more than half the tree by source size —
sanitizer-compiled and never sanitizer-executed.

It was all clean when the sweep was finally written, and that is the part worth
keeping in mind: this did not pay off a bug. Nothing had been asking, so there was
no telling which way it would go, and a gate that has never been reached is not
evidence of anything. It is the same shape as the fit checks on the sheets of
words, two of which found a line already lost the day they were written.

Three details are what make it a check rather than a gesture.

- **The dummy video driver.** `SDL_VIDEODRIVER=dummy` gets a window with no
  screen behind it and SDL falls back to the software renderer, which really does
  rasterize — a sanitized sector burns most of a core while this runs, so the draw
  calls are being executed and not merely linked. SDL always builds the dummy
  driver, which is why this works on the CI job that has no real backend at all.
- **`--soak N` rather than a `kill`.** A killed process never reaches
  `SDL_AppQuit`, and teardown is exactly the half of a lifecycle a sanitizer is
  most likely to have something to say about. The game spends a wall-clock budget
  and closes itself, so the script can read an exit status. The budget is paid in
  *raw* elapsed time rather than the `MAX_FRAME_DT` clamp: a sanitized frame can
  outlast the clamp, and a budget paid in clamped time would turn `--soak 2` into
  two minutes.
- **The title screen is soaked separately.** `--level N` skips the title screen
  and the prologue outright, so a sweep of sectors alone never draws
  [intro.c](../src/intro.c). A new screen owes this sweep a thought about
  whether any run reaches it.

**And "reached" has to be asked of the run rather than of the screen, which took
a second pass to learn.** The bullet above used to end "...never draws
`intro.c` or either prologue cutscene", and the sweep's own header said the
title screen was "the only entry point that reaches `intro.c`, the prologue's
cutscenes and the attract music". It never reached the cutscenes.
`STATE_INTRO` advances on `game->input.confirm`, every line that sets that flag
is inside an SDL event handler, and a headless process receives no events — so
the soak sat on the first screen for its whole budget while two comments said
it had walked the prologue. A forty-second run settles it: the process reports
finishing without having left the title screen.

That is the same failure the sweep was written to end, one floor up: a check
reporting coverage it did not have. What it left compiled-and-never-executed is
[chase_render.c](../src/chase_render.c) entire, most of
[cutscene.c](../src/cutscene.c) — the report between sectors, the outro, the
roll of names, the abduction — [manual.c](../src/manual.c), and the settings,
pause, continue and game-over halves of
[game_render.c](../src/game_render.c), plus the four restroom sublevels that no
`--level N` run enters either.

- **`--screen NAME` is the answer**, and it is a switch rather than synthesised
  keypresses for the reason `--soak` is a switch rather than a `kill`: a screen
  reached by three fake button presses is a screen whose coverage breaks the day
  a menu gains a row, and what would be under test is the event handlers rather
  than the renderers. `game_soak_screen` in [game.c](../src/game.c) holds the
  list — `abduction`, `chase`, `opening`, `manual`, `settings`, `pause`,
  `report`, `cleared`, `continue`, `gameover`, `outro`, `credits`, `restroom` —
  and `check_lists.py` holds it against the array in
  [tools/soak.sh](../tools/soak.sh), because those are two copies of one list and
  the direction that fails silently is the dangerous one: a screen the game knows
  and the script does not is compiled under the sanitizers, never run by them, and
  the sweep still reports clean.
- **`--page N` is the same argument one level in.** `manual` is a single screen
  name standing for ten sheets, each with an illustration of its own, and nothing
  turns a sheet but a hand — so the sweep drew `illus_night` and the nine drawings
  behind it were never executed. The script walks every sheet now, with the count
  read out of `MANUAL_PAGE_COUNT` rather than written down, so a new sheet is
  soaked by having been added.
- **The editor is soaked too**, and it was the last binary in the tree nothing
  ran. `make sanitize` builds `all test soak` and `all` is the game, so
  [editor_app.c](../editor/editor_app.c),
  [editor_render.c](../editor/editor_render.c) and
  [editor_ui.c](../editor/editor_ui.c) were not merely unexecuted under the
  sanitizers, they were never compiled under them; the macOS CI job built the
  editor and then did nothing with it. What made that hole hard to see is that
  the three editor translation units the suite *does* link —
  [editor_doc.c](../editor/editor_doc.c),
  [editor_legend.c](../editor/editor_legend.c) and
  [editor_validate.c](../editor/editor_validate.c) — are exactly the ones that
  touch no SDL. It has its own `--soak N` now, and `make sanitize` builds it.

The campaign's length is counted out of `levels/` rather than written down here,
for the reason every other count in this tree is: a literal seventeen in a shell
script would go stale on the same day as all the others.

## The shipped macOS app

`make app` builds `dist/Chuck.app`; `make notarize` gets it a ticket from Apple
and cuts the DMG. Everything either one needs is in [packaging/](../packaging/),
and the whole of it exists to close the gap between a binary that runs *here*
and a binary that runs on somebody else's Mac. Four decisions carry it.

**`make` and `make app` do not link the same SDL, and they must not.** The
development build takes Homebrew's, which is right for a machine with Homebrew
on it and wrong for everybody else's: it is arm64 only and it is built for the
macOS it was poured on — `minos 26.0` as this was written — so a bundle wrapped
around it starts on this Mac and refuses to launch on any other, with an error
that names nothing the player can act on. The app is therefore built against
libsdl.org's own universal `SDL3.framework`, fetched into `vendor/` by
[packaging/fetch_sdl3.sh](../packaging/fetch_sdl3.sh) with **the version and its
sha256 pinned in the script**: a shipped binary has to be traceable to the
library it was linked against, and "whatever the latest release was that day"
is not that. That framework carries both slices and a macOS 11 floor, which is
what `LSMinimumSystemVersion` is then allowed to say.

**The app is self-contained, and SDL travels inside it.** The framework's
install name is `@rpath/SDL3.framework/Versions/A/SDL3` and the link step writes
`@executable_path/../Frameworks` into the binary, so bundling is a copy and no
`install_name_tool` surgery — which also means there is no path to a Homebrew
directory left anywhere in the shipped Mach-O to work by accident on the
developer's machine. The levels are already in the executable and the audio is
synthesized at startup, so `Contents/Resources` holds nothing but the icon.

**The build always signs, and it says which of the two ways it signed.** With a
*Developer ID Application* certificate in the keychain it signs with that, under
the hardened runtime (`--options runtime`) and a secure timestamp, which is
what notarization requires. With no such certificate it signs ad-hoc and says
so in as many words, because an unsigned build that prints nothing looks exactly
like a build that succeeded until somebody else double-clicks it. An *Apple
Development* certificate is not a substitute: it signs for your own devices and
Apple will not notarize it. [packaging/notarize.sh](../packaging/notarize.sh)
refuses to upload anything not signed with a Developer ID and prints how to get
one, rather than letting Apple reject it twenty minutes later.

**Both the app and the DMG are notarized and stapled.** A player may be handed
either, and Gatekeeper checks whichever they got; stapling writes the ticket
into the bundle so the first launch needs no network. Credentials live in a
notarytool keychain profile (`xcrun notarytool store-credentials`), never in the
repository.

The icon is drawn, not stored: [packaging/draw_icon.py](../packaging/draw_icon.py)
paints the tower, its lit floors, the roof beacon and Chuck on the flank out of
the fx.h palette on a 128x128 grid, which is the same reason the levels' art is
procedural — the repository holds no binary assets, and an icon checked in as a
blob is one more thing that can drift from the palette everything else is drawn
in.

The version, the bundle identifier and the app name are written once, in
[version.h](../src/version.h): the binary hands them to `SDL_SetAppMetadata` (so
the audio device, the window's owner and a crash report all name the game
rather than "SDL Application") and `build_app.sh` greps them out of the same
header for `Info.plist`. `CFBundleVersion` is the commit count, which is a
number that already rises with every build anybody is handed.
