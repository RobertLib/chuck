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
make lint     # check_palette.py: no literal may respell an fx.h colour
              # check_docs.py: the prose agrees with levels/*.txt
              # check_lists.py: a list written down twice agrees with itself
make sanitize # rebuild game + editor + tests with ASan/UBSan into
              # build/sanitize, run the suite, then soak both binaries
make soak     # tools/soak.sh: run a built game headless across the title
              # screen, all seventeen sectors and every screen reached by a
              # choice rather than by play, then the editor, failing on
              # anything any of them says. SOAK_BINARY, SOAK_EDITOR and
              # SOAK_SECONDS pick what and how long; SOAK_MODE picks which of
              # the two questions — `full` is the coverage sweep, `smoke`
              # holds no timed sequence and says which ones it skipped
make coverage # count the functions `make test` never executes, and print
              # them by name. Not a gate — see the note below on why the
              # number is not the point and the list is
make app      # build dist/Chuck.app, universal and signed (macOS)
make notarize # notarize and staple it, and cut dist/Chuck-<version>.dmg
make clean    # remove build/, dist/, ./chuck and ./chuck-editor
```

The debug build is the only one with the level picker on its title screen
(`</>` or `[`/`]` to choose, `F5` to start there); the release build has no
`CHUCK_DEBUG` in it at all.

`./chuck --level N` boots straight into campaign sector N, skipping the title
screen and the prologue; it is what the editor's playtest button launches.
`./chuck --soak N` closes the window by itself after N seconds, which is the only
thing that lets a build be *run* by a script rather than played; it exists for
`make soak` and is explained below. `./chuck --screen NAME` opens one screen and
stays on it — the prologue's three beats, the manual, the options sheet, the
pause sheet, the four cards a sector or a run ends on, the ending, the roll of
names, the restroom, and `aftermath` — because those are reached by a *choice* and
a headless run makes none. `--screen aftermath` is the odd one: it is not a screen
but a *world state*, a sector staged a few seconds after it went wrong, because the
one thing no other switch reaches is a player who has **done** something. See
`soak_stage_aftermath` in [game.c](src/game.c).
`./chuck --screen NAME --page N` picks which drawing behind a name, because three
of them stand for more than one: the manual's ten sheets (nothing turns a sheet
but a hand, and without this the sweep covered the first illustration and left
nine compiled and never executed), the options sheet's two halves (the controls
page is every key cap on it), and the aftermath's five poses — `draw_player`
answers hacking first, crawling second and a bazooka before a sidearm, so one
frame holds exactly one of them, and the pose that covers the launcher is the pose
that stops covering the muzzle flash. `--screen aftermath --level N` on a climb
stages the wall's own two hazards instead, a thrown object and a bird, because
both spawn on a timer and "whether two seconds caught one" is luck rather than
coverage.
`--screen restroom --level N` is the same problem answered by the other switch:
which room a `U` opens on is decided by the sector's `THEME`, so a sweep pinned to
sector 1 drew the lobby's washroom four times and left the plant's, the archive's
and the penthouse's unexecuted — and the toilet prop `q` is in those three and in
no other map in the game.
`./chuck-editor --soak N` is the same switch on the other binary.

**A sanitizer that never runs the binary it built is not a sanitizer.**
`make sanitize` compiled the whole tree with ASan and UBSan and then ran only
`core_tests`, which links no SDL at all — so the sanitized *game* was built and
never started, and CI could not have started it either, because that job builds
SDL with `-DSDL_X11=OFF -DSDL_WAYLAND=OFF`. What went unexecuted was
[game_render.c](src/game_render.c), [level_art.c](src/level_art.c),
[cutscene.c](src/cutscene.c), [render_figures.c](src/render_figures.c),
[audio.c](src/audio.c), [intro.c](src/intro.c), [manual.c](src/manual.c) and
[chase_render.c](src/chase_render.c): more than half the tree by source size,
under the one target whose name promises otherwise. It was clean when the sweep
was finally written, which is the point — nothing had been checking, so nobody
knew either way. `make sanitize` ends in `make soak` now.
[tools/soak.sh](tools/soak.sh) is what closes it, and two details are what make
it a check rather than a gesture: the **dummy video driver**, which falls back to
the software renderer and therefore really rasterizes (a sanitized sector burns
most of a core), and **`--soak` rather than a `kill`**, because a killed process
never reaches `SDL_AppQuit` and teardown is the half of the lifecycle a sanitizer
is most likely to have something to say about.

**A new screen owes this sweep a thought about whether any run reaches it, and
"reaches" is a question about the run rather than about the screen.** This
paragraph used to end "`--level N` never draws the prologue, which is why the
title screen is soaked separately", and `soak.sh`'s own header said the title
screen was the only entry point that reached `intro.c`, *the prologue's
cutscenes* and the attract music. It never reached the cutscenes: `STATE_INTRO`
advances on `game->input.confirm`, every line that sets that flag sits inside an
SDL event handler, and a headless process receives no events — so the sweep held
the first screen for its whole budget while two comments said otherwise. That is
this file's own recurring defect, a check reporting coverage it does not have,
committed by the sweep written to end it. `chase_render.c` entire, most of
`cutscene.c`, `manual.c`, the settings/pause/continue/game-over halves of
`game_render.c` and the four restroom sublevels were what it left behind, and
`--screen NAME` is what reaches them now.

**And then it was measured, which is the only way any of this is ever known.** A
coverage build walked the sweep and counted the functions it never executes, and
the answer was forty-two of them across the SDL-side files — the half of the tree
no test can reach either. They came in three kinds, and the kinds are the lesson:

- **Named but not reached.** `--screen continue` drew the *game-over* card.
  `campaign_begin_continue` refuses a run with lives still in it and leaves the
  countdown at nought, this entry point ignored the refusal and forced the state,
  and the first update moved on to `STATE_GAME_OVER`. The sweep printed
  `screen continue ok` and rasterized the wrong card. Naming a screen is not
  reaching it, one floor below where that was last found.
- **Reached but truncated.** `SOAK_SECONDS ?= 2` on a twenty-seven-second outro
  draws the opening beat. Fifteen of `cutscene.c`'s drawing functions and three of
  `chase_render.c`'s were nought at two seconds and fully executed at sixty —
  among them `draw_helicopter`, sixty-three lines of the ending the whole campaign
  is played for. `screen_seconds` derives each budget from the game's own duration
  constants now, so a beat that gets longer is walked to its end by having been
  lengthened.
- **Reached but never acted on.** A headless player stands still where it spawned,
  so the bodies, the opened `%`, the alarm lighting, the crawl, the hack, the
  bazooka and the emitting half of `particle.c` were drawn by nothing at all —
  and `draw_downed_enemy` is the object the entire quiet route is played around.
  `--screen aftermath` stages that world instead of playing it, the way `report`
  and `cleared` already stage their numbers.

Measured again afterwards, the renderers went from **forty-two never-executed
functions to six**, and the six were honest: two in `audio.c` behind the mute key,
four in `pad_hint.c` behind a controller being plugged in. Those last four are
gone — not by being reached with a gamepad, but by the file moving to the side of
the boundary a test can reach; see the note further down on the treatment
[keybind.c](src/keybind.c) already had.

Getting the third one to actually draw took four passes, and each failure is worth
knowing because each was the same mistake as the bug: state staged, coverage
assumed. The reveal was left un-walked, so `render_world` ran at six percent
behind a hidden map. The simulation's first step cleared every transient, so the
poses were staged and a standing man was drawn — `STATE_PAUSED`, whose own comment
is "time stands still", is what freezes the frame. The camera was never moved, so
the tile loops drew a corner of the map the staging was not in. And the patch was
chosen near the player when sector 4's only `%` is twenty-eight tiles from the
spawn and a viewport is twenty-five wide — the man is moved to the patch now. **A
staged screen is not covered because it was staged; it is covered when something
has counted it.**

What is left is honest to write down rather than to claim: the pause and options
sheets' own row handlers in `game.c`, and `audio_toggle_mute` and
`audio_stop_music`. Both need menu presses, and neither is something `--screen`
can name — a switch names a state, and these are *transitions*.

**And `pad_hint.c` was the fourth one, which has now had the treatment
[keybind.c](src/keybind.c) already had** — SDL-free, in `TEST_SOURCES`, its
numbers held to `SDL_GAMEPAD_BUTTON_LABEL_*` and `SDL_GAMEPAD_TYPE_*` by
compile-time assertions in [game_input.c](src/game_input.c), one per row
generated from the same list the tables are. What is worth taking from it is
*why* it was reachable after all, because the reason applies to anything else
that ends up on the wrong side of this line: the four functions were listed as
needing a controller plugged in, and they never needed one. They needed **what a
controller said**, which is four label numbers and a type number, as inert as
the scancodes next door. Opening the pad and asking it those two questions is
the only part that wanted SDL, and it is nine lines; the decision it feeds —
which letter goes on which button, the whole reason the file exists — was
sitting behind it on the far side of the boundary because it happened to be in
the same function.

That decision is the one in this game a player cannot ignore if it is wrong: a
Switch pad prints A where an Xbox pad prints B, so the title screen asked for A
and the button printed A quit the game. It is now `pad_hints_apply`, and the
suite drives it with an Xbox, a Switch Pro, a PS3, a PS4, a PS5 and a pad type
the enum does not list, plus the two half-lettered pads that have to fall back
rather than commit a mixture. **"It needs hardware" is worth testing as a claim
rather than accepting as a reason** — most of the time what it needs is a number
the hardware would have said.

**And then the same question was put to the other half of the tree, which is
the half that had never been asked it.** Everything above is about the renderers
— the side no test can reach — and the whole argument for the SDL boundary is
that what is on the near side of it *is* reachable. Nobody had counted.
`make coverage` counts, and the first answer was **fourteen functions in the
SDL-free tree that the suite had never executed**: both platform updaters, two
of the four questions a dog asks about a hole in the floor, the whole of how it
picks somewhere to wander, `gameplay_destroy_crate` and
`gameplay_kill_dog_with_crate`, `release_body_bit`, two of the options sheet's
own accessors, and four of `editor_doc`'s. All fourteen have tests now and the
list prints `none`.

Two of them are a class rather than an oversight, and it is the one to watch
for. `level_update_moving_platforms` and `level_update_falling_platforms` are in
[level.c](src/level.c), on the *core* side, and their only caller is
`update_playing` in [game.c](src/game.c), on the shell side — so a suite that
links every gameplay module and drives it directly still never called them. **A
function is not reached because its file is linked; it is reached because
something in the test binary calls it.** `P` and `F` are on seven shipped
floors, and neither had been simulated once.

**And then that answer turned out to be the wrong question, which is the last
lesson in this file and the one it earned the hard way.** `none` was true and
believed for as long as it was printed, and underneath it sat **eleven hundred
lines of the SDL-free tree compiled and never run** — because this target
reported a function only if it was never *entered*, and said nothing about a
function that runs constantly with the mechanic inside it never firing.
`player_update` was called about two hundred thousand times by the suite without
one of those calls ever holding `down` on a floor, so the whole crawl was in the
covered column: the posture a gas canister is shot from, one of the two ways of
being hard to see, and every test of it setting `player.crawling` by hand. So
were the forty lines that pair two guards into a conversation, the twenty-five
that walk a console's reinforcements out of a door, the animal half of
`update_body_discovery`, the dog half of `apply_blast`, the camera half of the
flash charge — a promise [docs/gameplay.md](docs/gameplay.md) makes by name — and
the whole of the man with the mop.

That is precisely the sweep three paragraphs up, in the same three kinds, on the
half of the tree this page calls testable: **named but not reached** (a queue the
suite booked and never drained), **reached but truncated** (a function entered on
every frame, one branch deep), **reached but never acted on** (every crawl test
staging the flag instead of pressing the key). It arrived because the check for
it was reading the wrong column of `llvm-cov`, which is this file's own recurring
defect one more time: *a check reporting coverage it does not have.*

`make coverage` prints both lists now — the functions, and then the unexecuted
**lines** per file with the longest unbroken run in each, because a run of
twenty-five is a mechanic and a scatter of ones is a file's worth of `return
false` guards. Thirteen tests took the total from eleven hundred to seven hundred
and sixty-six, `gameplay_ai.c` from two hundred and seventy-nine to a hundred and
twenty-eight, and `player.c` from forty-eight to five; a later sweep took it to
**seven hundred and twenty-three** and is written up below.
What is left is honest to read rather than to claim, and reading it is the whole
point. **`none` is not the target and never was.**

Two of those ten are worth naming for what they say about the campaign rather
than the suite. `test_a_sector_gives_the_player_a_moment_to_read_it` drives every
sector for three seconds with nothing pressed, and six of the seventeen used to
cost a heart inside that window — sector 6 at two seconds of rifle fire, sector
14 with a guard dog eight tiles from an open spawn. Nothing was wrong with any
map by any rule then written down: every other campaign check asks about the
*map*, and none of them asked what the floor does to a player who has just been
handed the controls. And `test_the_whole_frame_survives_a_monkey_on_the_controls`
runs the passes in `update_playing`'s own order with seeded noise on the pad,
which nothing did — every other test drives a hand-picked handful of the frame,
and the soak runs the real order standing still, so the ordering was covered by a
player who did nothing and the actions were covered outside the ordering.

**And then the same list was read again, which is what it is for, and the thing
it turned up has a shape worth naming: two mechanics written as one another's
twin, with one test between them.** A climb carries exactly two things that can
cost a heart, a thrown object and a bird, and in
[gameplay_climb.c](src/gameplay_climb.c) they are the same twelve lines twice —
shatter on masonry, leave the world, or hit the man. `test_facade_bird_hits_player`
had held one of them since it was written. The other had never been simulated
once, on five of the seventeen sectors, and the soak sweep even stages both of
them for the *renderer* — so the picture was covered on both sides and the
simulation on one. Neither file tells you that by being read; only counting does.

Four more came out of the same list and every one of them is a branch rather
than a function, which is why the `-show-functions` half of this target said
`none` throughout:

- **A gust that never ends.** `test_facade_wind_warns_then_pushes_unless_sheltered`
  walked calm, warning, gust — and stopped asking, so the arm of `update_wind`
  that hands the wall *back* to calm was dark. A gust that never expired would
  push a climber sideways for the rest of the map, on the five tallest maps in
  the game, and every assertion in that test would still have passed. It counts
  both directions now, and requires the next gust to announce itself.
- **A branch is not reached because the function it calls is covered.**
  `gameplay_restore_checkpoint` dispatches on the sector's mode, and every test
  of the climb's restore called `gameplay_climb_restore_checkpoint` *directly* —
  so the two lines that choose between the two rules were never executed, while
  the functions on either side of them were. The shell calls the generic one.
  This is the `level_update_moving_platforms` lesson one floor up: that was a
  function whose only caller was on the other side of the boundary, this is a
  *branch* whose only caller was the shell.
- **Nine rows of the options sheet, tested two at a time.** `settings_adjust` had
  exactly one caller in the suite, naming the music slider and one assist switch;
  seven arms of its switch were dark, among them slower guards, infinite lives
  and veteran — the three that change the simulation. Naming rows one at a time
  is how seven of them came to have no test, so
  `test_every_options_row_moves_its_own_field` walks the enum and asks the
  property a copy-paste actually breaks: every row moves *a* field, and no two
  rows move the *same* one. It needs no list of which field is which, so it
  cannot be wrong in the way the thing it checks is wrong.
- **`||` is where a branch goes to hide.** Every test of body discovery ends
  `investigate_timer > 0 || raising_alarm`, and the suite always took the left
  side: a guard who finds a corpse rolls `GUARD_BODY_ALARM_CHANCE` and may run
  for a wall switch, and that roll and that call had never been executed from the
  corpse route — only from the sighting route. It is the risk the whole quiet
  route is played against; without it, hiding a body would cost nothing and
  nothing in the suite could tell. Seeded sixty-four ways now, requiring *both*
  outcomes to occur, because a floor where every witness runs for the alarm is as
  wrong as one where none does.

None of the five was a shipped bug — each was verified working before its test
was written, which is worth saying plainly rather than implying otherwise. What
they were is five mechanics the suite would not have noticed the loss of. Every
one of the tests was then **checked by breaking the thing it guards and watching
it fail**, fifteen mutations in all, on the same principle as the unsized-array
assertions further down: a guard nobody has seen fire is a guard nobody has
checked. Two of the fifteen are caught by the *game* build rather than the suite,
because they are the compile-time assertions tying `pad_hint.h`'s numbers to
SDL's, and the test binary links no SDL — which is the arrangement working, not a
hole in it.

`make coverage` is deliberately **not** a gate, and the reason is the shape of
the number. A line percentage in CI is a figure people learn to move; what is
worth knowing is a *function nobody runs*, which is a list you read once after
adding one. `make test` is the gate; this is the thing you run before believing
the gate covers what you just wrote.

**And the editor was the last binary nothing ran at all.** `make sanitize` built
`all test soak`, `all` is the game, so [editor_app.c](editor/editor_app.c),
[editor_render.c](editor/editor_render.c) and
[editor_ui.c](editor/editor_ui.c) were never compiled under a sanitizer, never
mind executed; the macOS CI job built the editor and did nothing with it. The
three editor files the suite *does* link are exactly the three that touch no
SDL, which is what kept the hole out of sight. `make sanitize` builds the editor
now and the sweep soaks it.

SDL3 must be discoverable through `pkg-config`. The **test binary links no SDL**
(`TEST_CFLAGS` omits the SDL flags), so `make test` works even where SDL3 is
unavailable, and it runs in well under a second.

There is no test filter: `tests/test_main.c` is one binary whose `main()` calls
every `test_*` function in sequence. To run a single test, temporarily comment
out the others in `main()`, or just run the whole suite. Failures are reported
by the `CHECK` macro as `file:line: check failed: <expr>` and the process exits 1.

**All of it runs in CI** ([.github/workflows/ci.yml](.github/workflows/ci.yml)),
in three jobs split by what each needs: `make test` on a runner with no SDL
installed at all — if that job ever needs one, the SDL boundary below has been
crossed and the failure is the point — then the sanitizers, then a macOS job
that builds the game, the editor, the debug target **and the shipped bundle** on
the platform the game actually ships on. That last one is
`make app`, and it is there because none of the others builds it: `make` links
Homebrew's SDL for this arch and this macOS, while the app is two slices against
the vendored universal framework with an older deployment floor. Left out, a
break in the fetch, the `lipo`, the framework's rpath or the `Info.plist` read
out of [version.h](src/version.h) was only ever found by whoever was cutting a
release. No signing identity is needed for it — `packaging/build_app.sh` falls
back to an ad-hoc signature, which still proves the bundle assembles and
verifies. `make notarize` is the step that needs a real identity and stays a
local one.

**And what bounds the clock is part of what has to be true, because nothing
bounded it.** GitHub's default job timeout is six hours, so a step that neither
finishes nor fails spends all six and then reports the hour rather than the
cause: a clone that stalls, a package manager that finds no bottle for the
runner's macOS and quietly starts compiling one, a soak whose binary never
reaches `SDL_AppQuit`. Every job carries `timeout-minutes` now and the steps that
reach the network carry their own, because a bound on the *step* names the
culprit. The sanitizer job's SDL is a staged install restored from a cache keyed
on the pin rather than a source build on every push; `concurrency` cancels a
superseded run on every ref including `main`, because this history is amended and
force-pushed and a run of a commit that no longer exists is not a record of
anything; and `-j` is on every build step.

**And then it was measured, which is the only way any of this is ever known.**
The first run of the above was thirteen minutes and none of it was where the
comments said. The macOS `brew install` — the step this file had just called the
one that could quietly turn into an hour-long source build — took **six
seconds**. Building SDL from source, the cost the whole cache exists to avoid,
took **1.8 minutes**. What took ten was `apt-get update`, in a step that existed
to install `pkg-config` onto an image that already ships it, on the one path every
run takes: a distro mirror went slow, apt sat there retrying, and a job that
checks nothing about this tree spent longer doing it than every real step
combined. It asks `command -v` first now, the build's own prerequisites moved into
the step that only runs on a cache miss, and the ALSA and Pulse headers are gone
along with the last reason to call a package manager at all — this job never opens
an audio device, because the sweep forces `SDL_AUDIODRIVER=dummy`. Steady state is
under six minutes. **The step that was described as the risk was the cheap one,
and the step nobody had described at all was the whole cost.** That last one is why `sanitize` is two `$(MAKE)` invocations rather
than one list of goals: goals on one command line are exactly what `-j` runs at
the same time, and a sweep started on a half-linked game is the loud half of that
— tools/soak.sh *skips* an editor it cannot find, so the same race on the editor
would have reported a clean soak of a binary it never opened.

## Architecture

### The SDL boundary

Two layers, and the split is the most important invariant in the codebase:

- **Application shell** (SDL-dependent): [main.c](src/main.c) (SDL callbacks),
  [game.c](src/game.c) (state machine, level loading, per-frame orchestration),
  [game_input.c](src/game_input.c), [game_render.c](src/game_render.c),
  [render_figures.c](src/render_figures.c),
  [render_sprite.c](src/render_sprite.c),
  [chase_render.c](src/chase_render.c), [level_art.c](src/level_art.c),
  [audio.c](src/audio.c),
  [intro.c](src/intro.c), [manual.c](src/manual.c),
  [cutscene.c](src/cutscene.c),
  [particle.c](src/particle.c). [crew.c](src/crew.c),
  [credits.c](src/credits.c), [manual_pages.c](src/manual_pages.c),
  [intel.c](src/intel.c), [pause_sheet.c](src/pause_sheet.c) and
  [sector_tally.c](src/sector_tally.c) sit on
  this side too and link no SDL, because they are tables of strings — but they
  are presentation, and no gameplay module may include them. See
  [The net](docs/story.md#the-net), [The credits](docs/story.md#the-credits),
  [The field manual](docs/screens.md#the-field-manual) and
  [The report between sectors](docs/story.md#the-report-between-sectors); each of them
  is split out so the suite can hold its words to the frame they are drawn in.
  A table of words that stays inside its renderer is a table nothing measures,
  which is exactly how the manual's control sheet lost its last line and how
  the report's intel line sat for a long time with its ceiling written down as
  a sentence in a comment and nothing holding it there.
  [pad_hint.c](src/pad_hint.c) is on this side and links no SDL for a different
  reason from those six: it is not a table of words but a *decision* — which
  letter is printed on which button — and it linked SDL until somebody asked
  what it actually needed. The answer was four label numbers and a type number,
  so those are plain constants here now, `game_input.c` asserts every one of
  them against `SDL_GAMEPAD_BUTTON_LABEL_*` and `SDL_GAMEPAD_TYPE_*` at compile
  time, and the pad is opened and questioned over there. It is in
  `TEST_SOURCES`. See the note above on why "it needs hardware" is worth
  testing as a claim.
- **Gameplay core** (no SDL, no knowledge of `Game`): `src/gameplay_*.c`,
  [level.c](src/level.c), [level_route.c](src/level_route.c),
  [player.c](src/player.c), [enemy.c](src/enemy.c),
  [chase.c](src/chase.c), [rng.c](src/rng.c), [camera.c](src/camera.c),
  [game_event.c](src/game_event.c). These only include each other plus libc.
  That is what makes them deterministic and directly testable.
  **This list is the whole of `src/*.c`, and saying so is the point**: it named
  neither [level_art.c](src/level_art.c) nor [camera.c](src/camera.c) for as
  long as it existed, so the file holding the campaign's largest renderer and
  the file holding its smallest simulated one were both outside the one
  invariant this page calls the most important in the codebase. A list of two
  sides that does not account for every file is a list a reader trusts and a
  new file quietly falls out of. `level_art.c` in particular reads as core work
  — it is named `level_*`, it takes a `LevelMap` — and is pure presentation
  that links SDL, which is exactly the mistake the naming invites.
  **And a file on the shell side is a file no test can reach**, which is not a
  filing detail but the reason `THEME_MUSIC` moved to [level.c](src/level.c):
  it was an enum-indexed table living in `level_art.c`, so a theme added
  without a row would have zero-filled to `MUSIC_INTRO` and scored a sector
  with the title screen, and nothing in the tree could have said so. Data a
  test needs does not belong on this side of the line, however much it looks
  like art direction.
  **`THEME_CORDON` is the second one, and it had already gone wrong** — which is
  the difference between the two: the music table was moved before it cost
  anything, and this was found paying. `facade_cordon` washes the lower face of
  a climb in the street's blue and red, strongest on the first and lowest wall
  and gone by the top; [docs/story.md](docs/story.md) says so, and the choosing
  was a `switch` in `level_art.c` keyed on **`art->backdrop`**. There are four
  facade backdrops and five climbs, because `FACADE_SLEET` borrows the storm's —
  so the highest wall in the game, two floors under the roof, answered as the
  second one and put *more* street under sector 15 than under the two climbs
  below it. Nothing failed; a picture simply stopped meaning what the page says
  it means. Two lessons, and the second is the general one: **a value there is
  one of per X has to be keyed on the thing there is one of per X**, and a
  renderer is where nobody can check which. It is a row per theme in
  [level.c](src/level.c) now, and
  `test_the_cordon_fades_as_the_climb_rises` walks the campaign's own climbs in
  order and requires it to fall.
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
respect, and a table of all seventeen plans.
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
  **And the rate was only half of it: a test also has to count in seconds.**
  That paragraph was written, and the suite went on driving the simulation at
  four rates the game does not run at — `1.0f / 60.0f` and `0.016f` at
  eighty-four call sites, `1.0f / 120.0f` and `0.05f` in another thirty-one
  loops — against forty-one places that used `SIM_STEP_DT`. A rule stated in
  the file every session loads, contradicted two directories over, three to one.
  Swapping the `dt` alone is not the fix and is the trap: a loop written
  `for (frame = 0; frame < 240; ++frame)` does not say four seconds, it says 240
  steps, which is four seconds at 60Hz and one at 240. Converted that way,
  forty-four checks failed for no reason but having been quietly cut to a
  quarter of what they used to simulate — and **the ones that still passed were
  the dangerous half**, because a test that has stopped reaching the thing it
  asserts is a test that reports success. `SIM_STEPS(seconds)` in
  [tests/test_main.c](tests/test_main.c) is how long, and it says how long; the
  one place another rate is still right is the test of rate-independence itself,
  which sweeps ten of them and says so.
  Two assertions turned out to be pinning the invented rate rather than the
  behaviour, and the second is the one worth remembering.
  `test_fresh_sighting_waits_before_aiming` asserted `steps == 4` for what is
  really `ENEMY_NOTICE_TIME`, which is merely brittle. `test_coyote_time_allows_a_late_jump`
  pressed jump 2 steps and 14 steps after the ledge, which at 1/60 sat either
  side of a `PLAYER_COYOTE_TIME` of 0.10s — and at the rate the game runs are
  0.008s and 0.058s, *both inside the window*. The half of that test that checks
  the window closing had stopped existing, and it passed the whole time because
  both halves took the same branch. It counts the branches now and requires one
  of each. **A frame count is nearly always a duration somebody has already
  divided by a rate, and the division is where the meaning goes.**
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
  **And "every word" was one word short for as long as that sentence stood.**
  The sheet's *footer* — the line naming the way out of it — stayed a literal
  inside `draw_settings_menu` while the strap, the title, every row and the three
  loose lines were moved out and measured, and it is the **widest line the plate
  carries**: 446px of a 490px column, 44px of air, less than anything else on it.
  The comment beside it said the two capture lines were measured, which was true
  and read as though it covered the block it sat in. So the failure was not a
  missing check but a check whose *name* overstated it, in a file whose own rule
  is that a table of words is measured — and the pause sheet, one entry down, has
  measured its own footer since the day it was split out. Two sheets, the same
  question, two answers. `SETTINGS_FOOTER_KEY_LINE` and
  `SETTINGS_FOOTER_PAD_LINE` are in the header now.
  **A footer written as a `pad_hint` template needs expanding rather than
  counting**, which is the second half and the one that was quietly wrong on both
  sheets. Both were measured by counting the *template's* characters, on the
  argument that a `$A`-style token is two characters standing for at most two
  glyphs — true of `$A` through `$Y`, whose widest spellings are a PlayStation's
  `[]` and `/\`, and false of `$START`, which is six characters standing for
  `OPTIONS`. The ceiling was right for the tokens in use and wrong for one that
  could be added at any time, with a comment beside it promising the check
  covered whatever was written there. Both are expanded through `pad_hint`
  itself now, against the widest spelling any pad in `read_named_buttons` can
  produce (`widest_pad_spelling` in the suite) — which only became possible when
  [pad_hint.c](src/pad_hint.c) stopped linking SDL.
  **[pause_sheet.c](src/pause_sheet.c) is the sixth of those files, and it is the
  one that found nothing** — which is why it is worth naming separately. Three
  rows, a title, a strap and a footer prompt, all of them literals inside
  `draw_pause_menu` with the plate's width a local `const float` beside them: the
  same shape `settings.c` was in, on the sheet a player opens more often than any
  other. Measured on the way out it all fitted, with 60px of air on the widest
  line. A check that passes the day it is written is still the only thing that
  will notice the fourth row, and finding the table *before* it has lost a line is
  what this rule is actually for — the other five were found afterwards. The row
  enum is generated from the same list as the words (`PAUSE_ROWS`), so the count
  cannot drift from the table the way the manual's mission sheet once drifted
  from the campaign.
  **[sector_tally.c](src/sector_tally.c) is the seventh, and it is the one that
  was written because the words were *missing* rather than unmeasured.** The
  report between sectors carries the stopwatch, the record beside it and the two
  bonuses, and it is shown only after a sector that leaves by its stair door —
  six of the seventeen. The other eleven clears went on paying a time bonus,
  paying a clean bonus and banking a per-sector record in silence:
  `progress_note_sector_time` writes a record for every sector and
  `progress_sector_time` had exactly one caller, the line that fills the report
  in, so eleven of them were kept on the player's disk across sessions with no
  screen in the game able to show them. The fiction's objection is to the *cut* —
  a window is a continuous route onto the facade and an ops screen over it would
  contradict the display — and that was never an argument about the numbers. So
  the numbers travel as one line, over the reveal of the sector above and under
  the card that ends the campaign, and the file is measured like the six before
  it (`test_the_sector_tally_fits_the_frame_it_is_drawn_in`). What is worth
  taking from it is the other half:
  **a rule that suppresses a screen has to be asked what else was on it.**
  **[run_tally.c](src/run_tally.c) is the eighth, and it is the same defect one
  screen further on: a table of words that existed and was reachable from only
  one of the two endings.** The score and the docket have been printed on the
  game-over card since either of them outlived the process, and that card's own
  comment called itself "the only screen a score is being looked at rather than
  played for". It was the only such screen because the other ending printed
  nothing: a *won* campaign banks both figures on the way into `STATE_OUTRO` and
  then drew SHE'S SAFE, a thank-you and a roll of names, so the twelve-sheet
  collection the fiction spends a page on was acknowledged to the player who died
  on sector three and not to the player who carried it onto the helicopter. The
  words are one file now and both endings draw them, measured at the card's own
  double scale (`test_the_run_tally_fits_the_frame_it_is_drawn_in`). The same file
  carries the record card's cells, because the ninth place a table of words was
  missing was the manual: `Progress` keeps seventeen per-sector times and the
  game could show one of them, on the screen after the sector that set it. `THE
  RECORD` is the sheet that reads them all.
  What is worth taking from this one:
  **a screen that reports a run has to be asked which ways a run can end.**
- **An array whose length is a claim must be written `[]`, or the assertion
  guarding it is measuring itself.** This is a whole class and every table in the
  tree was in it or beside it. `PAGE_ILLUSTRATIONS` in [manual.c](src/manual.c) is
  one drawing function per manual sheet and carried
  `_Static_assert(SDL_arraysize(PAGE_ILLUSTRATIONS) == MANUAL_PAGE_COUNT)` — over
  an array declared `[MANUAL_PAGE_COUNT]`, so the check read "the count equals the
  count" and no missing row could ever fail it. A short initializer zero-fills, so
  a sheet listed in `MANUAL_PAGES` with no picture beside it is a **null function
  pointer called** on the frame that sheet is opened. Deleting one entry built
  clean, passed `make lint`, passed all of `make test`, and segfaulted on
  `--screen manual --page 10`; only the soak sweep, which walks every sheet, had
  anything to say. Two other places asserted that this guard worked —
  [player.c](src/player.c)'s note on `WEAPON_CYCLE` and
  [docs/screens.md](docs/screens.md) — and `WEAPON_CYCLE` was the only one of the
  pair actually holding, because it is the one written `[]`.
  `LEVEL_THEME_NAMES`, `MANUAL_PAGES`, `FACE_TINT`, `PAD_FACE_POSITIONS`,
  `ED_TOOL_NAMES`, `ED_TOOL_HINTS` and `ED_GROUP_NAMES` were all the
  same shape and are all unsized now, each with an assertion that has been
  **checked by deleting a row and watching the build fail** — because an
  unverified guard against unverified guards would be the joke writing itself.
  **An `extern` declaration completes the type, so it has to be unsized too**:
  `extern const char *const ED_GROUP_NAMES[ED_GROUP_COUNT]` in the header handed
  the definition's `sizeof` the declared length, which made the assertion beside
  the table tautological again from a different file. Nothing outside takes these
  arrays' sizes; the count is the enum.
  **A table indexed by *designator* cannot be fixed this way at all**, which is
  why `THEME_ART` and `MUSIC_PLANS` are in `check_lists.py` instead: a designated
  initializer is exactly as long with a middle row missing as with it present. Both
  also sit on the SDL side, so `make test` can reach neither — a missing
  `THEME_ART` row draws a sector as an unlit void and a missing `MUSIC_PLANS` row
  plays it in silence, and until those two checks existed nothing in the tree
  would have said a word.
- **A list written down more than once owes [tools/check_lists.py](tools/check_lists.py)
  an entry**, which is the same rule as the one above with a different shape of
  copy. The numbers this file spends pages on are all *scalars* — the campaign's
  length, the climb count, the sheet count — and every one of them is derived or
  checked now. Three **lists** were not. The map legend is written out in
  [levels/LEGEND.md](levels/LEGEND.md), painted in
  [editor/editor_legend.c](editor/editor_legend.c) and parsed in
  [level.c](src/level.c); the soak sweep's screen names are in
  [game.c](src/game.c) and again in [tools/soak.sh](tools/soak.sh); and
  [main.c](src/main.c) parses its switches in three functions and lists them a
  fourth time for the unknown-argument warning. All three agreed on the day the
  script was written, which is the point — nothing was holding them there, and one
  of the three fails *silently in the worst direction*: a screen the game knows
  and the script does not is compiled under the sanitizers, never executed by
  them, and `make sanitize` still reports a clean sweep. The counting side of the
  same class was live and found by writing it: the editor's decoration cap and
  `check_docs.py`'s prop counter were each a fifth and sixth copy of the prop
  list, and both had already fallen behind the plant set. Both derive their list
  now — from the palette and from the parser — so a new prop is counted by having
  been drawn rather than by being remembered.
- **A table indexed by sector owes a check that the maps still agree with it**,
  and this is the rule the fit checks above do not cover: they measure whether a
  line *fits*, never whether anybody *reaches* it. The report between sectors is
  shown only after a sector that leaves by its stair door, so which rows of
  [intel.c](src/intel.c) a player ever reads is decided by the maps — and the day
  sector 15 became a climb, sector 14 had to gain a `Y` to put Chuck on the wall,
  which silently deleted the report after it. What went with it was `TWO-KEY
  DOOR. SHE IS THE SECOND.`: the answer to sector 8's turn and the only place the
  game says why the hostage is still alive. Every check passed. The line was
  measured, the map was loaded, and nothing compared the two.
  `INTEL_ARC_SECTORS` and `test_the_arc_lands_on_the_sectors_that_show_a_report`
  are what compare them now.
  **And a count that is *drawn* rather than written needs the same treatment**,
  which is the half that is easiest to miss because no fit check can see a
  picture. The manual's `THE MISSION` sheet illustrates the campaign as one tick
  a sector with the climbs in amber, counting to a literal 15 against four
  literal climb numbers, so it went on drawing the old campaign — two ticks
  short, one of them the wrong colour — on the one sheet whose job is to show the
  player the shape of the night. It reads `CAMPAIGN_SECTORS` and
  `CAMPAIGN_CLIMB_SECTORS` now, both held against the embedded maps by
  `test_the_manual_draws_the_campaign_it_ships_with`. If a renderer states a fact
  about the campaign, the fact belongs in a table the suite can reach.
  **And the words printed beside the picture are a third copy again**, which is
  the half that fix missed: `THE MISSION`'s strap says `SEVENTEEN SECTORS`, its
  caption says `FIVE OF THEM ARE ON THE OUTSIDE` and `THE CLIMB`'s strap spells
  `SECTORS 3, 7, 11, 13 AND 15` — all three hard-coded English, on the sheets
  whose illustrations had just been wired to the tables. Worse, the test's own
  comment claimed it covered them, so a reader had no reason to look. A comment
  that promises coverage owes the suite the check that delivers it;
  `test_the_manual_says_the_campaign_it_draws` is that check.
  **And a fact stated in a *sentence* needs a different answer, because no table
  will hold it.** The pages in `docs/` name which sectors carry a camera, a heavy
  or two medkits, and a sentence is not something `make test` can reach — so all
  three went on describing the fifteen-sector campaign, one of them naming a
  sector that had become a facade and therefore had no men on it to describe. A
  table of those facts would be read by nothing in the game, which makes it a
  second copy of the maps rather than a fix.
  [tools/check_docs.py](tools/check_docs.py) derives them from `levels/*.txt` and
  asks the prose whether it agrees, keyed on a fragment of each claim's own
  reasoning so that a reworded sentence fails loudly instead of being checked no
  longer. A sector list in prose owes this file an entry.
  **And a comment in a `.c` file is prose**, which is the third thing this script
  had to learn. It read `docs/`, then `docs/` plus `LEGEND.md` and `README.md`,
  and all the while [gameplay_interaction.c](src/gameplay_interaction.c) argued
  for the wasted-pickup rule by naming `sectors 10, 12 and 15` — 15 having become
  a facade with one medkit on it — and [level_art.c](src/level_art.c) explained
  why the moon climb is not a sunrise by quoting a wall clock four minutes out.
  A sentence does not become checked by being in a source file, and the defect is
  the same one: the campaign moved and the sentence describing it did not. The
  script strips a C comment's line leaders and holds those two to the maps like
  any page.
  **A clock reading is arithmetic on a constant, so it is checkable too.** Every
  time the pages quote — 00:22 at the front door, 00:57 on the roof, the two dials
  either side of the moon climb, the hour that makes a staffed reception desk
  unbelievable — is derived from `NIGHT_CLOCK_*` by the same script. All of them
  were written for a night divided fifteen ways and were up to four minutes out
  after it was divided seventeen; the dial is the one thing in the game that
  states the fiction's clock out loud, so a page misquoting it contradicts
  something the player can see.
  **And "the prose" is every page, not just `docs/`** — which took a second
  sweep to learn. The script read `docs/` alone for as long as it existed, so
  [levels/LEGEND.md](levels/LEGEND.md) and [README.md](README.md) kept saying
  whatever they had been typed with: the `F` panel list named sector 15, a
  facade, which has no slab to cut a panel into, and the README said the sectors
  leaving by a window carry no terminal while ten did and one of those ten
  carried three. Both broke in the edit that made sector 15 a climb — the same
  one that silently took the report off sector 14 — and both sat through green
  builds. All three pages are scanned now.
  **A claim that is a count rather than a list needs the other half of the
  script.** `nine of the ten that leave by a window` is not a sector list, so
  the list parser could never see it; those go in `phrase_checks`, which derives
  the figure and requires the sentence to spell it. The hazard budget sequence
  is checked the same way, and it is the one place this script knowingly keeps a
  second copy of a C function — `level_hazard_budget` — because the numbers the
  page quotes are that function's output and nothing else in the tree can
  compare the two. The reasoning is written out beside it; if that formula
  moves, this moves with it.
- **The editor's validator knows more rules than the suite did, so the suite now
  runs it over the shipped maps.** [editor/editor_validate.c](editor/editor_validate.c)
  asks the campaign's questions while a map is being drawn — and a few of its own
  that nothing else asks: a prop that hangs from a slab that is not there, fan
  blades that reach a ladder, a crate parked under them. Those only ever ran on a
  map somebody happened to open in the editor, so two sat in the campaign through
  a green build: sector 16's wall clock hung under open sky and was silently
  dropped by the loader, and sector 17 had a fan two columns from the ladder onto
  the helicopter pad, costing an unavoidable heart on the last climb of the game.
  `test_the_editor_has_nothing_to_say_about_the_shipped_campaign` walks all
  seventeen sectors and all four restrooms and requires nought errors **and nought
  warnings** — a warning here means "it loads, but it will not play the way it
  reads", which is precisely the class nothing else can see. Notes stay allowed.
  **The map's own path is part of the input**, and leaving it out was a hole in
  that test for as long as it existed. `editor_validate` takes the sector number
  off `doc->path` and `editor_doc_parse` memsets the document, so the empty path
  this test handed it made `editor_path_level_number` return nought and the whole
  cross-sector block never ran over a single shipped map — while the comment above
  the loop said it did. Two things were sitting in there, both of them *notes*,
  the one severity this test allows: the editor's own `ED_CAMPAIGN_LENGTH` and
  `ED_CAMPAIGN_FACADES` had stayed at fifteen and four, so the tool told every
  author that the shipped campaign disagreed with the tests, and sector 14's
  flash charge sat on a partition five rows above its floor where the route model
  cannot reach it. The two constants read `CAMPAIGN_SECTORS` and
  `CAMPAIGN_CLIMB_SECTOR_COUNT` now — which is why `manual_pages.c` is in
  `EDITOR_SOURCES` — and **an unreachable pickup is a warning** rather than a
  note: optional to take is never optional to reach, and one nobody can stand
  next to is a thing drawn on the map that no player will ever meet.
  **And both of them are now also shown a few thousand files nobody meant.**
  Every other test in the suite hands the loader a map somebody drew on purpose,
  which leaves the one input the editor actually exists to open — a file
  half-finished, mistyped, or not a map at all — checked by nothing.
  `test_the_loader_and_the_editor_survive_nonsense` generates three thousand of
  them from a fixed seed and asks only that both halves *answer*: what a refusal
  says is the business of the tests around it; that there is one, and that the
  parser did not walk off the end of a grid on the way to it, is this one's. It
  found nothing the day it was written, which is the `pause_sheet.c` precedent
  and the same argument — and it is worth most under `make sanitize`, where it
  becomes a few thousand passes over the bounds arithmetic with ASan watching.
  It is the one test that puts stderr in the bin while it runs, because a
  refusal explaining itself three thousand times is how a suite's real output
  gets lost.
