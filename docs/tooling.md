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
  the editor calls an error.
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
tree, which is what keeps its rules and the test suite's rules the same rules.

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
