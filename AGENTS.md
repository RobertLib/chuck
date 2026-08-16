# AGENTS.md

## Project

Chuck is a 2D action platformer written in C17 against SDL3. All art is drawn
procedurally at runtime and all audio is synthesized at startup, so the shipped
executable has no external asset files — levels are embedded into the binary at
build time.

## Commands

```sh
make          # build ./chuck
make run      # build and launch
make debug    # build build/debug/chuck-debug (-O0 -g3 -DCHUCK_DEBUG)
make run-debug  # build and launch it; its title screen has the level picker
make editor   # build ./chuck-editor, the level editor
make run-editor # build and launch the editor
make test     # lint, then build and run the core test suite (build/core_tests)
make lint     # tools/check_palette.py: no literal may respell an fx.h colour
make sanitize # rebuild game + tests with ASan/UBSan into build/sanitize
make smoke    # sanitize, then boot the real binary through every sector, screen and manual sheet
make app      # build dist/Chuck.app, universal and signed (macOS)
make notarize # notarize and staple it, and cut dist/Chuck-<version>.dmg
make clean    # remove build/, dist/, ./chuck and ./chuck-editor
```

The debug build is the only one with the level picker on its title screen
(`</>` or `[`/`]` to choose, `F5` to start there); the release build has no
`CHUCK_DEBUG` in it at all.

`./chuck --level N` boots straight into campaign sector N, skipping the title
screen and the prologue; it is what the editor's playtest button launches.

`./chuck --scene NAME` opens one screen directly — `abduction`, `drive`,
`arrival`, `manual`, `options`, `controls`, `report`, `cleared`, `continue`,
`gameover`, `pause`, `outro`, `credits`, `restroom` — and is read after
`--level`, so `--level 9 --scene report` draws the report sector nine would
have handed over.
It is an authoring and testing entry point beside `--level`, not a campaign
path: it banks nothing, and every name is a transition the game itself makes
rather than a state assembled by hand. `restroom` is the one that needs a
sector to stand on (`--level 1`, `5`, `9` or `14`), because it is a swap rather
than a transition: it goes through the same `enter_restroom` the `U` door does
and refuses a sector that has no door to swap through. `make smoke` is what it
is for; see below.

`./chuck --page N` opens the manual on sheet N (1-based, and read after
`--scene`, so `--scene manual --page 6` is how a sheet other than the first is
looked at). The book is the one screen `--scene` cannot cover on its own: it is
eight sheets, each with an illustration of its own, and a sheet is only ever
turned by a hand — so a run that presses no keys draws the first and none of
the other seven. It refuses a page that does not exist and a `--page` with no
manual open, for the reason `--scene restroom` refuses a sector with no door.

`./chuck --level N --demo` drives that sector with a scripted hand instead of a
player's, and it is the fourth entry point rather than a fifth `--scene` name
because **`--scene` opens a screen and this drives a state**. Booting a sector
executes a sector standing still: everything behind the player *acting* — the
downed bodies, the crawl, the hacking pose, both bazooka poses, the muzzle
flash, both rocket sprites, the alarm lighting — was executed by nothing in the
tree at all, which `llvm-cov` over a smoke run put at twelve live drawing
functions flat at zero. It keeps `--scene`'s own rule: nothing is assembled by
hand, the game is played into those states. It takes no value, is read last
because it drives whatever the three switches above opened, and refuses
anything that is not a sector. The script is [demo.c](src/demo.c), it is
SDL-free so `make test` holds it, and one lap is two halves — the floor, walking
at the live terminal, and the ladder, because a vertical shot is only ever fired
from a rung.

SDL3 must be discoverable through `pkg-config`. The **test binary links no SDL**
(`TEST_CFLAGS` omits the SDL flags), so `make test` works even where SDL3 is
unavailable, and it runs in well under a second.

There is no test filter: `tests/test_main.c` is one binary whose `main()` calls
every `test_*` function in sequence. To run a single test, temporarily comment
out the others in `main()`, or just run the whole suite. Failures are reported
by the `CHECK` macro as `file:line: check failed: <expr>` and the process exits 1.

**`make smoke` is the other half of the suite, and it exists because of what
the first half structurally cannot reach.** `make test` links no SDL, which is
what makes it fast and portable and also means it never touches the renderers,
the level art or the audio synthesis — about 24k of the tree's 56k lines. (Both
figures are measured rather than pinned, so read them as a proportion and not to
the digit; they said 19k of 49k for a while after they had stopped being true.)
`make sanitize` builds that half under ASan/UBSan and then ran only the core
suite, so the instrumented binary it had just produced was never executed at
all. `make smoke` executes it: the title screen, then every campaign sector in
turn, then every screen named by `--scene` and all eight sheets of the manual,
against SDL's dummy video and audio drivers so it needs no display and no sound
card. It asserts nothing about what was drawn — what it catches is the class the
core suite cannot see, a bad index in a draw loop or a read past a theme table,
which ASan says out loud the moment the code actually runs. Any output at all
from the binary is a failure, because a clean run of this game is silent.

**A dwell is not a coverage claim, and that is the third time this target has
had to learn its own lesson.** `SMOKE_SECONDS` used to be the dwell for
everything, which is right for a screen that is a *still* and wrong for every
screen that is a *clock*: the outro runs twenty-seven seconds, the roll of names
thirty-odd, the drive over a minute, and three seconds of each executed the
frame it opens on and reported the whole screen as covered. So the outro's
closing card, the credits' own last card and the drive's arrival were in exactly
the state the credits skyline had been in before this target existed — drawn by
code nothing in the tree ever ran — while the list of scene names said
otherwise. Each timed scene now dwells for its own length, read by
[tools/smoke.sh](tools/smoke.sh) out of the header that owns the number rather
than copied into the script, the way CI reads the pinned SDL version out of
`packaging/fetch_sdl3.sh`. `SMOKE_SECONDS` is now the floor — how long to sit
on a still — and cannot cut a clock short. The roll of names is the one whose
length is a property of its table rather than a constant, so `credits.h` states
a ceiling (`CREDITS_MAX_DURATION`) instead and
`test_credits_fit_the_frame` holds the table under it: a roll that outgrew the
dwell fails the build rather than quietly outliving the only thing that runs it.
**A new screen with beats after its first owes this file the same treatment** —
a constant to wait on, and a check keeping it honest.

**And a sector is a fourth shape, which took measuring to see.** Every scene
above is a screen, and the fifteen sector boots looked like the part of this
target that was already thorough — fifteen maps, every theme, every renderer.
They are fifteen maps *standing still*: the run presses no key, so the sector
executed is one where Chuck never draws a weapon, never crouches, never kills
anybody and is never seen. Held under `llvm-cov`, that left
[render_figures.c](src/render_figures.c) at 58% and
[game_render.c](src/game_render.c) at 77% — the only two renderers `make test`
structurally cannot link — with twelve live functions at nought, the downed
bodies among them, which the whole body-discovery rule in
[gameplay.md](docs/gameplay.md) is built on being visible. `--demo` is what
runs them, and every sector is now smoked twice: once still and once played.
**Anything a player reaches by pressing a button owes itself a beat in
[demo.c](src/demo.c)**, the way anything they reach by playing owes itself a
`--scene` name. The way to find out is to measure rather than to reason about
it — a coverage build and this script is an afternoon, and it has now been the
answer four times running.

**And the manual is a third shape again**, needing a switch rather than a clock:
nothing turns a sheet but a hand, so seven of its eight illustrations — some six
hundred lines of drawing, plus the figure helpers only they reach — were run by
nothing at all, in a file `make test` cannot link. What made that easy to miss
is that the *words* of all eight sheets are measured by the suite
(`test_manual_sheets_fit_the_column`): the half everybody checks was checked and
the picture beside it was not. `--page N` is what reaches them.

**And it has no gamepad either, for the same reason it has no display.** A run
has to say the same thing whatever is plugged into the machine it is on.
`open_gamepad` logs the pad it found and which letter that pad confirms with —
a correct and useful line, and under the rule above also a failure, so every
scene failed on any desk with a controller attached while CI, which has none,
stayed green. That is the worst shape a check can have: it fires everywhere
except where it would be read. `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT` names
a vendor and product that cannot exist, which is how SDL is asked to ignore all
of them.

**The scene list is the half this target spent a long time missing, and the
lesson is the same one the SDL-free word tables keep teaching.** Booting a
sector executes the sector; it does not execute anything only arrived at by
*playing* through one. So the two prologue cutscenes, the drive, the manual, the
options sheet, the report between sectors, the outro and the roll of names —
`cutscene.c`, `manual.c`, `chase_render.c` and the credits roll, something like
a fifth of the presentation code — were run by nothing in this tree at all,
while the paragraph above claimed the target covered the renderers. The first
run that reached them found undefined behaviour in the credits skyline: a
negative float cast to `unsigned`, in every frame of the one screen a finished
campaign always ends on, resolving differently on the two slices of the shipped
universal binary. A screen nothing boots is a screen nothing checks; anything
new that the player reaches by playing owes itself a `--scene` name here.

**The restroom is the same lesson, collected a second time.** A `U` is only
opened by walking into it, so booting a sector never drew the room behind it and
nothing in the tree did either — which was survivable while there was one room,
and stopped being survivable the moment there were four, each a different shape,
because a room's whole interior is derived from its own wall bounding box. The
four are smoked now, one per sector that carries the door.

**All of it runs in CI** ([.github/workflows/ci.yml](.github/workflows/ci.yml)),
in three jobs split by what each needs: `make test` on a runner with no SDL
installed at all — if that job ever needs one, the SDL boundary below has been
crossed and the failure is the point — then the sanitizers plus the smoke run,
then a macOS job that builds the game, the editor, the debug target **and the
shipped bundle** on the platform the game actually ships on. That last one is
`make app`, and it is there because none of the others builds it: `make` links
Homebrew's SDL for this arch and this macOS, while the app is two slices against
the vendored universal framework with an older deployment floor. Left out, a
break in the fetch, the `lipo`, the framework's rpath or the `Info.plist` read
out of [version.h](src/version.h) was only ever found by whoever was cutting a
release. No signing identity is needed for it — `packaging/build_app.sh` falls
back to an ad-hoc signature, which still proves the bundle assembles and
verifies. `make notarize` is the step that needs a real identity and stays a
local one.

## Architecture

### The SDL boundary

Two layers, and the split is the most important invariant in the codebase:

- **Application shell** (SDL-dependent): [main.c](src/main.c) (SDL callbacks),
  [game.c](src/game.c) (state machine, level loading, per-frame orchestration),
  [game_input.c](src/game_input.c), [game_render.c](src/game_render.c),
  [render_figures.c](src/render_figures.c),
  [render_sprite.c](src/render_sprite.c),
  [chase_render.c](src/chase_render.c), [audio.c](src/audio.c),
  [intro.c](src/intro.c), [manual.c](src/manual.c),
  [cutscene.c](src/cutscene.c), [pad_hint.c](src/pad_hint.c),
  [particle.c](src/particle.c). [crew.c](src/crew.c),
  [credits.c](src/credits.c), [manual_pages.c](src/manual_pages.c) and
  [intel.c](src/intel.c) sit on
  this side too and link no SDL, because they are tables of strings — but they
  are presentation, and no gameplay module may include them. See
  [The net](docs/story.md#the-net), [The credits](docs/story.md#the-credits),
  [The field manual](docs/screens.md#the-field-manual) and
  [The report between sectors](docs/story.md#the-report-between-sectors); each of the four
  is split out so the suite can hold its words to the frame they are drawn in.
  A table of words that stays inside its renderer is a table nothing measures,
  which is exactly how the manual's control sheet lost its last line and how
  the report's intel line sat for a long time with its ceiling written down as
  a sentence in a comment and nothing holding it there.
- **Gameplay core** (no SDL, no knowledge of `Game`): `src/gameplay_*.c`,
  [level.c](src/level.c), [level_route.c](src/level_route.c),
  [player.c](src/player.c), [enemy.c](src/enemy.c),
  [chase.c](src/chase.c), [rng.c](src/rng.c),
  [game_event.c](src/game_event.c). These only include each other plus libc.
  That is what makes them deterministic and directly testable.
- **Player state on disk**: [settings.c](src/settings.c),
  [keybind.c](src/keybind.c) and
  [progress.c](src/progress.c) are neither. They link no SDL and are held to a
  round trip through text by the suite, but they are the shell's own state, not
  the simulation's — the shell owns both files, because `SDL_GetPrefPath` is
  the only part of either that needs a platform. See
  [The options sheet](docs/screens.md#the-options-sheet) and
  [What outlives the process](docs/screens.md#what-outlives-the-process).
  [keybind.c](src/keybind.c) is the odd one of the three: its numbers *are* SDL
  scancodes, copied out of somebody else's header into a file that links none
  of it, so [game_input.c](src/game_input.c) asserts every one of them against
  `SDL_SCANCODE_*` at compile time — one assertion per row, generated from the
  same list the table is, so the check cannot fall behind the thing it checks.
  A number written down twice is checked or it is two numbers.

There is a second SDL binary, the level editor in [editor/](editor/); see
[The level editor](docs/tooling.md#the-level-editor) below.

Gameplay code never plays a sound, spawns a particle, or shakes the camera
itself. It appends to `GameplayState.events` (a `GameEventBuffer`, see
[game_event.h](src/game_event.h)) via `game_events_sound`,
`gameplay_world_sound`, `game_events_particles`, `game_events_explosion`,
`game_events_camera_shake`, `gameplay_crew_chatter`. The shell drains that
buffer once per frame in
`dispatch_events` ([game.c](src/game.c)) and turns events into audio and
presentation; the prologue pursuit reports its feedback through the same
function with its own buffer. Keep new gameplay feedback on this path — calling
`audio_play` from a gameplay module would both break the layering and break the
tests, which assert on emitted events.

## Where the rest of it is written down

This file is the part every session needs: what the project is, how to build it,
the one boundary that must not be crossed, and the conventions. Everything else
— the fiction, the systems, the screens, the maps, the art rules, the tools —
is in `docs/`, one page per question a reader actually arrives with.

That split is itself a rule worth keeping. This page is loaded in full at the
start of every session that touches the tree, so what earns a place in it is
what a reader needs *before* they know which corner of the game they are in.
The rest is reference, and reference is read on the way to a specific answer.
Twenty-six thousand words of it read up front is not context, it is ballast.
None of it was cut: the pages below hold the same sentences in the same order,
and a rule that changes still owes an edit everywhere it is written down.

| Page | What it answers |
| --- | --- |
| [The story](docs/story.md) | The night, the twelve names, the clock, and every table of words the player reads — the report between sectors, the crew's net, the credits, the props that are only true tonight, and the people who are not in the fight |
| [Architecture](docs/architecture.md) | `Game` composition, the frame's ordering and the collision invariants that ordering rests on, the RNG, and the three renderers |
| [Gameplay](docs/gameplay.md) | Hearts and the two real deaths, the stomp, the one blast rule, the forgiving jump and the checkpoints, the facade climb, the restrooms |
| [Screens](docs/screens.md) | The options sheet and its controls page, what outlives the process, the letter on every pad button, the prologue's three beats, and the field manual |
| [Levels](docs/levels.md) | What a map is, what a new sector owes the campaign, walls that open, one plan per sector, and the themes |
| [Art and audio](docs/art-and-audio.md) | The palette and its two rules, lighting, how a figure and a wall are built, the type rule, the sound and music tables |
| [Tooling](docs/tooling.md) | The level editor, and the shipped, signed, notarized macOS app |

[levels/LEGEND.md](levels/LEGEND.md) is the other reference page and did not
move: it is every map character, the authoring rules the geometry has to
respect, and a table of all fifteen plans.
## Conventions

- C17, built with `-Wall -Wextra -Wpedantic`; the tree is warning-free, keep it
  that way. `make sanitize` should stay clean too.
- Allman braces, 4-space indent, `CHUCK_*_H` include guards, `/* */` comments
  used to explain _why_ a rule exists rather than restating the code.
  **The renderers are legacy 2-space** and are named here rather than described,
  because "one legacy file" was written down while there were five of them and
  a reader following the sentence reformatted whichever one they opened first:
  [game_render.c](src/game_render.c), [render_figures.c](src/render_figures.c),
  [render_sprite.c](src/render_sprite.c), [particle.c](src/particle.c) and its
  header. [game_input.c](src/game_input.c) is the one that is *half* converted
  and is the only file in the tree with both styles in it. Match the file you
  are in; converting one is a commit of its own, not a line inside another
  change.
- **The simulation is stepped at a fixed `SIM_STEP_DT`**, not at the frame.
  `SDL_AppIterate` banks the real elapsed time and spends it in whole steps, so
  what the physics produces is a property of the game and not of the display it
  is drawn on. Fed the frame directly, as it used to be, the jump apex was
  68.7px at 240Hz, 71.0px at 60Hz and 77.4px at the `MIN_FRAME_RATE` floor —
  a third of a tile of difference in the one quantity every map is drawn
  against, which made a ceiling placed to cap a jump clearable on a slow machine
  and not on a quick one. `test_the_jump_apex_does_not_depend_on_the_frame_rate`
  runs that loop at ten refresh rates and holds them to a tenth of a pixel.
  Anything new that integrates against `dt` inherits this for free; anything
  reaching for wall-clock time instead does not, and owes a reason.
  **And a test of that code owes it `SIM_STEP_DT` rather than a round number.**
  A hand-picked `1.0f / 60.0f` is the rate the game does not run at, and the
  gap is not academic: climbing *down* a ladder was deadlocked on every map in
  the campaign — the grab needs the box to travel 1px into the rung and a
  240Hz step carries it 0.42 — while every test of it, written at 1/60 where
  one step carries 1.67, passed. A whole mechanic was broken in the shipped
  game and green in the suite. If a new check drives the simulation, drive it
  at the step the simulation uses, and at a couple of rates either side if the
  behaviour could be a distance rather than a rule.
- Adding a new gameplay `.c` file: the game build picks it up via
  `$(wildcard src/*.c)`, but `TEST_SOURCES` in the [Makefile](Makefile) is an
  explicit list — add the file there as well or the tests will fail to link.
  The editor wildcards `editor/*.c` but names the `src/` files it links, so a
  new dependency of the editor's goes in `EDITOR_SOURCES` too.
- Tests build levels from small inline map strings and drive the gameplay
  modules directly with a fixed seed, asserting on state and emitted events.
  New behavior in a gameplay module should get a test in that style.
- A **table of words** that the player reads — a sheet, a roll, a line on the
  net, a line on the report — goes in its own SDL-free file beside
  [crew.c](src/crew.c), [credits.c](src/credits.c),
  [manual_pages.c](src/manual_pages.c) and [intel.c](src/intel.c), with the
  geometry it has to fit written as constants the renderer lays it out from,
  and a `make test` check that measures it against them. Every time this has
  been skipped the result has been the same: a line nobody can see, in the part
  of the game that exists to be read.
  **[settings.c](src/settings.c) is the fifth of those files and was the one
  nobody had noticed was one** — it is a table of rows the player reads, it had
  its geometry in the renderer, and it had no fit check. It was already over
  the edge: the controls heading's detail line, the single sentence explaining
  how to rebind anything, ran about thirty pixels off the right of the plate.
  `SETTINGS_PANEL_W` and the rest now live in [settings.h](src/settings.h) and
  `test_every_word_on_the_options_sheet_fits_the_plate` walks every label,
  detail, strap and title on both pages. **The check is worth writing before
  you believe the sheet is fine**, because that is twice now that adding one
  has immediately found a line already lost.
