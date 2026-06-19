# AGENTS.md

## Project

Chuck is a 2D action platformer written in C17 against SDL3. All art is drawn
procedurally at runtime and all audio is synthesised at startup, so the shipped
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
              # check_lists.py: a list written down twice agrees with itself,
              # including the X11 packages the payload job installs and the
              # ones build_linux.sh says SDL will not configure without
make sanitize # rebuild game + editor + tests with ASan/UBSan into
              # build/sanitize, run the suite, then soak both binaries
make soak     # tools/soak.sh: run a built game headless across the title
              # screen, all seventeen sectors and every screen reached by a
              # choice rather than by play, then two captures, then the editor,
              # failing on anything any of them says. SOAK_BINARY, SOAK_EDITOR
              # and SOAK_SECONDS pick what and how long; SOAK_MODE picks which of
              # the two questions — `full` is the coverage sweep, `smoke`
              # holds no timed sequence and says which ones it skipped. The
              # captures count the files off the disk rather than reading the
              # exit status: `--shot` was the last thing in this binary that no
              # gate ran at all
make coverage # count the functions `make test` never executes, and print
              # them by name. Not a gate — see the note below on why the
              # number is not the point and the list is
make coverage-shell # the other half: build the game instrumented, walk it with
              # the soak sweep, and print the functions **neither** gate runs.
              # Needs SDL and a few minutes; also not a gate. It is 41, and the
              # figure in the prose below has drifted once already — re-measure
              # it in the same commit as whatever moves it
make press    # photograph the game: 19 stills, 5 GIFs, the itch.io cover, the
              # page banner and a wallpaper into dist/press/, with a
              # MANIFEST.txt naming the exact
              # command behind each file. There is no art in this repository to
              # crop, so the only place it exists is a running process
make clean    # remove build/, dist/, ./chuck and ./chuck-editor
```

**A release is four targets, one per platform, and nothing else.**

```sh
make mac      # dist/Chuck-<v>-macos.zip — universal, signed, notarized,
              # stapled, and the .dmg beside it. Needs a Mac and an Apple ID
make win      # dist/Chuck-<v>-windows-x64.zip, cross-built with mingw-w64.
              # Runs anywhere mingw-w64 does, a Mac included
make linux    # dist/Chuck-<v>-linux-x86_64.tar.gz, SDL3 built from the pin and
              # travelling in the payload. Needs a Linux
make web      # dist/Chuck-<v>-web.zip — WebAssembly, played in a browser tab.
              # index.html at the ROOT of the archive, which is the opposite of
              # the three above and is what itch.io's HTML5 hosting requires.
              # Needs emscripten; cross-builds anywhere. 604KB, because a game
              # with no asset files has nothing to download
```

Each writes one archive into `dist/` and stops. Nothing here uploads anything and
nothing here knows an account exists. Which machine can make which is a fact
rather than a taste, and `.github/workflows/payloads.yml` exists to hand back the
one a Mac cannot produce.

**It was three for as long as the shop was a place people downloaded from, and
that was the mistaken half.** Most of itch.io is played in the browser: a visitor
who has to download a binary, unblock it and find it in a Downloads folder is a
visitor who has already left, and the three targets between them offered exactly
that to everybody. The web build is not a lesser fourth thing — on that shop it
is the one most people will ever run. What it cost was one flag in a header and
a build script, because the tree was already shaped for it: SDL main callbacks
rather than an owned loop, `SDL_SetRenderVSync(1)` already asked for, no threads
anywhere in `src/`, no asset files to package and no filesystem use beyond
`SDL_GetPrefPath`. See [The browser build](docs/tooling.md#the-browser-build) for
what was measured and what is deliberately not gated.

`make app` still exists and is not one of the four: it builds and signs the
bundle without going near Apple, which is what the macOS CI job needs and what
`make mac` runs first. **There were six targets here** — `app`, `notarize`,
`itch-macos`, `itch-linux`, `itch-windows` and an `itch` — plus a
`release-macos` whose whole job was to name the order the first three had to run
in, and the list also advertised a `make itch-page` that **had never existed at
all**. Three of the six were one platform's release split into steps, and that
split is what shipped an archive nobody could open; see the note further down.
`itch/page.html` is kept by hand beside `itch/page.md`, which is the thing the
missing target was described as doing.
```

The debug build is the only one with the level picker on its title screen
(`</>` or `[`/`]` to choose, `F5` to start there); the release build has no
`CHUCK_DEBUG` in it at all.

`./chuck --level N` boots straight into campaign sector N, skipping the title
screen and the prologue; it is what the editor's playtest button launches.
`./chuck --soak N` closes the window by itself after N seconds, which is the only
thing that lets a build be *run* by a script rather than played; it exists for
`make soak` and is explained below. A value that is not a positive number **refuses
to start** rather than falling through to the title screen, because the one
caller is a script and a mistyped budget used to mean a process that ran until
somebody noticed; `--page` is the same, and so is `--shot-at` — which arrived
after that work and did not inherit it, on the one switch in the binary that
*produces* something. `SDL_atof` answers nought to `abc` exactly as it answers
nought to `0`, and nought is a perfectly legal lead-in, so `--shot-at abc`
photographed frame one, logged `Wrote 1 frame(s)` and exited nought. The two
switches beside it escaped by luck rather than by design: `shot_plan_broke`
refuses a frame count under one and a rate under `MIN_FRAME_RATE`, so a typo
there decayed to a value that happened to be out of range. Worth knowing why
the file count in `tools/soak.sh` cannot see that one: that check exists so a
capture which wrote *nowhere* cannot pass, and a capture of the wrong **moment**
writes a file and sails through it.
**Every numeric switch on this line reads through one strict parse now**
(`parse_switch_number`), and the sentence above is why it had to become one
rather than three: the strictness was written for the capture switches, the
paragraph explaining it was written beside them, and `--soak` went on calling
`SDL_atof` eighty lines further up the same file — so `--soak inf` and
`--soak 1e400` set a budget that never runs out and the process **never closed**,
which is the failure that paragraph describes, in the switch it names, for a
release. `--soak 3s` and `--page 3s` were quietly read as 3. `--level` reads
through it too, since its documented licence to fall through to the title screen
is about the exit *code* and not about what counts as a sector number.
`parse_seed` stays out for a reason of its own: a seed is any 64-bit value and a
`double` stops holding one at 2^53. The refusals are the one thing here no test
can reach, so `tools/soak.sh` drives them — under a watchdog, because a switch
that hangs offers no status to check and no line to grep.
`./chuck --screen NAME` opens one screen and
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
nine compiled and never executed), the options sheet's pages (the controls page
is every key cap on it; the records page arrived after this sentence said "two
halves" and DIFFICULTY after it said three, which is why the sweep counts them
off the enum), and the aftermath's poses (`AFTERMATH_POSE_COUNT`, which the sweep
now reads off [game.h](src/game.h) — it was a literal in the switch, a literal in
the bash array and a comment above the array saying "four" while both said five,
and a `--page` above the count silently drew pose 1 and was logged `ok`, which is
the manual's own refusal that this screen never had) — `draw_player`
answers hacking first, crawling second and a bazooka before a sidearm, so one
frame holds exactly one of them, and the pose that covers the launcher is the pose
that stops covering the muzzle flash. The last two are a throw on a rung, and
they are there because `render_figures.c:draw_thrown_in_hand` was executed by
**neither** gate: see the note further down on the one figure that drifted onto
that list. A rung pose is also staged *on a rung* now — `on_ladder` is a flag
rather than a place, so page 4 spent its life drawing a man gripping a ladder
that was not there, and no counter can tell you that.
`--screen aftermath --level N` on a climb
stages the wall's own two hazards instead, a thrown object and a bird, because
both spawn on a timer and "whether two seconds caught one" is luck rather than
coverage. And on a floor with trunking on it the crawl is staged *inside a duct*,
because the louvres a shaft is worth anything for are laid back over the man
behind them by a pass of their own (`render_duct_fronts`) and no headless run has
ever put anybody in a shaft: a sweep that presses nothing crawls into nothing.
`--screen restroom --level N` is the same problem answered by the other switch:
which room a `U` opens on is decided by the sector's `THEME`, so a sweep pinned to
sector 1 drew the lobby's washroom four times and left the plant's, the archive's
and the penthouse's unexecuted — and the toilet prop `q` is in those three and in
no other map in the game.
`./chuck-editor --soak N` is the same switch on the other binary.

`./chuck --shot PATH` writes the frame to a BMP and closes, with `--shot-at
SECONDS`, `--shot-frames N` and `--shot-fps F` saying which frame and how many. It
is the only switch here that *produces* something rather than checking something:
all of the art is drawn at runtime, so a store page and a bug report have nowhere
else to get a picture of this game. In a capture the world runs on that rate as a
synthetic clock, one `1/F` step per drawn frame, which is what makes a burst play
back at the speed it was asked for. [tools/press_kit.sh](tools/press_kit.sh) is the
caller and [docs/tooling.md](docs/tooling.md#the-press-kit) is the write-up.

**`--shot`, `--soak` and `--screen` make the run a *scripted* one, and that is
part of the contract rather than a detail** (`GameRunKind`, `PlatformState.scripted`): shipped
defaults, no saved fullscreen applied, and neither `settings.cfg` nor
`progress.cfg` read or written. Without it a capture was the *window* — which is
what `screenshot_write` reads back — so a machine with `fullscreen 1` saved wrote
1024x706 where another wrote 800x552, pre-scaled by a non-integer factor that
pixel art does not survive, while the runner's CRT filter, reduced motion and
assists rode into the art with it. The other direction was a gate rewriting the
player's records, because `--screen cleared` finishes a sector. `tools/soak.sh`
holds the capture to `VIEW_W`/`VIEW_H` off the header, and there is a paragraph at
the end of this file on how both halves were measured.
`--screen` is in the list because it stages a *frame* rather than starting a run —
a cleared card on the last sector, a report with numbers nobody scored, a title
screen offering a resume nobody earned — and `game_soak_screen`'s own comment
claiming "nothing is banked" was only ever true of the sweep, which passes
`--soak` beside it. A hand running `--screen cleared` banked the sector its timer
ran down into. **A staged frame must not become somebody's save.**
`--level` deliberately
keeps the player's display and the player's save: a switch a script drives owes
the script a repeatable artifact, and a switch a hand drives owes the hand the
game they last set up.

`./chuck --seed N` pins the night, and it exists because a capture is a
measurement and a measurement has to be repeatable. Two things were seeded off
the wall clock — the simulation's own stream through `game_init`, and SDL's,
which the camera shake, the particles and the title screen's starfield draw from
— so the same command on the same commit photographed a different live card,
guards in different places and different decoration variants every time, while
`press_kit.sh`'s own header said the pictures could be *rebuilt* after a change.
Both streams take this seed now, `press_kit.sh` passes `PRESS_SEED` and names it
in the MANIFEST line so the command printed is the command run, and two press
runs off one commit are byte-identical in all 46 files — on one machine, which
was the whole of what this claim could promise until `--shot` stopped reading the
runner's settings; a seed pins the part of the frame that comes out of the RNG and
nothing else. It is the one numeric
switch here that had to be parsed strictly on its own account rather than by
luck: every other one decays to a value its own range check refuses, and
**nought is a perfectly good seed**, so `--seed abc` would have silently pinned
nought and said nothing.

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
  **And one screen was filed on the wrong side of that split for as long as the
  split existed.** `screen_seconds` sorts a name into a beat with a clock of its
  own or a card that "holds still until a hand moves it", and it derives the
  clock from a `#define` — so a duration that is a *literal* is filed as a card
  by default. `STATE_LEVEL_CLEARED`'s was `1.2f` inside `game_enter_state`, and
  that card does not hold still: it runs down and then calls `advance_level`. So
  `--screen cleared` spent 1.2 of its two seconds on the card and the remaining
  0.8 **simulating the next sector**, which is the one thing the comment above
  that case list says cannot happen. It is `LEVEL_CLEARED_DISPLAY_TIME` now.
  The other half of it is the staging: this state is only ever reached on the
  *last* sector — every other floor goes to the report or straight through a
  window — and the sweep staged it on sector 1, so the card was drawn over a
  world it cannot occur in and its tally line read `SECTOR 01 CLEAR` under
  sector 1's HUD. **A screen's default sector is part of naming it**, which is
  the lesson `--screen aftermath` already had and this row did not inherit.
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
chosen near the player when the staged sector's only `%` was twenty-eight tiles
from the spawn and a viewport is twenty-five wide — the man is moved to the patch
now. **A staged screen is not covered because it was staged; it is covered when
something has counted it.**

(Which sector that is has since moved, and moved by itself, which is the point of
having derived it: `soak_aftermath_sector` takes the first interior with both a
`%` and a dog on it, and the weak-wall pass written up further down this file left
sector 4 without a patch. It was sector 4 and it is sector 9, and nothing needed
editing for that to be true — the failure mode this replaces is a literal sector
number here going stale the first time a map moved.)

What is left is honest to write down rather than to claim: the pause and options
sheets' own row handlers in `game.c`, and `audio_toggle_mute`. They need menu
presses, and none of them is something `--screen` can name — a switch names a
state, and these are *transitions*. (`audio_stop_music` stood here too and has
since become covered, which is the sort of correction the paragraph after next is
about.)

**And that sentence was this file's own recurring defect one more time, because
nothing had measured it.** It was written from the renderer sweep, which is where
the work had been, and it reads as a statement about the shell. `make
coverage-shell` measures the shell — the game built instrumented, walked by the
same sweep, intersected with what the suite reaches — and the answer was **42
functions no gate executes**, against the 14 named above. Which is to say the
death, the continue, the write to the player's disk and the way out of a restroom
— the area where the *shipped* veteran-lives bug written up further down this file
was found by hand, and the area a reader of the paragraph above would have
believed was accounted for. The renderers, measured the same day, came out at
**394 functions and nought unexecuted**, so the claim they were the problem was
true and the claim they were the *whole* problem was not.

**And then that figure drifted, which is exactly what this file says a figure
written in prose will do.** The paragraph above is a *reading*, and it says so;
the note further down about the running line count says such a number is meant to
be re-measured by whoever adds a test and corrected in the same commit. Re-run
later it was **46**, and the four extra were not a regression in anything — they
were `screenshot.c` entire (`screenshot_write`, `shot_plan_open`,
`shot_plan_broke`, `shot_frame_path`) plus `main.c`'s `parse_shot_number`, which
had arrived with `--shot` after the sentence was written. The breakdown was stale
in the other direction too: `audio_stop_music` had become covered, and `game.c`
was twenty rather than eight, the twelve additions being the pause and options
sheets' own handlers the paragraph above this one names in words and never
counted.

**That the drift was a whole subsystem is the part worth keeping.** `--shot` is
the only switch in this binary that *produces* something rather than checking
something: there is no art in this repository to crop, so the store page, the
README and every bug report get their frames out of a running process. It was
outside both gates and unnamed here, which is the `make app` story from further
down this file with the object swapped — a break in it would have been found by
whoever was next cutting press assets. `tools/soak.sh` writes one still and one
three-frame burst now and **counts the files off the disk** rather than trusting
the exit status, because `shot_plan_broke` already fails on an unwritable frame
and the failure this cannot see is a capture that wrote nowhere and said it was
fine.

Measured after that: **41 functions no gate runs.** `audio_toggle_mute`, twenty
in [game.c](src/game.c) — `finish_player_death`, `continue_game`,
`game_save_progress`, `leave_restroom`, `game_apply_assist_everywhere`,
`game_resume_campaign`, `game_set_fullscreen`, `settings_current_row`, and the
twelve `game_pause_*` / `game_settings_*` handlers — and twenty in
[game_input.c](src/game_input.c), which is the whole gamepad path including
`turn_manual_page` and `toggle_fullscreen`. All forty-one need a menu press or a
pad in a hand, and neither is something `--screen` can name: a switch names a
state and these are *transitions*.

**And then a forty-second arrived and it was not a transition, which is the one
thing that sentence had ruled out.** Re-measured, the list was 42, and the extra
name was `render_figures.c:draw_thrown_in_hand` — a *renderer*, on a page that
had just finished saying the renderers were at 394 functions and nought
unexecuted. It came in with the flash-charge sweep written up at the end of this
file: the ladder throw pose put a grenade in his hand for all three throwables,
and the fix that split the prop from the animation is the reason that function
exists. The simulation half of that fix got two tests. The drawing half — the
half `draw_flashbang`'s own comment is *about*, that a charge has to be told from
a grenade at a glance because one of the two is going to kill whoever is standing
next to it — was run by nothing at all, because no headless run has ever
**carried** a charge.

Three things worth keeping. A pose *is* nameable: `--screen aftermath --page N`
had staged five of them for exactly this reason, so the answer was two more pages
rather than a new mechanism, and the figure is back to 41. The sentence above was
true when it was written and became false by a commit that never touched it —
which is why it now says what kind of claim it is rather than standing as a rule.
And **the paragraph that said "the renderers came out at nought" is the reason
nobody looked**: a subsystem certified clean once reads as a subsystem that stays
clean, and this one drifted the release after the certificate. Re-measure the
figure in the same commit as whatever moves it; that instruction is three
paragraphs up this page and it is the same instruction.

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
something in the test binary calls it.** `P` and `F` are on six shipped
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
seven hundred and twenty-three, the one written up in the middle of this section
to six hundred and ten, the pass at the end of it to five hundred and
ninety-one, the duct sweep written up below to five hundred and eighty-six, and
the two-parser fuzz written up at the end of this file to five hundred and
eighty-five. The RECORDS page's readouts took it back up to **five hundred and
eighty-eight** — three of those are `run_tally_format_record`'s `snprintf < 0`
arm, which is the same unreachable guard the three formatters beside it each
carry, and it is written down here rather than chased. The dry-press sweep
written up at the end of this file took it to **five hundred and seventy-five**,
twelve of them `gameplay_combat.c`'s and one `gameplay_world.c`'s.
That last one is a single line and it is worth saying why rather than rounding it
away: that test took `settings.c` from nineteen unexecuted lines to sixteen and
`progress.c` from fifteen to thirteen — five real error branches nothing had
reached — and gave four straight back, because the editor check it arrived beside
has arms of its own that the shipped campaign does not walk. **A number that
barely moves can still be two things that did.**
Re-measured with the flash-charge pass written up at the end of this file it is
**five hundred and sixty-three**, and the twelve are the honest kind: the three
new tests reach `player_carry_loadout` and the two throw branches that had never
said what left the hand.
The crawl-and-shaft sweep written up further down took it to **five hundred and
fifty-eight**, and all five are one file: `gameplay_interaction.c` went from
nineteen to fourteen, which is `case ITEM_FLASHBANG:` — the pickup arm of the one
item on the floor that no test had ever walked over.
The outside read written up at the end of this file took it to **five hundred and
thirty-one**, and the split is the whole of what the figure is good for.
Twenty-five of the twenty-seven are `gameplay_ai.c`, 103 down to 78: the
`find_dog_slot` arm that had never run and the reinforcement drip whose only
initialiser is on the shell side, so no test in the suite had ever started it.
Two more are `level.c` and one `enemy.c`, from the same drip reaching the door
timers. And one went back the other way — `sector_tally.c` from nine to ten, the
new accessor's own NULL guard — which is the honest kind of regression to have.
`editor_validate.c` did not move at all despite gaining a hundred lines of mine
rules, because those lines are covered; a total that stands still is two things
that did. **One real hole and a scatter of branches** is what a move of this size
usually is, and which is which is the reason the list prints per file rather than
as a number.
The crate-and-grace read written up at the end of this file took it to **five
hundred and twenty-seven**: three of the four are `gameplay_physics.c`, 14 down
to 11, which is `crate_position_clear` learning about the three moving surfaces
it had never asked about, and the fourth is `gameplay_climb.c`, from the grace
check finally driving a wall's own frame. `settings.c` stood still at sixteen
while gaining two functions, which is the "a total that stands still is two
things that did" case again — both are covered by the new action-row test.
The facade-grace and reveal-duration read written up at the end of this file took
it to **five hundred and twenty-five**, and both are `level.c`:
`level_reveal_hold_for` arrived with a test that drives it over all seventeen
maps, so the one new function on the SDL-free side came in fully covered rather
than adding to the list. Which is the smallest move on this page and the one
worth naming for its shape: the *other* two fixes in that pass are a character in
a map and a parser in `main.c`, and neither can move this figure at all — one is
data and one is on the far side of the SDL boundary. **A pass that fixes three
things and moves this number by two has not done less work than the number
says.**
What is left is honest to read rather than to claim, and reading it is the whole
point. **`none` is not the target and never was.**

That running figure is itself a number in prose that nothing holds, and it had
already drifted — it said seven hundred and twenty-three while the target printed
seven hundred and sixteen. Worth knowing what kind of claim it is: this one is a
*reading*, not a rule. It is meant to be re-measured by whoever adds a test and
corrected in the same commit, which is why it is written here in words rather than
checked by a script — a gate on it would be a number people learn to move, which is
the argument this whole section is built on.

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

**And it skipped the five climbs, behind a comment saying they were covered
elsewhere.** The sentence read "the facade has its own frame and its own bot, in
`facade_bot_reaches_window`", and that bot is not a frame: it drives
`gameplay_climb_update_player` and nothing else, because what it exists to ask is
whether a map has a dead end. So `update_facade_playing`'s order — the player,
then the hazards and the wind, then the pickups — was run by nothing that pressed
anything, on a third of the campaign, while the check that would have covered it
said in writing that something else did. Same defect as the paragraph above it and
the paragraph below, one directory over: **a check whose name or comment overstates
its reach is worse than a missing one, because a missing one leaves a reader
curious.** Both modes are driven in their own shell function's order now and the
counts at the end require both to have been walked.

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

**And the list was read once more, and this time one of them was a shipped bug.**
Six findings, and the order they are written in is the order of how much they cost.

- **A veteran run stopped being one after its first death.** `campaign_reset` was
  handed the `veteran` flag, spent it on the opening lives and continues, and
  forgot it; `campaign_accept_continue` is the *other* place lives are handed out
  and had nothing to ask, so it handed out `PLAYER_LIVES`. Since
  `VETERAN_CONTINUES` is nought, a veteran's very first death takes the
  score-reset branch — and came back with three lives. The mode
  [docs/screens.md](docs/screens.md#the-options-sheet) describes as three numbers
  held two of them for exactly one mistake, which on a one-life run is the whole
  run. `test_the_veteran_run_is_three_numbers_and_no_more` had checked
  `campaign_reset` and stopped there: **checking the opening state of a mode is
  checking it for as long as nothing has happened yet.**
- **The options sheet's *reader* had six of its seven arms dark.** The write-up
  further up this file is about `settings_adjust` being tested two rows at a time;
  `settings_value_bool`, the function that decides whether each switch prints ON or
  OFF, kept exactly that treatment and nobody noticed, because the fix had been
  filed as done. Its every caller is in `game_render.c`, so the suite had asked it
  one question in its life. A miswired arm shows the player the wrong state for a
  switch they just flipped, on the one screen whose whole job is reporting state.
  **Fixing one half of a symmetric defect is the most reliable way to stop anybody
  looking at the other half.**
- **Three twins with one test between them, all in the same file.** The rocket's
  swept box is tested against masonry and guards and had never met a crate, a gas
  canister or a dog — thirty-six lines in three near-identical loops. A guard's
  round had never met a crate, while the player's round meeting one was covered.
  And `test_ladder_explosives_follow_aim_direction` walked the rocket and the
  grenade past two more copies of the same twelve lines: the flash charge and the
  bolt, neither ever thrown from a ladder by anything. It is
  `test_every_ladder_throw_follows_the_aim` now, because the old name was true of
  what it did and false of what it was for.
- **A dog's body could be dragged and nothing had ever dragged one.**
  `dragging_is_dog` appeared in the suite once, as `false`; `body_slot` ran at half
  its regions. An animal is a body the AI investigates like any other and `W` puts
  one on ten of the seventeen sectors, so this is half a mechanic covered on the
  half of the world that happens to be human.
- **The monkey skipped a third of the campaign behind a comment saying otherwise**
  — written up in its own paragraph above, because the shape matters more than the
  gap.
- **And four numbers that were literals beside named constants.** Three throw
  branches wrote `0.18f` next to a `PLAYER_KNIFE_ACTION_TIME` that is also 0.18,
  the pistol wrote `0.12f`, the muzzle-flash threshold `0.055f` stood in five
  places in `render_figures.c` and a sixth as a number inside a comment, and the
  underarm arc was solved out longhand three times — with the third copy carrying a
  comment claiming it was "the same arc the grenade is thrown on", an equality
  nothing held. All named now, the arc is one `throw_arc_speed`, and the two
  relationships a header cannot state (a throw lasts a knife stroke; a pose
  outlasts the flash drawn on it) are in the suite, because a float comparison in a
  `_Static_assert` is a GNU extension and this tree is `-Wpedantic`.

Two more came out of the same pass and neither is a defect — both are facts the
tree was resting on without having written down, which is the other thing reading
this list is for.

- **A standing round flies over a dog, and that is the canister's own mechanic
  wearing a different coat.** The shot leaves the hand at `PLAYER_H * 0.35`, which
  puts its underside 0.8px above anything 16 tall standing on the floor;
  `DOG_H` and `GAS_CANISTER_H` are both sixteen. The canister half is documented
  and pinned, the animal half was neither, so the sidearm quietly does nothing to
  the fastest thing on a floor unless Chuck crawls first — with the blade, a
  ladder shot and any blast as the other answers. `test_the_shot_line_is_chest_high`
  states the cause once and both consequences beside it. **A margin under a pixel
  carrying two mechanics is a coincidence until somebody writes it down.**
- **Every underarm throw rises at the cap, and the formula above it is inert.**
  `throw_arc_speed` solves the rise from the horizontal speed and then clamps it,
  and at the three speeds the game ships the solve is 335, 348 and 261 against a
  cap of 220. So all three throwables leave the hand on exactly the same arc, the
  lower clamp needs a throw over 2600px/s to be reached at all, and the number that
  moves the feel of a lob is the cap rather than the strength — the opposite of what
  the code reads like. Pinned by `test_every_underarm_throw_rises_at_the_cap`, so if
  a speed change ever brings the solve inside the clamps, that arrives as a failing
  test rather than as a bolt that lobs differently from a grenade.

Every one of these was **checked by breaking the thing it guards and watching the
new test fail** — thirteen mutations — and the sweep also ran a wider monkey than the
suite keeps: all seventeen sectors, eight seeds, sixty seconds each, invariants on
NaN, world bounds, hearts, ammunition, score and every entity count. That found
nothing, which is the useful half of the answer.

**And the list was read again, and this time what it found was mostly the shape
of the checks rather than the code.** Five findings and one deletion; none was a
shipped bug, and every one of them was a mechanic or a rule the suite would not
have noticed the loss of. Each new test was **checked by breaking the thing it
guards and watching it fail** — nine mutations, and the interesting one is the
mutation that *passed*: it is why the fourth test below was rewritten before it
was believed.

- **A twin with one test between them, on the way out of seven sectors.**
  `gameplay_player_reached_exit` answers the *window* first and returns, so every
  caller the suite ever had — its own bots included — handed it a map with a `Y`
  on it. Measured: 66 900 executions of the window branch against **nought** of
  the door's, including the line that refuses a *locked* door, which is the whole
  of what a card is for. Seven of the seventeen sectors leave by their stair core
  and the last of them is the roof, so what nothing checked was the finish
  condition of the floor the campaign ends on.
  `test_the_stair_door_is_a_way_out_only_once_it_opens` is the missing half; the
  behaviour was correct all along, which is worth saying plainly.
- **A guard was sent to look at a body 4 390 times and never once arrived.** The
  suspicion block in `update_enemy_pursuit` ran constantly; the branch inside it
  that fires when the man *reaches* the spot was dark, so the shortening to
  `ENEMY_INVESTIGATE_LOOK_TIME`, the turn on the spot and the drop back to patrol
  were staged by nothing. That is the beat the quiet route is played against —
  hiding a body is worth doing because the witness walks over, looks round and
  **gives up** — and a guard who arrived and never left, or who never stopped
  walking, is one line from either. It is the "reached but truncated" kind this
  file's own sweep named, on the mechanic the whole route is built on.
- **A rule that had been prose since the sheet existed.**
  [levels/LEGEND.md](levels/LEGEND.md) says to put the docket sheet "somewhere
  that costs a detour rather than on the route to the door". Nothing measured it,
  and seven of the twelve were sitting on a *shortest* path to the way out with
  sector 12's costing one step: the one collectable in the game that is meant to
  be a decision was, on eight floors of twelve, picked up by walking to the door.
  The sheets moved — the campaign runs +11 to +92 steps now, with sector 4 the
  closest to the bar at 22 against a 136-step walk — and
  `test_the_docket_sheet_costs_a_detour` holds them there.
  Two things worth taking from it: **a rule about where a thing goes is as much a
  rule as one about what it does**, and the measurement needs a flood *from the
  sheet* rather than a flood from the door read backwards, because a step off a
  ledge is a one-way edge and the model is full of them.
- **The route model's two claims about the body, held by nothing.**
  `route_neighbours` decides that a hole is clearable one tile wide (two with a
  row spare overhead) and a spike bed hoppable one tile wide. Those are claims
  about `PLAYER_JUMP_SPEED`, `PLAYER_WALK_SPEED` and `GRAVITY`, written as
  literals in a different file — and this model is what certifies every shipped
  map as playable. Measured, the margin is one tile in each direction: the
  simulation clears a three-tile hole and pays a heart for a two-tile bed, so the
  model is on the conservative side, which is the side to be on. **The first
  draft of the test pinned the wrong thing** — hard-coded at one and two tiles it
  passed happily when the model was widened to three, which is exactly the
  mutation it should have had an opinion about. It asks the model for its widths
  now and requires the simulation to deliver each one, so widening the spike hop
  to two tiles fails on the change instead of on a player.
  **And the same bullet was asked the other direction, where it was false.**
  Both claims above are the model promising something the body has to deliver.
  [levels/LEGEND.md](levels/LEGEND.md) also lists what the model *refuses*, and
  it closed that list with "anything the rules below say is out of reach really
  is out of reach" — which is the converse, and is a claim about the body rather
  than about the model. Half of it is true: two spikes cost a heart. The other
  half is not. A two-tile step up is 64px and the jump apex is 68.7, so the body
  can land two tiles up, the model will never route it, and an author reading
  that sentence would have drawn a pocket up there and called it unreachable.
  **The suite already half knew** —
  `test_the_jump_apex_does_not_depend_on_the_frame_rate` has asserted
  `apex > 2.0f * TILE_SIZE` since it was written, as a sanity guard, under a
  comment explaining it as proof that the man left the floor. The number in it
  was the whole of what the authoring page three directories away was denying.
  Which side to be on was never in question and the model keeps refusing:
  under-promising certifies fewer maps and never certifies an unplayable one.
  What changed is the wording and a test that holds both halves — the body
  clears two tiles, the model refuses two tiles, and the body does *not* clear
  three, which is what makes the model conservative by exactly one tile rather
  than wrong by any number of them. Measured with a flood widened by that one
  move, the campaign gains **nought** reachable tiles on every shipped sector,
  so nothing in it rested on either reading. The first draft of *that*
  measurement was wrong in an instructive way: it required the tile diagonally
  between take-off and landing to be passable, and that tile is the ledge's own
  face. **A model of a move that forbids the ordinary case of the move measures
  nothing**, and it agreed with the right answer for the wrong reason.
- **And the dead zone and the diagonal, six lines behind `SDL_GetGamepadAxis`.**
  `make coverage-shell` says the gamepad path is executed by neither gate, and
  most of it does need a pad in a hand. `menu_stick_direction` did not: what it
  needs is the two numbers a pad reports, which is the same sentence
  [pad_hint.c](src/pad_hint.c) is already built on. The reading stayed with SDL
  and the decision came across as `pad_stick_direction`, where the suite drives
  it — the dead zone on both axes and in both directions, the corners, and the
  exact diagonal that has to resolve *vertically* because every cursor it feeds
  runs down a column. Both halves of that are things a player feels and neither
  had a test: a tie resolving sideways is a stick pushed corner-wise moving
  nothing on three screens.
- **And a tile type nothing parses to.** `TILE_FALL_PLATFORM` sat in
  [level.h](src/level.h) as the only mention of itself in the tree, carrying a
  comment describing the mechanic `FallPlatform` in `LevelRuntime` actually
  implements: `F` parses to `TILE_EMPTY` plus a runtime entry, because a panel is
  a thing with a position and a velocity and the parsed map has to stay exactly
  what the file said. An enumerator nothing produces is a claim about the map
  format that is not true of it.

**And the newest mechanic was read the same way, which found the one thing on
this page that was costing a player something while it was written.** The duct
arrived with a split solidity rule, an editor check, a route-model edge, a
renderer pass of its own and two tests — and every one of those asks about the
*horizontal* crawl, because that is the direction a shaft is for. Nothing had
asked what trunking does to a man moving up or down through it. It does the same
thing in both directions.

- **Crouching on the lid rocked the player 240 times a second.** The crawl lowers
  the box, `level_blocks_stance` opens a duct to a crawler downwards as well as
  sideways, so the tile he was standing on stopped holding him; `on_ground` went
  out with it, `want_crawl` requires `on_ground`, and the very next step stood him
  back up onto the trunking and handed `on_ground` straight back. Every run on
  sector 12 has its storey's own air above it, so every one of them was a walkway
  where the crouch key did that: the pose the renderer draws alternating, the box
  14px taller and shorter by turns, and `crawling` — one of the two ways of being
  hard to see — true on half the sight checks a guard makes. **A tile that is
  solid to one stance and open to the other can take the floor away by being
  crouched on**, and until the duct there had never been one, which is why the
  crawl had no reason to ask.
- **One press of JUMP left the shaft through its roof.** A rise is resolved in the
  posture the player is in too, so a duct was open upwards as well.
  [levels/LEGEND.md](levels/LEGEND.md) had already spent a sentence on this —
  *the crawl is the only move it allows from inside a shaft* — and the sentence
  was true of the route model and false of the game. So "a duct with one mouth is
  not a route" described nothing, and the louvres the whole bet is made against
  could be lifted at any tile. This is the `gameplay_restore_checkpoint` shape
  from further up the page in a new place: **a claim written about the model
  reads as a claim about the simulation, and only one of the two is holding it.**
- **And the SPAWNS parser was written down twice with the guards on one copy.**
  `level_load_data` refuses a token that is not a number, a digit run that will
  not fit an `int`, and junk on the end of one; `editor_doc_parse` stopped at the
  first non-digit without a word and multiplied by ten until it wrapped. So
  `SPAWNS -1 4` opened in the editor as *no* spawns at all — a hand-edited map
  could be opened, drawn on and saved with its door counts gone, because
  `editor_doc_serialize` writes the line only when there is something in it — and
  `SPAWNS 99999999999999` is signed overflow in a translation unit this
  repository already builds under UBSan, reached by nothing because the fuzz
  corpus stopped at seven digits. [tools/check_lists.py](tools/check_lists.py)
  holds the lists written down twice; a *parser* written down twice is the same
  class and has no script.
- **And the refusal it printed named the wrong thing.** One `bool` fed one
  message, so `SPAWNS -1` on a map with no doors printed `expected 0 values,
  found 0` and refused — a refusal an author cannot act on, quoting two figures
  that agree with each other. A malformed token and a miscount are two faults and
  want two sentences.

Three things are worth taking from it. The first is that the two gameplay bugs
are **the same bug in two directions**, and the write-ups above would have found
one of them: a check reading `crawling` once would have passed on whichever step
it landed on, so the assertion has to be *how many times it changed*. The second
is that the fix's whole risk is in the other direction — a rung, a cracked panel
and a moving platform hold a player up with no solid tile under his feet at all,
so "the tile under the feet must be masonry" would have taken the crawl away on
the seven floors carrying a `P` or an `F` to close a hole on one, and that case
is in the test because it is the mutation that matters. The third is that the
editor's answer comes in **two halves** and only one of them is the parser: a
check asking merely "does the editor end up refusing this" passes with the parser
fully broken, because the validator catches the miscount the discarded values
leave behind — and the values are still gone. It is separated by counting the
tokens on the line, which needs neither parser's opinion.

All four were **checked by breaking the thing they guard and watching the new
tests fail** — nine mutations, including two that pass a weaker version of the
same test and are the reason it is written the way it is.

**And then the same sentence was read once more, because a sentence that has
already been wrong once is the best place to look.** The duct's cost is stated
twice — once in [levels/LEGEND.md](levels/LEGEND.md) as a list of what a player
"cannot" do inside trunking, once in [docs/gameplay.md](docs/gameplay.md) as a
list of what "a crawl already denies". The JUMP clause of the first was found
prose-only and fixed in the code, written up above. **Between them the two
sentences made six claims and three of them were false**, and nothing in the tree
had an opinion about any of the six, because a list of things that cannot happen
is the one kind of claim no test ever accidentally covers.

- **"A crawl denies the sidearm" is the opposite of true, and the game's own
  manual says so.** The movement sheet sells the posture as *"the only way to hit
  something sitting on the floor"*, the combat sheet says *"Crawl and shoot a GAS
  CANISTER"*, and `test_the_shot_line_is_chest_high` exists because a standing
  round passes 0.8px over anything sixteen tall on the floor. So the page
  explaining the duct denied, by name, the mechanic two player-facing sheets are
  built on. **Two documents, one question, two answers** — this file's most
  reliable smell, pointing at a page rather than at a screen for the first time.
- **"A crawl denies the hack" was never enforced by anything.**
  `gameplay_prepare_terminal` gates on `on_ground && !on_ladder` and says nothing
  about posture; driven against every console in the campaign a crawler is in
  range of exactly what an upright man is. Contrast `player_can_drag`, which
  spells `!player->crawling` out — which is why the third clause was the one that
  survived. What is true is narrower and is about the tile: a `T` cannot be
  *inside* trunking, because a console needs a tile and the duct is the wall.
- **"Cannot fire from inside it" is not a refusal, it is a cost.** The trigger
  works. The round leaves the clip, trunking still answers `level_is_solid`, and
  measured it travels **0.0px** — against 5.86 tiles for the same press taken on
  the lid. The launcher spends the sector's rocket and puts its own blast on the
  man holding it: two of three hearts.

The last one is the one to be careful about, because the *game* is right and only
the sentence is wrong: a rocket fired with the muzzle in masonry does this
anywhere in the building, and this page has said since `apply_blast` was unified
that the player is inside his own blast radius. What earns a shaft a paragraph is
that it is the one place the muzzle is **always** in masonry — there is no
stepping back from it — so the cost is certain rather than a mistake, on the floor
named after ducts.

Three things worth keeping. **A claim about what a mechanic is *for* reads as a
claim about what the simulation refuses**, and this is the third time that exact
substitution has been found in these two sentences; the honest fix is to say which
function refuses each item, which is what both now do. The direction matters and
the two were fixed in opposite places — the JUMP clause in the code because the
game was wrong, these in the prose because the game was right, and telling those
apart is the whole of the work. And **the suite half-knew all along**: the
canister test had been firing from a crawl since it was written, one directory
from a page saying that could not happen. `test_what_the_crawl_takes_away` and
`test_a_shot_from_inside_a_shaft_reaches_nothing` hold all six claims now, each
with the control beside it that makes it an assertion about the posture rather
than about the fixture — checked by five mutations: the hack gate made to refuse a
crawler, the drag made to allow one, the sidearm made to refuse one, trunking made
porous, and the charge's sound put back to the grenade's.

**And the flash charge was half-wired one more time, in the two senses.** The
write-up at the end of this file found it missing from four places at once and
fixed them; the fifth was the pickup itself. `case ITEM_FLASHBANG:` in
`gameplay_collect_items` was **compiled and never executed** — every test in the
suite hands itself a charge by assigning `player.flashbangs`, and no test had ever
picked one up off a floor, so `test_a_pickup_that_would_be_wasted_is_left_alone`
covered the grenade, the rocket and the medkit and stopped one item short of the
list its own comment describes. And what it played was `SFX_PICKUP_GRENADE`: ammo,
grenade, medkit and launcher each have their own sound, and the one item in the
game carrying a written rule that it must be told from a grenade at a glance
announced itself as one. It has `SFX_PICKUP_FLASH` now, in the register its own
detonation lives in. **A pickup is a sound as well as a drawing**, and the sense
that cannot see `draw_flashbang` is the sense nobody checked.

The stale sector list in that test's comment is the footnote and it is the sharpest
one on this page. It read `10, 12 and 15` — 15 being a facade with a single medkit
— which is the *same* wrong list `gameplay_interaction.c` was corrected for, and
[check_docs.py](tools/check_docs.py) already held that copy, keyed on that
paragraph's own sentence about the restrooms, while reading `tests/` for two other
lists in the same run. The script had the file. The script had the phrase. What
was missing was two lines of entry, because **the fix landed on one copy of a
sentence and the check landed with it** — which is precisely how the other copies
stop being looked at. Held now.

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

**And the tree had only ever been compiled by one compiler, which is the same
shape as the editor above.** `make win` cross-builds the game with
mingw-w64, and the first attempt did not finish: [manual.c](src/manual.c) called
`snprintf` with no `<stdio.h>` anywhere in it, having compiled for a year only
because clang's SDL headers happen to drag one in. Four more diagnostics came with
it, none a bug a player would have met and all of them real — two loop counters in
[level_art.c](src/level_art.c) set and read by nothing, a `%zu` that is correct C
and that gcc checks against the *platform's* printf rather than SDL's, and two
format buffers in [sector_tally.c](src/sector_tally.c) whose worst case does not
fit. The conventions below have called this tree warning-free for as long as they
have existed; it was warning-free under one compiler.

**And a staged screen was being covered under a menu.** `--screen aftermath`
stages a floor a few seconds after it went wrong — the bodies, the opened patch,
the alarm light, the crawl, the launcher — and freezes it in `STATE_PAUSED`, which
also draws the pause sheet over all of it. The draw calls ran, so the coverage
number was right and the frame was a picture of a menu: **a counter cannot tell a
frame that was drawn from a frame anybody could look at**, and nobody had looked
until `--shot` existed. `--screen pause` is the name that draws the sheet, so
skipping it on a staged frame costs nothing.

**And then a release stopped happening, on the one platform a Mac cannot build
for.** `make linux` died in SDL's cmake with `Couldn't find dependency package for
XTEST` after twenty lines of successful checks: the apt list in
[payloads.yml](.github/workflows/payloads.yml) had no `libxtst-dev` in it, and had
never had one. Three things are worth keeping, and the third is the general one.

- **The comment above that list had the diagnosis and not the cure.** It says, in
  as many words, that "SDL turns a missing X extension into a hard configure error
  rather than a backend it goes without, so this list is what a player's SDL has
  to have been built with" — written by somebody who had understood exactly this
  failure and then enumerated the extensions from memory. A list whose own comment
  explains why it must be complete is a list nobody re-derives.
- **`libxrender-dev` was missing too and never failed**, because `libxcursor-dev`
  depends on it. So one of the two holes was invisible and would have opened on an
  apt resolution nobody in this repository controls — the same shape as the
  editor's three unsanitized files above: present by luck, and luck that reads
  exactly like a decision.
- **The fact itself lives in somebody else's tree**, which is why the fix is not a
  check. What SDL requires is SDL's cmake to say, and no gate here can hold a list
  against it; the most this repository can do is make the *cost of being wrong* one
  line instead of a minute. `preflight_x11` in
  [packaging/build_linux.sh](packaging/build_linux.sh) stats eleven headers before
  the clone and prints the apt line for whatever is missing, and it says out loud
  that it is advisory: **it can be behind, it cannot be wrong**, because a list it
  has fallen behind on degrades to exactly the cmake error that prompted it.
  What *is* held is that the script and the workflow agree —
  [check_lists.py](tools/check_lists.py) fails if the preflight names a package the
  workflow does not install, which is the preflight working and the release still
  not happening. And the script now refuses to run on a non-Linux at all, first
  thing, because it links with the host `cc`: on a Mac it would otherwise have
  built a macOS binary, tarred it as `linux-x86_64` and printed `upload this`.

**And the options sheet was telling the player the opposite of what the switch
does.** The `VETERAN` row read `FASTER CREW, ONE LIFE, NO CONTINUES. NEXT RUN.`,
and two of those three numbers reach the run in progress: the pace on purpose, and
the lives by nobody's intention. `campaign_accept_continue` is the *other* place
lives are handed out, it asks `campaign->veteran`, and `apply_assist_to_state` has
kept that flag on the switch ever since the shipped bug where it did not — so one
flip and one death later, a continue hands out `VETERAN_LIVES` instead of
`PLAYER_LIVES`. Flipping a switch labelled NEXT RUN costs two lives in this one.

The behaviour is right and stays: a veteran continue handing back three lives is
the mode expiring on first contact, which is what put the flag on `CampaignState`
to begin with. What was wrong was every sentence about it. The comment at `case
SETTING_VETERAN:` in [game.c](src/game.c) stated flatly that the lives
"deliberately do *not*" reach a run in progress, called doing so "the one thing on
this sheet that can cost a player something they already had", and finished by
citing the row's own detail line as its evidence.
[docs/screens.md](docs/screens.md) argued **both sides two paragraphs apart** —
that the flag is live and follows the sheet in both directions, and then that the
lives do not. And `test_the_veteran_run_is_three_numbers_and_no_more` had been
*asserting* the live behaviour the whole time. So the suite was right, the code was
right, the docs contradicted themselves, and the one of the four a player actually
reads was the wrong one.

Three lessons, and the middle one is new to this file.

- **A comment that cites a user-facing string as its justification has made that
  string part of the invariant**, and nothing was holding the two together. The
  argument read as a check for as long as anybody skimmed it. That is this file's
  recurring defect with the object swapped: not a check reporting coverage it does
  not have, but a *rationale* reporting agreement it does not have.
- **A page long enough to argue with itself will.** `docs/screens.md` is nine
  hundred lines and the two claims are thirty apart; both were written by somebody
  reading the code, months apart, and neither reader scrolled. The answer is not a
  shorter page, it is that the disagreement had a testable consequence and no test.
- The fix is `test_the_veteran_row_says_when_it_bites`, which asks the *simulation*
  which world it is in and requires the words to match — so latching the flag at
  `campaign_reset`, which is a defensible change and the one the old comment
  thought had been made, fails until the row says so too. It holds in both
  directions and there is no list of which string is right.

**And there was a fifth copy, on the page a stranger reads first.** The paragraph
above counts four places the claim lived and fixes three of them; [README.md](README.md)
kept describing the old reading for a release after the row was corrected —
"the faster pace takes hold in the sector you are already standing in, while the
lives wait for the next run so nothing on that screen can take something you
already had", which is the retired comment's argument almost word for word. Every
gate was green over it: `test_the_veteran_row_says_when_it_bites` holds the *row*
and cannot see prose, and [check_docs.py](tools/check_docs.py) had no opinion
because this is not a sector list or a derived figure.

Two things worth keeping. **A sentence written up as fixed in four places is a
sentence nobody counts a fifth time** — which is this file's own "these copies must
agree" defect, and the reason `BUILDING_FLOORS` two sections down is a `#define`.
And the direction is now read out of the row itself and required of the prose *in
both directions*: `veteran_row_detail` in `check_docs.py` greps the `VETERAN`
row's detail out of [settings.c](src/settings.c) and fails if the README says the
wrong one of the two things, or says both. "Must contain" alone would have passed
`docs/screens.md` in the state it was found in, which was arguing both sides two
paragraphs apart.

**And the one place a fact like this can still hide is a store page**, which is
why [itch/](itch/) is in that script too. Worth re-reading whenever the mode
changes: the shop copy states the veteran switch in one sentence and nothing about
`page.html` is derived from `page.md` except by `check_lists.py` holding the two
copies to each other.

**And the building's height had six copies and an owner nowhere.**
[docs/story.md](docs/story.md) carried a paragraph insisting the height "is stated
in four places that have to agree" — a paragraph that had *itself* previously said
three and left the credits roll out, which is how the roll came to state the
campaign's length from memory as well. That half got
`test_the_credits_say_the_campaign_they_roll_over`. The height got the paragraph,
and there were six places rather than four: the title screen's tagline, the men
shouting off the facade, the roll twice, the README, and the store page in both of
its copies. [check_docs.py](tools/check_docs.py) derives the campaign length, the
climb count, the hearts, the crew, two readings off the night clock and the length
of the night — and had no opinion about the number the game opens on.

**A sentence whose entire content is "these copies must agree" will miscount the
copies**, because nothing counts them; that is the whole argument for this
paragraph being a `#define`. `BUILDING_FLOORS` is in
[game_config.h](src/game_config.h) beside `CAMPAIGN_SECTORS` — the two are
deliberately not derived from each other, since a sector is a stretch of the climb
and not a storey, which is exactly why the pair drifts in prose. It is computed
with nowhere and exists to be checked, the way `INTEL_ARC_SECTORS` does. The two
SDL-free tables are held by `test_the_tower_is_one_height_everywhere_it_is_said`,
which searches both for *any* line counting floors rather than working from a list,
so a fifth line is checked by having been written. Two counts on the store page
went the same way and had the same excuse — `nine sector controls` and a
`ten-sheet` manual, both derived for `README.md` and `docs/screens.md` and never
asked of the shop. **The page written for people who are not reading this
repository was the page held to the least.**

**And the switch that exists so a script can run the game would run forever.**
`--soak abc` printed `--soak expects a positive number of seconds` and then opened
the title screen and sat there — permanently, because a headless process receives
no events and nothing was ever going to close it. `parse_soak_seconds` answered
nought to both "not asked" and "asked and unusable", so the refusal was a message
and never a status. That is the failure every workflow in this repository spends
its `timeout-minutes` on, produced by the one switch whose only caller is a
script: a step that neither finishes nor fails, reporting the clock instead of the
cause. `--page` had it too, and worse in kind — `--screen manual --page abc` drew
sheet one and the sweep logged `manual sheet ok`, which is a check reporting
coverage it does not have, on the switch written to end exactly that. Both return
a third answer now and `SDL_AppInit` refuses on it, the way `--screen bogus`
already did. `--level` deliberately keeps the old shape: **a switch a script
drives owes the script an exit code; a switch a hand drives owes the hand a
running game**, and that one is the editor's playtest button.

**And the only two parsers that read a file the game did not write had a dozen
hand-picked lines between them.** `test_the_loader_and_the_editor_survive_nonsense`
generates three thousand files nobody meant, on the argument that the one input
the editor exists to open is a file half-finished or not a map at all — and it was
the only generated corpus in the tree. Meanwhile a map is embedded in the binary
and cannot be corrupted by anything short of a corrupted binary, while
`settings_parse` and `progress_parse` read `SDL_GetPrefPath` and are as damaged as
a disk that filled up, a process killed mid-write, or somebody with an editor and
an opinion about `challenge_veteran`. **The corpus was pointed at the input that
cannot rot.**

`test_the_players_own_two_files_survive_nonsense` is four thousand generated files
through both, and the property worth having is the second one rather than the
absence of a crash: parse, serialise, parse, serialise, and require the last two
to be identical bytes. A value that survives parsing but not serialising, or that
comes back different the second time, is a setting that **changes by itself on the
launch after the one that repaired the file** — and nothing was asking that of a
value which arrived broken. It found no live bug, which is the useful half of the
answer; the mutation that proves it earns its place is a parser accepting a sector
time just under `PROGRESS_MIN_TIME`, which the serialiser then writes back at
`%.2f` and the next parse drops. Every hand-picked line in the suite passes that
mutation.

**And the last one is a design rule that had been settled prose for as long as the
mechanic existed.** [docs/levels.md](docs/levels.md) disposes of the weak wall in
one clause — the route model counting a `%` as wall in both directions "is what
keeps a `%` a shortcut and never the way out". The second half is checked by the
model itself, three ways over. The first half was checked by nothing, and on four
of the seven interiors carrying a patch it was false: opening every patch on the
floor shortened the walk to the way out *and* to every pickup by nought steps.
Sector 10 held six of the campaign's seventeen patch tiles, the most in it. The
other three saved 7, 15 and 25 against walks of 80, 133 and 19, so the mechanic
worked where it worked.

Sector 2 is the one to read, because it shows how it happens rather than that it
did: its patch was a two-tile partition across the basement, and a paired `D` door
twelve tiles away already crosses that same partition for free — so the loud route
past the wall duplicated the quiet one, and taking it cost the sector's only
bazooka rocket. Nothing was wrong with the tile. **It is the floor around it**,
which is the one thing no rule about a single tile can see and no author can hold
in their head, and it is why this rule and the docket sheet's both live in the
route model rather than in the grid parser. The two are mirror images: `*` has to
*cost* a detour and seven of twelve were on a shortest path to the door; `%` has to
*save* one. Both were prose for exactly as long as they went unmeasured.

`check_weak_wall_shortcut` in [editor_validate.c](editor/editor_validate.c) opens
every patch, floods again and notes a sector where nothing got shorter — nought
being the threshold because a shortcut that shortens nothing is not a shortcut *in
any degree*, rather than because somebody picked a bar. The docket's rule needed
one; this one must not invent one. And it stays a **note**: the model counts steps,
a patch under an alarm is bought with risk rather than distance, and whether a
partition is worth a rocket when the floor is red is a judgement the tool has no
standing to fail an author's build over.

**And then the four sectors were fixed, which the paragraph above had been
carefully explaining why nobody had to do.** That explanation is right about the
*editor* and was doing double duty as a reason for this game to ship four floors
of patches that measure nought — a note an author reads once and four entries an
author learns to scroll past are different things, and five of the campaign's
seventeen sectors printing the same note is the second. Fixing it produced two
different answers and the split is the lesson:

- **Sector 2's patch moved.** It is at the foot of the partition beside the start
  now, where blowing it drops Chuck through the slab into the basement instead of
  walking the long way round through the door pair: 43 steps of a 56-step floor,
  the best patch in the campaign, and it gives the `D` pair the job of being the
  way back up.
- **Sectors 4, 6 and 10 lost theirs.** Every position on all three floor plans was
  searched — vertical and horizontal, two tiles and three, each candidate run
  through the whole validator — and the best saving available anywhere on any of
  them is **two or three steps**. Those floors carry ladders at four or five
  columns each. That is a good thing for a floor to be built on and exactly why a
  `%` there promises something the plan cannot deliver, so the honest fix is not a
  better position, it is masonry. The campaign carries four patches now — sectors
  2, 9, 12 and 14, worth 43, 7, 15 and 25 — and the note fires on nothing.

Three things came out of doing it, and the first two are the ones to remember.

**The check could be satisfied by making the map worse, and the search found that
by trying to satisfy it.** `distance_to` falls back to "the floor a thing drawn in
mid-air is collected from" — right for a card hanging over a walkway, and wrong for
*the same tile measured twice*, because a patch can stop a tile being somewhere
anybody can stand. Open a hole in the slab under the way out and the way out is no
longer a standing cell, so the flood stops reaching it, so the fallback answers
with the landing below, which is nearer. Sector 4 had a position worth an apparent
**eighty** steps of its 136-step walk, the best on the floor by a factor of three:
the slab under the window the sector leaves by, which the patch would have removed.
A check an author can satisfy by damaging the map is worse than no check, because
it is the one they reach for when the note will not go away. **And the suite's own
copy of the same measurement was honest the whole time** — it indexes the flood
directly and requires both readings to have landed — which is what made the
difference invisible: two readings of one question, and the wrong one was the one
an author reads. `test_a_patch_that_deletes_the_way_out_is_not_a_shortcut` holds
both halves, including the half a stricter rule breaks first, which is that a
genuine shortcut still clears the note.

**And nothing had ever looked at the floor the patches leave behind.** Every route
question in this tree is asked of the map *as authored*, because that is what a `%`
is to the model — and the hole lasts as long as the visit does, so for most of a
sector the player is walking a floor plan no check had seen. What that misses is
the one thing a hole can do and a doorway cannot: **drop somebody**. Sector 2's new
patch has a slab tile in it and is therefore a one-way fall, and for an afternoon
the only thing establishing that it was survivable was a throwaway program.
`check_opened_walls_leave_a_way_out` asks it now, as an **error** rather than a
note, because this is not "it will not play the way it reads", it is a floor that
can eat a run. It asks exactly one question and says out loud why the other two it
could have asked cannot fail: opening a wall only adds passable tiles, so the only
thing it takes away is support, and `route_reaches` already answers that with the
tile below — down the very shaft the hole just made. An arm nothing can reach is an
arm nobody has checked.

**The third is that the suite's floor became the rule.** It counted no sectors on
purpose — a ratchet on four would be a number people learn to move — and now that
the campaign is clean it asks the property instead: *every* sector carrying a patch
saves something by it, and the best of them saves five steps or more. "All of them"
is not a number somebody can move, it is what
[levels/LEGEND.md](levels/LEGEND.md) says a patch is.


SDL3 must be discoverable through `pkg-config`. The **test binary links no SDL**
(`TEST_CFLAGS` omits the SDL flags), so `make test` works even where SDL3 is
unavailable, and it runs in well under a second.

There is no test filter: `tests/test_main.c` is one binary whose `main()` calls
every `test_*` function in sequence. To run a single test, temporarily comment
out the others in `main()`, or just run the whole suite. Failures are reported
by the `CHECK` macro as `file:line: check failed: <expr>` and the process exits 1.

**All of it runs in CI** ([.github/workflows/ci.yml](.github/workflows/ci.yml)),
in four jobs split by what each needs: `make test` on a runner with no SDL
installed at all — if that job ever needs one, the SDL boundary below has been
crossed and the failure is the point — then the sanitizers, then the cross-build,
then a macOS job that builds the game, the editor, the debug target **and the
shipped bundle** on the platform the game actually ships on. That last one is
`make app`, and it is there because none of the others builds it: `make` links
Homebrew's SDL for this arch and this macOS, while the app is two slices against
the vendored universal framework with an older deployment floor. Left out, a
break in the fetch, the `lipo`, the framework's rpath or the `Info.plist` read
out of [version.h](src/version.h) was only ever found by whoever was cutting a
release. No signing identity is needed for it — `packaging/build_macos.sh` falls
back to an ad-hoc signature, and `MACOS_BUNDLE_ONLY=1` is what stops it before
Apple, which still proves the bundle assembles and verifies. `make mac` is the
whole thing and needs a real identity and Apple's servers, so it stays a local
step.

**And the shipped macOS archive could not be opened, because one Makefile
prerequisite made the documented order impossible.** The target that cut the zip
had `app` as its prerequisite, and the script `app` ran opened with
`rm -rf "$app"`. So cutting the archive *destroyed* the notarization ticket it
needs and then packed the result — and the closing advice, "notarize before
handing the zip to anybody", could not be followed in that order either:
notarizing staples the bundle, and the zip had already been cut from the
un-stapled one. What a player got was Gatekeeper refusing the app as software
"Apple cannot check", which reads as the game being broken rather than as a step
having been skipped.
Three things are worth keeping. `codesign --verify` **passes** on a
signed-and-never-submitted bundle, so the guard that was there could not see
this at all: notarization is a ticket and `xcrun stapler validate` is the
question. The prose was right the whole time —
[itch/README.md](itch/README.md) had spelled the working order out and warned
about exactly this since it was written — so this is
this file's oldest defect with the halves swapped: **the documentation was
correct and the tooling contradicted it**, which is worse than the usual way
round, because the person following the instructions is the one who gets the
broken artifact.
**And the fix was not a guard, it was deleting the seam.** The first attempt
added a staple check to the zip step and a target naming the order — which is
one more thing to know, on a list already long enough that nobody could find the
three commands that matter in it. macOS now has one script, `build_macos.sh`,
beside `build_linux.sh` and `build_windows.sh`, doing what those two do in the
same four steps: the library, the game, the payload, the archive. The ordering
mistake is unreachable rather than guarded against, and the check written to
catch it is gone because it has nothing left to ask. **When a bug is only
possible because a job was split in three, the fix is to stop splitting it** —
and the smell that says so is a family of names where the members do not do the
same kind of thing.

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

**And then the campaign's own rules were read the way the code has been, which
found the proxy this file has been describing for pages sitting inside a check
about the maps.** Three findings; the first is the one to keep.

- **A check measuring the map where the rule names the climb.**
  [levels/LEGEND.md](levels/LEGEND.md) says a climb's budget *and its height*
  must exceed the previous climb's. The test asserted `map.height` — the row
  count of the file. The two agree only while the spawn sits on the bottom row,
  and one of the five did not: sector 13 carried two blank rows under its `S`,
  so its map was 48 rows against sector 11's 46 and the assertion passed, while
  both walls were 45 tiles from spawn to window. **The player is shown that
  number**: the facade HUD read `WINDOW 45M` on the third climb and again on the
  fourth, so the campaign's one visible promise about the walls — each one
  taller than the last — stopped being true a third of the way up and every gate
  was green over it. Nothing was wrong with either map; what was wrong was that
  the quantity measured was not the quantity ruled on, which is this file's
  oldest defect wearing the campaign's clothes rather than the suite's. The `S`
  moved to the bottom row (39, 42, 45, **47**, 51) and the test computes the
  rise the way `draw_facade_hud` computes it, so the assertion and the readout
  cannot come apart again. Worth knowing how it hid: `map.height` is also what
  the "no two sectors are the same size" check three lines above uses, where it
  is exactly right — one field, two questions, correct for one of them.
- **A numerator and a denominator asked different questions.**
  `run_tally_format_record` counted sector times across
  `PROGRESS_MAX_TRACKED_SECTORS` and printed them against `CAMPAIGN_SECTORS`.
  The slack between the two is deliberate — it is what lets a longer campaign
  ship without a new file format, and `progress_parse` accepts every index
  inside it — so a file from such a build, or from anybody with a text editor,
  made the RECORDS page print `20 / 17`, a fraction over its own whole. The
  four-thousand-file fuzz over `progress_parse` could not see it: it asks
  whether a parsed value survives a round trip, and this value did. **A fuzz on
  the parser is not a check on what the parser's output is then used for.**
- **One file holding one rule and applying it twice out of three times.**
  [run_tally.c](src/run_tally.c) exists so a record reads the same wherever it
  is read. Its docket line suppresses itself when there is no record to quote,
  and `run_tally_format_record` spells that same state `--`, both under a
  written-out argument that a high-water mark of nought means "no run has
  finished". The score line fell through to its comparison clause and told the
  first player to die before scoring `SCORE 0 - BEST 0`, quoting a record that
  does not exist at them, on their first sight of the scoreboard. It is
  `SCORE 0` now. The test asks the property rather than the strings — no line
  quotes a record until one exists, every line quotes it once there is one — and
  reads the card and the page off the same `Progress` in the same state, which
  is the shape that would have caught it.

And one thing that was neither a defect nor a gap in the rules, but is the
reason the branch beside it is worth having. Four arms of
`gameplay_combat_handle_player_action` answer a loaded weapon with nowhere to
put what it fires, and each is the same three lines. Two were covered — the
launcher because `MAX_ROCKETS` is one, the bolt because its limit is a clock —
and the grenade's, the charge's and the sidearm's had never been executed by
anything, because no test had ever had eight grenades, four charges or eight
rounds in the air at once. What that costs is not damage: the trigger does
nothing and **says** nothing, which reads as the pad having missed the press,
and the answer to that is to press it again, in a fight.
`test_a_full_sky_is_still_a_dead_press` fills the sky rather than staging the
slots, because "the slots are full" is the thing under test and a hand-set
`active` flag passes whatever the search does. The sidearm's arm needs the whole
scenario its own comment describes — `MAX_AMMO` is six against `MAX_BULLETS`
eight, so a full clip cannot fill the air by itself and the magazines are picked
up off the floor the way they are in the game.

All six changes were **checked by breaking the thing they guard and watching the
new tests fail** — the map restored to its old spawn, the record count back on
the array, the score clause disabled, and each of the three dry presses
silenced in turn.

**And then the suite's own invariants were run wider than the suite runs them,
which is where the one shipped bug on this page since the veteran lives was
sitting.** `test_the_whole_frame_survives_a_monkey_on_the_controls` asserts, among
other things, that nothing alive is inside masonry. It drives one seed for six
seconds per sector. Driven twenty-four seeds for ninety — about 37 million
simulation steps, the same invariants, nothing else changed — it fails, on one
sector, and it fails with the pad untouched: **a guard stands inside sector 6's
ceiling, three of them at once, on a floor where the player is doing nothing at
all.** Everything else in the campaign came back clean, which is the useful half
of the answer.

The cause is one term. `level_update_elevators` clamps a lift to
`top_limit = first * TILE_SIZE`, where `first` is the highest `V` in the run — so
the deck parks flush with the *top* of that tile and a rider, who is exactly
`TILE_SIZE` tall, ends up standing wholly in the tile *beyond* the shaft. The
campaign has three shafts. On two of them the tile above is air, so the only
symptom was a lift climbing out of its own run and nobody had reason to look. On
sector 6 it is `#`.

- **The guard is not crushed, and that is the part you can see.**
  `enemy_begin_elevator_ride` snaps him to the deck with no ceiling test at all,
  and figures are drawn after the tile layer, so he is drawn *over* the ceiling:
  0.89s of every 10.5s lift cycle overlapping the slab, 0.03s of it entirely
  inside.
- **The player is killed.** `gameplay_resolve_player_crush` finds the slab
  overhead, a one-tile shaft offers nothing either side to be squeezed into, and
  an elevator crush is an outright death rather than a heart. LEGEND.md calls
  that shaft "the way up" for the sector; riding it a second past the storey
  ended the run.

**The route model had been right the whole time and nothing was reading it.**
`route_in_shaft` only ever answers for tiles that *are* shaft, so every map in the
campaign was certified on exactly the rule the simulation was breaking: a lift
carries its rider to the top of its own run and no further. The fix is to make the
simulation agree — the topmost shaft tile is the rider's headroom, not the
platform's parking space — which also means no ceiling test is needed anywhere
downstream, because every tile a deck can now reach is a `V` and a `V` is
passable.

Three things are worth keeping, and the second is the one that cost the most time.

- **Both elevator tests used fixture maps with air above the shaft.** So the
  over-travel they were asserting had nothing to hit, and one of them pinned it
  as a literal: `player.y < 2.0f * TILE_SIZE` on a map where that means "got
  above the shaft". It asks the lift for `top_limit` now. The guard's fixture was
  worse — its top `V` sat in a *slab* row with masonry either side, and the guard
  still walked off at the top, so the map asserted a destination `route_in_shaft`
  has never sanctioned. Its shaft is a row taller now. **A fixture that differs
  from every shipped map in exactly the respect under test is not a test of the
  shipped maps.**
- **`top_limit` alone does not fix it, and the first attempt shipped that.** With
  the headroom in and nothing else changed, the player still died.
  `gameplay_carry_player_on_elevator` runs a pass *before* `level_update_elevators`
  — deliberately, so the crush check sees where the rider is going, which is what
  squeezes an off-centre boarder clear instead of killing him — and it integrated
  the lift's velocity **without the clamp the lift itself gets a few lines later.**
  One fraction of a pixel past the stop is a different row to a check that reads
  the row of his top edge. The overshoot lasted one pass of one frame,
  `gameplay_ride_platforms` corrected the position at the end of the same frame,
  and nothing but the crush check ever saw it. Two halves, one symptom; the tests
  require each half separately and were checked by reverting each on its own.
- **The gates that were green over it.** The soak sweep drew sector 6 every run.
  `make coverage` counted every line of the lift as executed. Both elevator tests
  passed. `make coverage-shell` said 41. The editor's validator had nothing to say,
  and has no rule about a shaft's head at all — it counts shafts against
  `MAX_ELEVATORS` and stops. What found it was turning an invariant the suite
  already owned up from six seconds to ninety.

Four more came out of the same read, and none of them was a shipped bug.

- **A twin with one test between them, on the prop the player pushes.**
  `gameplay_resolve_player_crates` answers a crate four ways and the suite drove
  two: measured, `direction = -1` and the whole underside arm had never been
  executed by anything, because every crate test in the tree — its own bots
  included — walks `right`. Both work. What they were is half a mechanic on a prop
  that is on eleven floors. The underside needs a crate with air beneath it and
  that is a real configuration rather than a staged one: support is
  box-against-tiles and `CRATE_W` is 28 against a 32 tile, so a crate shoved to
  the lip of a ledge stays up on one pixel of tile and hangs the rest over the
  drop. The test puts it there and then **asks the simulation whether it stays**
  before jumping into it, and it measures the same jump with the crate removed,
  because otherwise every assertion is satisfied by a jump that merely ran out of
  height at the right moment.
- **`RECORD_LABELS` was a table the game does not draw.** It is in
  [run_tally.c](src/run_tally.c), guarded by a `_Static_assert`, asserted on by
  the suite — *"a label over it, because a figure with no name on it is a number
  on a plate"* — and `run_tally_record_label` had **no caller outside the tests**.
  The names the player reads were a second copy spelled out in `RECORD_ROWS` in
  [settings.c](src/settings.c). So the file whose own header says it "already owns
  what a record reads as" owned a table nothing drew, the check sat on the side
  nobody could see, and the copy on the plate was held by nothing.
- **Which is how the RECORDS page came to quote the wrong unit at the player.**
  The third row read `FURTHEST FLOOR` over a value that formats as `SECTOR 09`,
  under a detail line saying "THE HIGHEST SECTOR ANY RUN HAS REACHED" — one word
  out of three disagreeing, on the one screen whose whole subject is what the game
  remembers. It is not a synonym here: `BUILDING_FLOORS` is forty and
  `CAMPAIGN_SECTORS` is seventeen, and this file keeps those two deliberately
  underived from each other for precisely this reason. The words have one home
  now and reach the renderer from it through `settings_row_label`, so a fifth
  figure cannot arrive with two names. **Pointer identity is not the check that
  holds it** — two identical literals in two translation units are pooled to one
  address, so re-adding the string compares equal and sails through; verified by
  doing it. What catches a second copy is that a readout must carry no label of
  its own at all.
- **And the spawn moved for the height rule landed off the window grid.** A
  facade's painted windows sit on rows of three and columns of four; the rise the
  `S` was moved to was 47, the one figure in `39, 42, 45, 47, 51` not divisible by
  three. Sector 13 spent a release as the only shipped map with a finding against
  it — the single note in all seventeen — and the only thing in the tree that knew
  was a tool nobody runs over maps they are not editing. Two rules pull on that
  tile: beat 45, stay under 51, be a multiple of three. That leaves exactly 48.

**So the notes bar moved, and it is a decision rather than a tidy-up.**
`test_the_editor_has_nothing_to_say_about_the_shipped_campaign` allowed notes
under a comment arguing that a note is guidance and not a defect — which is still
exactly right *about the editor*, where a note must not refuse an author's build,
and was doing double duty as a reason for the shipped campaign to carry them. The
seventeen sectors are held to nought of every severity now. This is the turn the
weak wall's rule already took: it counted no sectors on purpose, because a ratchet
on a number is a number people learn to move, and once the campaign was clean it
asked the property instead. The four restrooms are held to **one** note each,
because each carries the same one — "No THEME line, so the map loads as RESTROOM"
— and these are the maps that default exists for; held as a count rather than by
matching the sentence, so a second note is a failure while the known one is not.

All of it was **checked by breaking the thing it guards and watching the new tests
fail** — nine mutations: each half of the lift fix on its own, the crate's left
push and its underside, the record label's unit and its second copy, sector 13's
spawn back off the grid, and both halves of the new lint below. The wide sweep was
then re-run and comes back at nought.

**And a release could not be built at all, on the one platform a Mac cannot build
for — for the same reason as the macOS archive two sections down.**
`payloads.yml`'s smoke test opened with `ls -d dist/stage/Chuck-*-linux-x86_64`,
and the last thing `build_linux.sh` does before printing "upload this" is
`rm -rf "$dist/stage"` — deliberately, with a comment saying why: left standing,
the staging tree makes `dist/` hold the payload twice, which is how somebody
uploads the folder instead of the archive. So the step died on `ls: cannot access`
every single time it ran. **It is the macOS story with the object swapped** — a
step consuming an intermediate the step before it is documented as destroying —
and it went unseen because that workflow is `workflow_dispatch` and starts on
nothing.

The fix is not a guard and not a reordering: the step unpacks the **tarball**. The
staging tree was never what a player downloads, and extracting the archive proves
more than reading the tree did — that tar kept the executable bit and the `lib/`
travelling beside the binary, which is the whole point of soaking the payload
rather than a fresh build. It also cannot be broken again by the script tidying up
after itself, because what it consumes is the shipped file. And the class now has
a check: `check_lists.py` derives the doomed paths from the `rm -rf "$dist/..."`
lines in `packaging/build_*.sh` and fails if any workflow reaches into one. Worth
knowing how the first draft of that check was wrong — it read each script whole,
and these scripts explain every `rm -rf` in prose directly above it, so
commenting the real line out left the check still reporting a tree it was no
longer told about. **A lint that cannot find what it checks is worse than no
lint**, which is why it has an empty-set guard and why that guard is the thing
the second mutation tested.

## Architecture

### The SDL boundary

Two layers, and the split is the most important invariant in the codebase:

- **Application shell** (SDL-dependent): [main.c](src/main.c) (SDL callbacks),
  [game.c](src/game.c) (state machine, level loading, per-frame orchestration),
  [game_input.c](src/game_input.c), [game_render.c](src/game_render.c),
  [render_figures.c](src/render_figures.c),
  [render_sprite.c](src/render_sprite.c),
  [chase_render.c](src/chase_render.c), [level_art.c](src/level_art.c),
  [audio.c](src/audio.c), [screenshot.c](src/screenshot.c),
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
| [Tooling](docs/tooling.md) | The level editor, the press kit that photographs the game, the shipped signed notarized macOS app, and the three payloads a release is |

[levels/LEGEND.md](levels/LEGEND.md) is the other reference page and did not
move: it is every map character, the authoring rules the geometry has to
respect, and a table of all seventeen plans.

[itch/](itch/) is the third and the newest, and it is not about the code: the store
page's own operating manual, the description, and the install instructions —
`itch/README.md` says what goes in which field.

## Conventions

- C17, built with `-Wall -Wextra -Wpedantic`; the tree is warning-free, keep it
  that way. `make sanitize` should stay clean too.
  **`make win` is the cheapest second opinion available to this tree**:
  mingw-w64 is neither the developer's compiler nor the developer's platform, and
  the first time it ran it found a missing `<stdio.h>`, two dead loop counters, a
  `%zu` checked against the wrong printf and two format buffers whose worst case
  does not fit.
  **And then it stayed a command somebody had to remember, which is the state
  this whole page argues against.** For a release it was in no workflow at all —
  `ci.yml` had three jobs and none of them cross-compiled, and `payloads.yml` is
  `workflow_dispatch` and builds Linux — so "the tree is warning-free" went back
  to meaning *under one compiler on one platform*, with the paragraph above
  sitting in the repository claiming otherwise. It is the `crosswin` job now, and
  it is the cheapest job in CI: mingw-w64 is an apt package, the SDL it links is
  libsdl.org's own mingw release at the pin, nothing it produces is run, and it
  uploads the Windows payload as an artifact because a payload built on every
  push is the one thing `payloads.yml` cannot hand anybody on demand.
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
  New behaviour in a gameplay module should get a test in that style.
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
  **And "every word" was one axis short on the sheet that has the most of them.**
  Everything above is about width. `test_every_word_on_the_options_sheet_fits_the_plate`
  walks every label, detail, strap, title and footer on the options sheet and asks
  one question of each: is this line narrower than the column. The plate has a
  *height* too, it is computed from the table rather than fixed, and by the time
  anybody looked the sheet was running about **50px off the bottom of the frame**:
  the RECORDS strap printed through the footer prompt, and `RESET RECORDS` and its
  detail line drawn below the plate and off the screen. On every machine, because
  the renderer is given a fixed logical presentation — and in
  `dist/press/15-options.png`, which is the still the store page is cut from.
  Three gates were green over it. The soak drew the frame, so `make coverage`
  counted it. The width check passed, because nothing on it was too wide. And the
  fit check's own model of a pad cap is `widest_pad_spelling`, the *expanded*
  form, while the sheet was drawing the raw `$A` — the other bug in the same
  paragraph, below.
  Two things are worth taking from it. The first is the arithmetic, because it is
  a shape rather than a slip: the renderer squeezed the rows that carry a label
  and a sentence, which is right, and then recomputed the budget as
  `rows_h *= squeeze` — as though the *headings* had shrunk with them. They
  cannot; a heading's height is its rule plus its sentence. **A total that was
  built from two kinds of thing cannot be scaled as though it were one.** The
  second is where the fix had to live: the whole layout is `settings_page_layout`
  in [settings.c](src/settings.c) now, on the SDL-free side, and
  `test_every_page_of_the_options_sheet_fits_the_frame_it_is_drawn_in` re-adds the
  rows the way the renderer's own loop adds them rather than reading the field
  back — because `rows_h` agreeing with the drawing loop is precisely what had
  stopped being true, and a check that reads the field is a check that agrees with
  the bug. `SETTINGS_SQUEEZE_MIN` is derived from a row's own ink now instead of
  being a literal `0.6` with a comment claiming "past this the labels touch",
  which was a measurement nobody had taken; it derives to exactly 0.6, which is
  the pleasant half of that story. RECORDS is a page of its own, which is what
  puts the main page back inside its budget with air to spare. And the selection
  highlight was a fixed 36px chosen against an unsqueezed 40px row, so on a
  squeezed page — and on the controls page, whose rows are 28 — the wash reached
  over the label under it and claimed a row the cursor was not on.
  **`test_every_word_on_the_options_sheet_fits_the_plate` also walked a
  two-entry array of pages with a literal `p < 2` beside it**, so a third page
  would have been measured by nothing at all. It walks `SETTINGS_PAGE_COUNT`
  now. Same for the cursor walk next door: `test_settings_cursor_only_lands_on_rows`
  asserted "two laps" over `row_count * 2` steps, which is two laps only while a
  page is mostly rows — the records page is one row under one heading, and the same
  assertion on the same walk saw four. A lap is counted in reachable rows now.

  **And the caps on the controls page were drawn in the opposite order to the
  one the caret walks them in.** `settings_bind_slot` is one number, left and
  right step it, and slots 0 and 1 are the row's *keys* — the heading over the
  page says `TWO KEYS, THEN TWO PAD BUTTONS` and the manual's own `CONTROLS`
  sheet draws the keyboard first. The renderer pinned the keyboard's pair to the
  right margin and put the pad's to its left, under a comment claiming that made
  a row read keys-then-buttons "in the order the caret walks them". So the page
  opened with the caret on the **third** cap from the left, LEFT was refused at
  slot 0 with two caps sitting beside the label, and the two the caret reached
  last were the two drawn first — on the one sheet whose entire job is telling a
  player which control is which.
  Three things are worth keeping. The strap, the comment and the manual all
  described the order correctly and the drawing did not, which is the *rationale
  reporting agreement it does not have* defect this file already has a name for,
  one screen over from where it was last found. **Nothing in the tree could see
  it**: the fit check measures the run's width and the order does not change it,
  and the soak sweep drew the frame every single run — a counter cannot tell a
  frame that was drawn from a frame anybody could read, which is the lesson
  `--shot` was added for and the first time it has paid since. And the check that
  should have owned the question was laying the run out a *second* time,
  "exactly as `draw_setting_keys` lays it out", so both copies agreed about the
  width while the renderer put the halves in the wrong places. It is
  `settings_bind_caps` in [settings.c](src/settings.c) now, beside
  `settings_page_layout`, which had moved there for the same reason one release
  earlier; `test_the_binding_caps_run_left_to_right` asks the property rather
  than the numbers, so a fifth slot or a regrouping is checked by having been
  written.

  **And `fits` cannot say how close a page is to the edge.** It is a cliff, it
  has been true for every page since the records split, and the honest answer
  turned out to be that the main page is *at* it: one spare value row, and
  **none** with the mute warning up, drawn at a squeeze of 0.68 against a floor
  of 0.6 — about 4px of air between a detail line and the label under it where
  the design allows 17. Nothing is broken and the gate does fire on the row that
  would break it, in the muted state, one release before a player sees anything.
  What a boolean cannot do is tell the next person adding a row that they are
  adding it to a full page, and a flag that has been green for a long time reads
  as room. So the layout answers `spare_rows` too, the figures are written
  into [settings.h](src/settings.h), and the fit test checks the number *against
  the function* rather than restating it — a page with `spare_rows` in hand must
  still fit a frame that much shorter and must not fit one shorter still, since
  one more row costs a squeezed row rather than a whole one.

  **And the split it asked for has been made, which took a release because the
  paragraph above closed by saying a check could not make it.** That was true and
  it is also how a measured number sat at one for a release: the tool reported the
  page was full, the sentence explained that somebody had to decide *which* rows
  left, and nobody was the somebody. DIFFICULTY is the fourth page — ASSIST and
  CHALLENGE together, because they are one question asked in both directions and
  `gameplay_enemy_speed_scale` reads both switches and resolves them against each
  other — and the main page keeps the two levels and the three display switches and
  carries a row down to each of the other three, under a `MORE` heading of its own.
  Which fixed a grouping nobody had chosen, either: `CONTROLS` used to hang off the
  end of DISPLAY and `RECORDS` off the end of CHALLENGE, two "this opens another
  sheet" rows inside two sections about something else, arrived at because there
  was no room for a fourth heading. Measured after it: **six** spare value rows and
  no squeeze at all in either state, against one and 0.68.

  **And the RECORDS page did not show any records.** It was split off the main page
  to relieve exactly the height problem above, and what went over there was a
  strap listing the three things the game keeps and one row offering to delete all
  of them. The numbers were readable on the field manual's `THE RECORD` sheet and
  nowhere else, so the one screen in the game whose whole subject is the records
  was the one screen that would not print one — and the only destructive row in the
  options sheet was asking "are you sure" about figures the player could not see.
  It carries four readouts now, formatted by `run_tally_format_record` beside the
  manual's own cell, because one file answering "what does a record read as"
  wherever it is read is the same rule the rest of this list is built on.
  Two things about it are the general ones. It is a **row kind** rather than a
  block drawn under the table, because everything that makes this sheet safe is
  per-row — the plate's height is summed from the rows, the fit check walks the
  rows, the caret walks the rows — so a line outside the table would be a line
  none of the three had an opinion about, which is precisely how this sheet came to
  be drawn 50px off the bottom of the frame the last time something was added to
  it. And its *value* is the one thing on the plate the table does not spell, so
  the fit check drives every ceiling `Progress` will store through the formatter
  and measures the widest answer rather than reading a bound somebody wrote down.
  The cursor's own rule moved to `settings_row_is_reachable` at the same time,
  because "which kinds can the caret stop on" was a `kind != SETTING_ROW_HEADING`
  in three places — two walks and a test — and a second unreachable kind would
  have had to be remembered in all three.

  **And three tuning numbers were spelled out in words on sheets that hold every
  other number they print.** The campaign's own figures are all derived by now —
  the sectors, the climbs, the crew, the floors, the docket. The *mechanics'*
  were not: `A GUARD sees a cone seven tiles ahead` and the `SEVEN TILES` label
  on the picture beside it against `ENEMY_VIEW_RANGE`, `Every three tiles of
  climb is banked` against `FACADE_CHECKPOINT_STEP`, and the pause sheet's `THE
  TEN SHEETS` against `MANUAL_PAGE_COUNT`. The first of those is the number the
  entire quiet route is played against, written out twice and derived neither
  time; the last is the one `check_docs.py` already derives for the store page,
  the README and `docs/screens.md`, so the only copy nobody held was **the one a
  player reads** — this file's oldest defect with the audience swapped. All four
  were correct the day the check was written, and that is the point: what was
  missing was a guard, not a fix. `test_the_sheets_spell_the_tuning_they_quote`
  finds each claim by its own shape rather than by index and requires the claim
  to have been found at all, so rewording a sentence fails loudly instead of
  quietly being checked no longer. The illustration's label moved to
  [manual_pages.h](src/manual_pages.h) to get there, and the split is the rule
  rather than an inconsistency: `FROM ABOVE` and `LAND ON HIS HEAD` stay in the
  renderer because they are captions on a picture, while this one is a claim
  about the simulation.

  **And the sheet was printing `$A` at the player.** Same screen, same release,
  and it is the *shell* half of the same story. `keybind_pad_name` hands back a
  `pad_hint` template — `$A`, `$LB` — because what a face button is called
  depends on the pad, and the controls page expanded it with
  `pad_hint(game_pad_hints(game), buf, size, form, form)`. `game_pad_hints`
  answers NULL with nothing plugged in, `pad_hint` hands a NULL set its *key*
  form through verbatim, and the only key form a `$A` template has is itself. So
  every player without a controller — which is how this game is mostly played —
  read `$A`, `$B`, `$X`, `$Y`, `$LB` and `$RB` as literal text in the pad
  column, on the one screen whose whole job is reporting state. The d-pad rows
  were right throughout, their names carrying no token, so the sheet looked
  half-finished rather than broken.
  Three things. The manual's own control sheet had been spelling those letters
  correctly the whole time, from the same tables — **two screens, the same
  question, two answers**, which is this file's most reliable smell. `pad_hint`
  itself has a test and is not at fault; what was wrong was a *call site* on the
  far side of the SDL boundary, and the guard that reaches it is
  `test_every_pad_cap_spells_a_button_with_no_pad_in_hand`, which asks the one
  thing that catches it from either end — **no `$` survives the expansion**. And
  the distinction the fix turns on is worth keeping: a *prompt* asks what is in
  the player's hands and should say ENTER when that is nothing, while a *binding
  row* reports what the slot holds, and the slot holds it with the pad in a
  drawer. `game_pad_spelling` is the second question, and it is never NULL.

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
  **And [itch/](itch/) was the last directory it learned to read, which is the one
  worth being embarrassed about.** Every check was anchored to `docs/`,
  `README.md`, this file, the `Makefile` or a comment under `src/` — so the page
  written for people who are *not* reading the repository, the store copy a
  stranger reads before playing a second of the game, was the page held to
  nothing. It states the campaign's length, the climb count, the hearts, the crew,
  two readings off the night's clock and the length of the night: very nearly the
  complete list of what this script is for. All of them are derived now.
  And the two copies of that copy are held to each other by
  [tools/check_lists.py](tools/check_lists.py). `itch/page.html` is `itch/page.md`
  again in a format the store's own editor will swallow — it takes no markdown and
  offers no source view — with "change one, change the other" written above them,
  which is an instruction to a reader rather than a check. The silent direction is
  the bad one: the derived checks hold the *markdown*, so editing that alone left
  every gate green and the shop showing the old text.
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

**And the newest item in the game turned out to be half-wired into four places
at once, which is one shape rather than four findings.** The flash charge `!`
was added after the grenade and the rocket, and every place those two are named
as a pair it was left off the end of the pair. It is the item
[docs/gameplay.md](docs/gameplay.md) calls "the answer to *after*" and the store
page sells as a headline bullet, laid out on the six sectors a floor can go
wrong on, one to a floor, no respawn.

- **A death destroyed it — the only shipped bug in this pass.** The shell kept
  its own copy of the loadout rule, `transfer_player_loadout` in
  [game.c](src/game.c), listing the clip, the grenade, the rocket, the hand and
  the facing. `finish_player_death` calls it across a `player_reset`, so what is
  not on that list is gone. The comment directly above the call said "a death
  costs the walk back, never the kit: the grenade and the rocket saved for the
  hard part survive it", and [docs/gameplay.md](docs/gameplay.md) said the same
  two of three in the same words. So the one item in the game whose entire
  subject is *a floor having already gone wrong* was the one item the game took
  away when a floor went wrong. Proved through the real function rather than by
  reading it: `before death: grenades=1 rockets=1 flash=1` /
  `after death: grenades=1 rockets=1 flash=0`.
- **And the restroom door took it for the length of the visit**, same function,
  same missing line, second call site. Not a permanent loss — the sector's own
  player is frozen outside — which is exactly why nothing would ever have
  noticed: he has it again when he comes out. He does not have it in the room,
  and the four rooms have men in them and one of them has a dog.
- **The sector HUD did not draw it at all.** `render_hud` drew the grenade and
  the rocket, on the twelve floors where the charge is the only one of the three
  that can actually be thrown. That is not a cosmetic omission, because
  [docs/gameplay.md](docs/gameplay.md) leans on that row *by name* to justify
  "a pickup never arms itself" — the rule is only fair if the player can see
  they are carrying the thing that did not arm itself.
- **And the facade HUD drew it on top of two of the six cartridges.** Placed at
  706 in a pip run that ends at 717, so a climber carrying a charge read a
  four-round clip off the readout whose own comment promises "every pip is
  always lit". Two strips, the same question, two answers, and **neither of them
  the right one** — the usual smell pointing at both of its ends at once.
- **And the ladder throw pose put a grenade in his hand for all three.** One
  pose and one `grenade_throwing` flag is deliberate and is right about the
  *animation*; it was quietly doing duty for the *prop*, with
  `draw_flashbang`'s own comment insisting the charge "has to be told from a
  grenade at a glance, because one of the two is about to kill whoever is
  standing next to it".
- **And the manual's one list of what to pick up** named the card, the ammo, the
  medkit, the grenade and the rocket — **twice**, once in the words and once in
  the illustration beside them, which is the half that is easiest to miss
  because no fit check can read a picture. The picture's copy was a `[5]` of
  names beside a `for (i < 5)` and a `switch` on the index: the exact shape
  [manual.c](src/manual.c) already carries a paragraph about, three hundred
  lines further up, under "an array whose length is a claim must be written
  `[]`". It is a pair table of name-and-drawing now, unsized, with the alert
  block below it placed off its length. The sheet before this one teaches what a
  flash *does*; no sheet said it was a thing to pick up.

Four things are worth keeping, and the second is the one this page has not
written down before.

**The fix is not five fixes, it is deleting the seam** — the same move the macOS
release story further up this file ends on. The loadout rule existed twice
because half of it was in the shell, and the half in the shell was the half no
test could reach: `carry_throwables` in [player.c](src/player.c) is now what
both rules agree through, `player_carry_loadout` is beside
`player_begin_sector` where its other half lives, and `game.c` has no opinion
about a loadout any more. Likewise the HUD row is one `draw_hud_carried` used by
both strips instead of two hand-placed sets of coordinates.

**A check that asks a list of fields would have gone stale the way the thing it
checks went stale.** `test_every_doorway_hands_over_the_whole_pack` walks
`PlayerWeapon` and asks `player_weapon_available` — every weapon that was in the
pack before a doorway is in it after — so a fourth thing to carry is checked by
having been added to the enum, and there is no list anywhere for anybody to
forget to extend. It also pins what the two rules deliberately *disagree* about,
so a change that tidies them into one fails here rather than in somebody's run.

**A renderer's own literals are a layout nothing can measure**, which is the
`settings.c` lesson arriving one screen over. The row's geometry is in
[game_config.h](src/game_config.h) now — slots, ink widths, where each strip's
clip ends and where its block does — and the check is geometry rather than a
list of positions: no glyph reaches into the one beside it, the row clears the
cartridges to its left, it stops before the rule that closes the block. Every
width is measured from `x - 1`, because that is where the outlines open and one
origin is the only way the readings can be compared at all. The sector block is
sixteen pixels wider for the third slot and the pixels came out of the air the
SCORE readout was sitting on, so ACCESS, SECTOR and SCORE all moved and the
TRAIL meter did not.

**And `--shot` paid for itself a second time.** Every gate was green over both
HUD defects: the soak sweep drew both strips every single run, `make coverage`
counted `draw_flashbang` as executed, and nothing headless has ever *carried* a
charge, so the frame that shows the overlap had never been drawn. What found it
was arithmetic on the two coordinates, and what proved it was a staged capture —
a counter cannot tell a frame that was drawn from a frame anybody could read.

All eight mutations were **checked by breaking the thing they guard and watching
the new tests fail**: each of the three throwables dropped from
`carry_throwables`, a respawn made to open on the pistol the way a sector does,
the facade row put back on top of its clip, the sector block put back to its old
width, a slot made narrower than the glyph in it, and each of the two new throw
branches made to lie about what left the hand.


**And then the whole tree was read again from the outside, with every gate green,
and what it turned up was mostly the same two shapes this page keeps naming: a
check measuring the wrong quantity, and a fix that landed on one half of a
symmetric defect.** Every gate *was* green and stayed green throughout — `make
test`, `make lint`, `make sanitize`, `make coverage` at `none`, `make
coverage-shell` at 41 with no drift, `make win` at nought warnings, clang's
static analyzer at nought findings on the whole SDL-free tree, and 117 million
steps of the monkey widened to 120 seeds and 240 seconds a sector. Nothing in
this pass was a crash and nothing cost a player a run. What follows is what
survives that.

- **`SPAWNS` was in neither seating sum, and it is the deterministic half of the
  mechanic.** `test_every_sector_can_seat_the_reinforcements_it_can_call` and
  `check_caps` in [editor_validate.c](editor/editor_validate.c) both counted
  `terminals × TERMINAL_REINFORCEMENT_MAX_COUNT` and stopped. The `SPAWNS` line
  is the *other* way men come out of a door — `game.c` copies it into
  `door_spawns` at level start and `gameplay_ai_update_spawns` drips it out on a
  timer with no alarm and no console involved — and it lands in the same
  `spawn_enemy_from_door`, taking the same slots and rolling the same
  `DOG_DOOR_HANDLER_CHANCE`. Driven rather than added up, sector 14 reaches **21
  of 24** and the check said 18: the busiest floor below the roof had three slots
  of headroom while the assertion described six, and the ratchet at the bottom of
  that test — "if a rewrite leaves the worst floor asking for a third of the
  array, the number wants revisiting" — was being evaluated on the understated
  figure. The editor was worse in kind: its warning was gated on `called > 0`, a
  console count, so a map whose entire over-subscription came out of `SPAWNS`
  produced **the same report as `SPAWNS 0 0`**, verified at `SPAWNS 9999 9999`
  with no console on the map and nothing in the loader bounding the value.
  Three things are worth keeping. The dogs had no arrival line at all — the
  authored `W` was capped and what a floor could *send for* was not, which is the
  cap list's own missing-line story one row further down the same function. The
  reason nobody had looked is that the single line joining the map's `SPAWNS` to
  the runtime counter is in `game.c`, on the shell side, so
  `stage_sector_at_its_spawn` never sets it and **no test in this file had ever
  seen a shipped floor's drip** — the `level_update_moving_platforms` lesson with
  the object swapped. And the arithmetic fix is checked by a *simulation*:
  `test_the_reinforcement_drip_fills_the_floor_it_is_wired_to` starts the drip
  the way the shell starts it and requires the cheap sum to be an upper bound on
  what the floor actually fills to, because an arithmetic fix verified by more
  arithmetic is a check agreeing with itself. Its first draft read 14 on sector
  14 and would have passed a `MAX_ENEMIES` of 16: without
  `gameplay_ai_update_movement` in the loop the arrivals stand in the doorway
  they came out of and the floor stops filling at two. **A harness that jams the
  door measures the jam.**
- **The mine had no authoring rule of any kind, and it is the most expensive tile
  in the game.** Two hearts of three — an ordinary blast, the same one that opens
  a `%` — against five checks for a fan that costs one and one check for a spike
  bed that costs one. It is invisible to the route model as well, which knows a
  spike (`RouteMap.spike`, and a hop rule for one) and has never heard of a mine,
  so a certified route runs straight over all 46 of them. `check_mines` asks the
  four questions a fan is already asked, and none of them forbids a mine
  anywhere: what it warns about is a charge whose own answers something *else* on
  the plan has removed. Two shipped tiles tripped it — sector 12's mine directly
  under a fan, where ducking the blades lands on the charge and hopping the
  charge lands in the blades, and sector 17's mine hard against the ladder out of
  the spawn. The fan moved and the mine moved; the counts did not, because
  `level_hazard_budget` is a documented sequence and this was a placement
  problem.
  **The radius is one column and not the fan's two, and copying that number was
  the first draft's mistake.** Blades *reach* — `CEILING_FAN_BLADE_LENGTH` hangs
  better than a tile either side of the fan's column — so a fan two columns off a
  ladder genuinely overhangs the arrival and the span measures something. A mine
  is exactly its tile. Measured, ±2 flags six tiles across four of the seven
  mined floors and ±1 flags the two where the step off the rungs really has
  nowhere to land; the wider rule would have overridden authored intent on half
  the mined campaign on the strength of a constant borrowed from a rule whose
  justification does not come with it. The first draft also measured *across
  masonry* and reported sector 8's rocket pocket, which is one column from a
  ladder with the pocket's own wall in between — a step-off cannot happen through
  a wall, so the scan walks outward and stops at one.
- **Ten of the sixteen story beats were written and read by nobody**, which is
  this page's own "fixing one half of a symmetric defect" with the halves one
  file apart. `sector_tally.c` exists because the report is suppressed after a
  window and the *numbers* went quiet with it; the same screen carries a line of
  `TRANSITION_INTEL`, and that half was left where it was. `MONITOR WALL: VOSS.
  HER HAND ON THE SEVENTH LOCK.` and `TWELVE PLACES LAID IN THE GALLEY. TWELVE
  MEN.` among them. Every gate was green because **every check on that table asks
  whether a line fits its column and none asked whether anything puts it on the
  glass** — and `intel.c`'s own note said, in as many words, "put a beat the
  ending depends on in an unmarked row and nobody will ever see it", which reads
  as advice about emphasis and was a description of a hole. The tally carries the
  sentence now, in the report's grey over the amber, and the new assertion is
  about *reach* rather than width: every row either shows a report or rides the
  tally.
  Two things came out of doing it. The band has to grow **away** from whatever it
  sits next to, and the two placements have opposite neighbours — the reveal is
  on the bottom edge of the frame with the map rising behind it, the cleared card
  is directly under `draw_overlay_panel`'s verdict. Grown upward on both, as the
  first version did, the card printed the sentence straight through `SHE IS
  TWENTY FEET AWAY`: no test could see it, no *run* reaches it either, and what
  found it was `--shot`, which is the third time that switch has paid for itself
  and the same lesson each time. And `--screen reveal` is in the sweep now,
  because `cleared` was the only staged screen that put a tally on the glass and
  it is the other placement of the same band: **a placement is not covered
  because its twin is.**
- **Adding that screen name exposed a third copy of the screen list.**
  `check_lists.py` held `game_soak_screen` against `tools/soak.sh` and never
  looked at the sentence `--screen` prints when it turns a name down. So for the
  length of one edit the game accepted `reveal` and its own refusal listed the
  fourteen names without it — a message about which names exist, wrong about
  which names exist, on the one switch a caller reads it from. Held now, in both
  directions. **A list written down twice is usually written down three times**,
  and the third copy is the one in an error string, because nobody greps for
  those.
- **A dead branch, and the twin of a covered one.** `level.c` normalised a
  moving platform's `vx` sign under a comment saying "ensure vx has correct
  sign", and the `else` was unreachable on any map that can be authored: `P` is
  the only thing that makes a platform and it sets the speed positive where the
  character is read. Same shape as the `TILE_FALL_PLATFORM` that once sat in
  `level.h` naming a tile nothing parsed to. And `find_dog_slot`'s recycling arm
  was **compiled and never run** while `find_enemy_slot`'s had a test with a
  comment explaining why it mattered — the animal half of a rule written out
  twice, on the mechanic the whole quiet route rests on, with `W` on ten of the
  seventeen sectors.
- **An impossible pair of numbers, written down twice, and the fixture taught the
  staging.** `--screen report` and `--screen cleared` staged a run of 01:14 under
  `BEST 01:31` with `best_is_new` false, which says at once that the record was
  beaten and that it stands — `sector_tally_format` spells `NEW BEST` rather than
  quote a time the run has just replaced, so the pair cannot occur. The same pair
  was in the fit test's own shape table, described as "the ordinary one". Nothing
  failed, because every assertion about that line is a *width*; what it cost is
  that `--shot` is where the press stills and the store page come from. Held as a
  property over the fixture now, so it cannot come back the next time somebody
  wants a realistic-looking row.
- **And one balance outlier, measured rather than felt.** Sector 14 carried the
  highest pressure below the roof (a budget of 70) with a single medkit, no crate
  and no gas canister — the only late interior with neither — while sector 10 at
  58 and sector 12 at 65 each carry two medkits and both tools. It has a second
  medkit on the middle storey now, between the first heavy and a mine so it costs
  something, and `check_docs.py` found all **three** copies of the sector list
  that had to change with it: the page, a comment in
  [gameplay_interaction.c](src/gameplay_interaction.c), and a comment in the test
  that covers the rule. That script working is the whole reason a balance change
  is a two-minute job here.

Three findings were left as decisions and written down instead of changed, which
is its own kind of fix. Trunking is on one floor of twelve, lift shafts on three
and moving platforms on three with one panel each — read as running mechanics
they look abandoned, and they are what makes sector 12 the floor called `DUCTS`
the way the goods lift is what sector 5 is. Sector 2 introduces ten tiles at once
against sector 1's eight and everything after it arriving one at a time, which is
front-loading on purpose and also means sector 2 is the one floor where an
eleventh tile is a real cost. And the console's reinforcement penalty is live on
sectors 4, 8 and 14 alone, because the other five floors carrying a `T` have no
`D` for anybody to come out of — defensible, since the vault and the roof get
their pressure from fourteen and fifteen men, but a shape nobody had chosen in
one place. All three are in [levels/LEGEND.md](levels/LEGEND.md) now, the last of
them as a sector list `check_docs.py` holds. **A decision nobody wrote down is
indistinguishable from an oversight, and the next reader will either re-raise it
or "fix" it.**

All of it was **checked by breaking the thing it guards and watching the new
tests fail** — twelve mutations: the editor's seating formula reverted to the
console count, the drip dropped from the simulated bound, the dog slot's bit
release removed, each of the four mine rules disabled in turn, the refusal
message left a name short, the console sector list made wrong in the prose, and
the impossible time pair put back. Two of the twelve are worth naming for what
they say about writing the check rather than the code: the mine mutations had to
be applied to the *detection* rather than to the message, because three of the
four report through one `report_add` whose format string is split across source
lines — and the wide monkey was re-run afterwards at 32 seeds and 120 seconds a
sector, plus the determinism replay, both clean, because two of these changes
move tiles on shipped maps and a map edit is not a comment.

**And then the tree was read from the outside one more time with every gate
green, and the thing it found was a check that counted to seventeen while asking
twelve — in the one place this file had already found that exact defect and fixed
it next door.** Every gate was green throughout: `make test`, `make lint`, `make
sanitize`, `make win` at nought warnings, `make coverage` at `none` functions and
531 lines, `make coverage-shell` with no drift. Four findings, and the order they
are written in is the order of what they cost a player.

- **Sector 15 took a heart 1.25 seconds after the player arrived on the wall, on
  94 of 256 seeds, with nothing pressed.** `SPAWN_GRACE_SECONDS` is the rule that
  the first three seconds of a sector belong to the player, and
  `test_a_sector_gives_the_player_a_moment_to_read_it` loops over
  `EMBEDDED_LEVEL_COUNT` — so it reads as covering the campaign. The stepper it
  drove was the *interior* frame, and a climb has no men on it, so the five walls
  passed by having nobody to answer with; measured with the interior passes, no
  facade loses a heart in thirty seconds. What a wall costs a heart with lives in
  `gameplay_climb_update`, which nothing in that test called.
  The map was the other half. A facade hazard wakes on **vertical** distance
  alone and its first delay is a quarter to a little over half of the ordinary
  one, so a bird can be in the air 0.6s in; sector 15's lowest `v` sat three rows
  above the spawn and four columns across, the closest pairing on any of the
  five, and **a bird is the one hazard out there with no windup** — a thrower
  shouts and leans out first. Moved to six rows up, where 11 and 13 keep theirs,
  it is 0 of 256 and the first heart at 14.3s. Both modes are driven in their own
  order now and the rule is in [levels/LEGEND.md](levels/LEGEND.md), which had
  said where an `r` and a `v` go on the window grid and nothing about how far the
  lowest one is from `S`.
  Two things worth keeping. `test_the_whole_frame_survives_a_monkey_on_the_controls`
  **had the same hole in the same five sectors** and was fixed by driving both
  modes; this check, one screen over, was not — which is this file's "fixing one
  half of a symmetric defect" with the halves in the same file. And a loop bound
  that names the whole set is worse than a missing check, because it answers the
  question a reader would otherwise ask.
  The same pass also put `gameplay_combat_update_hazards` into the interior
  stepper, whose own comment had promised "the hazards he is standing in" and was
  the one clause of it nothing delivered.
- **`RESET CONTROLS` put nine actions and four slots each back to the defaults on
  a single press.** No arm, no confirmation, no undo anywhere in the game, drawn
  in the same white as every row that changes nothing. `RESET RECORDS` had been
  arm-then-confirm since it existed and `ABANDON RUN` is armed *and* red — so of
  three destructive rows, the one that destroys the thing a player cannot rebuild
  by playing was the one that asked nothing.
  The comment over `SETTINGS_RECORDS_ARMED_DETAIL` opened *"the only row on
  either sheet that cannot be undone"*, which is both the argument for the arm
  and the reason nobody recounted: **a comment claiming to have enumerated
  something is the last place anybody recounts.**
  The fix is not a second flag. `bool settings_records_armed` became
  `SettingId settings_armed_row`, so the sheet has one thing to remember however
  many rows arm; which rows those are is `settings_row_armed_detail`'s answer,
  read by `game.c`'s handler, by the renderer that swaps the detail line and by
  the fit check that measures whichever line the player is reading — the last of
  which had the armed string in a hand-written `LOOSE[]` list, which is what let
  a second armed row arrive unmeasured. `game.c`'s three-way `if` over the
  page-opening ids became `settings_row_opens`, which is what makes the property
  checkable without a list: **an action row either opens a sheet or asks twice**,
  never both and never neither, held by
  `test_every_action_row_either_opens_a_sheet_or_asks_twice`.
  The RECORDS page is the one place the pause sheet's own rule — "the item that
  cannot be undone must never be the one sitting under the thumb" — cannot hold,
  because the four rows above it are readouts the caret steps over and
  `settings_first_row` has exactly one row to land on. That is written down beside
  the row now rather than left looking like an oversight, and it is why the arm is
  load-bearing there in a way it is not anywhere else.
- **A crate ignored every moving surface in the building**, and two of the three
  were reachable by pushing on shipped maps: on sector 6 a crate shoved off the
  walkway drops down the goods-lift shaft and the deck drives through it (19
  simulation steps of overlap), and on sector 12 one goes through a cracked panel
  (83). `crate_position_clear` asked about masonry and other crates and nothing
  else. Nothing was unfinishable — the route model has never counted a crate as
  floor — but the two things this game draws as floor both had a solid object
  falling through them, and **no gate could see it: the soak drew both frames
  every run.** That is the `--shot` lesson for the fourth time.
  The three surfaces get three different answers on purpose. A **lift shaft** is
  refused outright rather than its deck, because a deck is the one moving surface
  here that travels *up into* things and `level_update_elevators` must not start
  asking about crates. A **falling panel** holds one, because a panel only ever
  falls away, so support is the entire interaction — and it does not arm, since
  `triggered` is Chuck's weight alone. A **moving platform** holds one too, with
  the limit written down: its own pass carries nobody, so it slides out from under
  a crate and drops it, and nothing on any shipped map can reach one.
  `test_a_crate_rests_on_what_the_player_does` drives every crate on every sector
  both ways, and its last assertion is the one that keeps the other three honest:
  **a crate on ordinary floor still travels.** Every rule added there is a rule
  that could wedge a box, and "nothing overlaps" is exactly what a wedged box
  delivers — the weak-wall check's "satisfied by making the map worse" shape, one
  mechanic over.
- **And two sentences that had gone stale, in opposite directions.** The pause
  sheet's OPTIONS row read `SOUND, DISPLAY AND ASSIST` a release after the
  options sheet became four pages with ASSIST two levels down — not false, which
  is why nothing caught it, just a description of a layout that no longer exists
  on the one line whose job is saying what is behind a door. And
  [docs/story.md](docs/story.md) said "a thirteenth name anywhere is a thirteenth
  man the docket does not have", which the game contradicts on its first floor:
  `CREW` is twelve **callsigns** and Voss is not one of them, so `SIX FORTY,
  THIRTEEN WAYS. DO NOT MAKE IT TWELVE.` is arithmetic — twelve badged
  contractors plus the man who badged them — and `TWELVE OF US, ONE OF HIM` is a
  man counting his own shift. The distinction between a name and a callsign
  carries all of it and the rule did not draw it, which is the usual failure of a
  sentence saying "these copies must agree" without saying which it counted.
  The first rewrite of the pause line said `THE SHEETS BEHIND IT` and **failed
  the suite**, because `test_the_sheets_spell_the_tuning_they_quote` reads any
  pause detail containing SHEETS as a claim about `MANUAL_PAGE_COUNT` and requires
  exactly one row to make it. That is the check being right, and it is why the
  line names no count at all.

All four were **checked by breaking the thing they guard and watching the new
tests fail** — six mutations: sector 15's `v` put back three rows above the
spawn (the grace check fails on 4 of its 16 seeds), the bindings row's arming
removed, its armed line lengthened past the plate, and each of the shaft and
panel rules disabled in turn, both of which fail on the shipped campaign rather
than on a fixture, which is what proves the campaign-wide half actually reaches
the two real cases. Re-measured afterwards: `make coverage` at `none` functions
and **527** unexecuted lines, down from 531.

**And then it was read again from the outside with every gate green, and the two
findings that matter are both the same shape: a rule measured on the wrong axis,
and a line nothing timed.** Every gate was green throughout and stayed green —
`make test`, `make lint`, `make sanitize` with the full sweep, `make win` at
nought warnings, `make coverage` at `none` functions, clang's static analyzer at
nought findings over the whole SDL-free tree, and a monkey widened to 128 seeds
and 240 seconds a sector across all seventeen with an invariant the suite does
not have (the *player* walled in — it checks guards and dogs and never the man
being played), plus the same over the four restrooms, which nothing in the suite
simulates at all. None of that found anything. What follows is what survives it.

- **The second climb of the campaign was the harshest wall in it, and the curve
  ran backwards for three sectors.** Sector 7 took a heart off a climber who
  pressed nothing at **7.18s on 45 of 256 seeds**; sectors 13 and 15, the two
  tallest walls in the game, both wait about fifteen seconds, and 3 and 11 never
  bite at all. The cause is one character: its thrower `r` stood in the spawn's
  own column, and moving it eight columns across takes the wall to nought at 256.
  Proved by mutation rather than by reading — moving the *bird* instead changes
  nothing.
  What makes a wall different from a floor carries the whole finding. A floor
  does nothing at all to somebody standing still, which is why
  `SPAWN_GRACE_SECONDS` is three seconds and why every check in this tree stops
  there. A **gust carries a passive climber three to five tiles sideways**, and
  that is what walked him into the arc; a thrown object's horizontal reach is
  `THROWN_OBJECT_SPEED` over the 1.35s its travel time is clamped to, about 8.6
  tiles, so a source the drift puts him under is in range and one four columns
  further out is not.
- **And the authoring rule that should have caught it was measuring the wrong
  axis, with two shipped maps breaking even the version it stated.**
  [levels/LEGEND.md](levels/LEGEND.md) said "keep the lowest `r` or `v` six rows
  above `S`", derived once from sector 15's bird and never re-measured. Measured
  properly — one source at a time on a real wall's geometry, 128 seeds of thirty
  seconds — it is **exactly right about birds and silent about throwers**: a `v`
  costs nothing from six rows up at any column and is worth 79 to 106 seeds of
  128 at three rows up anywhere near the spawn, while an `r` stays dangerous at
  six *and* nine rows up and is decided by how many columns lie between it and
  the spawn, with a cornice in between worth more than either.
  Sectors 3 and 7 were both sitting at three rows and both measured clean, which
  is what a one-axis rule buys: they were saved by their cornices rather than by
  their placement and nobody had looked. Both are six rows up now and **it bought
  nought at 512 seeds of sixty seconds**, which is worth writing down rather than
  dressing up — the maps obey the campaign's own rule, and that is the whole of
  what changed there.
  The check is behavioural rather than positional and that is deliberate: how far
  is far enough depends on which cornice is between the two, which is a fact about
  the floor plan and not about the tile — the same reason the weak wall's rule and
  the docket sheet's live in the route model rather than in the grid parser.
  `test_a_climb_does_not_bite_before_its_first_gust_blows_out` gives a wall its
  own window, derived from the wind cycle rather than picked: `FACADE_WIND_CALM_MAX`
  plus the warning plus the gust, 11.05s, by which point the wall has shown the
  player the one thing it does on *every* seed rather than on the lucky ones. It
  is honest rather than fitted, and the way you can tell is that measured at 15s
  the figures are **identical** — 23 of 256 on the old sector 7 and nought on
  every other climb, with nothing at all appearing in between, so the boundary is
  the map's and not the constant's.
- **The line between sectors was on the glass for a fifth of a second, and how
  long depended on how big the next floor was.** The band carrying the story beat
  for ten of the sixteen sector boundaries and the score line with `DOCKET n/12`
  in it is drawn while `STATE_LEVEL_START` is up and nowhere else — and that state
  lasts exactly as long as the tile reveal, which is `width * height / 3000`
  seconds: **0.18s on sector 1, 0.43s on the tallest climb**, for two lines and
  about 120 characters. A one-line crew barb holds `CHATTER_HOLD_TIME`, 3.8s.
  Every gate was green and the reason is the sharpest thing on this page: **the
  fit checks measure whether a line is too wide, the soak draws the frame so
  `make coverage` counts it, and `--shot` proves the pixels are there. None of
  the three can see how long anybody had to read them.** A line that cannot be
  read is the same defect as a line that does not fit, one axis over — and
  [docs/story.md](docs/story.md) put `DOCKET n/12` on that band specifically so a
  player could learn the count *while there was still something to do about it*.
  `level_reveal_hold_for` holds the reveal open for `SECTOR_TALLY_HOLD_TIME`,
  which is `CHATTER_HOLD_TIME` because two answers to "how long does a sentence
  stay readable" would be two numbers nothing holds.
  **The first attempt let the band ride on into play instead, and looking at it is
  what killed it.** After the reveal the player is standing at his spawn, and on
  all five climbs and four of the five interiors that carry this line that spawn
  is the bottom row of the map — directly behind a band pinned to the bottom edge.
  It hid the climber for the whole hold. A reveal has nobody drawn on it yet, so
  it is the one screen this line can own; that is the fourth time `--shot` has
  paid for itself and the same lesson every time, except that this time what it
  caught was *the fix* rather than the bug.
  The cost is 3.8s of non-interactive time on the ten window transitions, against
  `LEVEL_TRANSITION_DURATION`'s **9.4s** on the six that show the report instead —
  so this is the cheaper of the two beats rather than a new expense, and it makes
  a reveal somebody had deliberately sped up into an animation long enough to be
  one.
  The test is the property rather than the duration:
  `test_a_stretched_reveal_lasts_the_same_on_every_map` requires the **spread**
  across the seventeen maps to collapse and measures the unstretched spread in the
  same loop to require that it is wide, because "it depended on the map" is the
  actual bug and a check asserting one duration per map would pass a stretch that
  was still proportional to the tile count.
- **And `--shot-at` accepted anything at all, on the one switch in this binary
  that produces something rather than checking something.** `SDL_atof` answers
  nought to `abc` exactly as it answers nought to `0`, and nought is a perfectly
  legal lead-in, so `--shot-at abc` photographed frame one, logged
  `Wrote 1 frame(s)`, exited nought and put a file on disk. This is the class
  `--soak` and `--page` each got a third answer for, written up at length near the
  top of this file, and `--shot-at` arrived after that work and did not inherit
  it. The two switches beside it escaped **by luck rather than by design**:
  `shot_plan_broke` refuses a frame count under one and a rate under
  `MIN_FRAME_RATE`, so a typo there decayed to a value that happened to be out of
  range, which reads exactly like a decision.
  Worth knowing why the guard added for precisely this cannot see it:
  `tools/soak.sh` counts the capture's files off the disk rather than trusting the
  exit status, because a capture that wrote **nowhere** must not pass — and a
  capture of the wrong **moment** writes a file and sails through. All three read
  through `SDL_strtod` now and refuse anything they cannot consume whole, which
  also catches `3s`, and a missing value is a refusal rather than a fallback.
- **And writing that fix turned up a worse one underneath it: `--shot-at nan`
  never came back at all.** `nan` and `inf` are things `SDL_strtod` reads happily
  and consumes whole, and every guard in `shot_plan_broke` is a `<` comparison —
  all of which are false against a NaN. So the lead-in was set to something that
  counts down forever: no capture, no error, no exit, a process still running
  five minutes later. That is the failure every workflow under `.github` spends
  its `timeout-minutes` on, produced by a switch whose only callers are scripts,
  which is word for word what this file already says about `--soak`. It predates
  the typo bug above and would have outlived a fix aimed only at that. Non-finite
  is refused in the parser now rather than downstream, because downstream is
  eleven `<` comparisons and each of them is a place to forget.
- **And the first draft of the fix ate a good error message, which is the lesson
  of the three.** It answered `-1.0` for malformed — and `-1.0` is a number a
  command line can perfectly well contain, so `--shot-at -1`, which
  `shot_plan_broke` had always refused with "expects a number of seconds of
  nought or more", started being refused **silently** instead. **A sentinel
  inside the range of the thing it stands for is not a sentinel.**
  `SOAK_MALFORMED` gets away with exactly that shape only by luck: a soak of
  minus one second is refused by the same branch with the same words either way.
  The answer comes back through a pointer now, and the check that caught it was
  running every input past the switch and reading what it said — the same habit
  that found the NaN one line up, and the reason both are in this list rather
  than only the one that was looked for.

Two things were measured and deliberately left alone, because a decision nobody
wrote down is indistinguishable from an oversight. Sector 15 carries nine `r` to
eight `v` where the four walls below it are symmetric — it is the tallest wall and
the only one ending at a roof, so the extra thrower is the finale and not a slip.
And the four restrooms are simulated by nothing in the suite: they are checked
structurally, validated by the editor and drawn by the sweep, and the monkey run
over them for this pass came back clean, so what is missing is coverage rather
than a fix.

All of it was **checked by breaking the thing they guard and watching the new
tests fail** — five mutations: sector 7's thrower put back in the spawn column
(the new grace check fails on 10 of its 64 seeds, naming the sector and the
second), `level_reveal_hold_for` made a no-op (which prints the defect's own
figures back — 0.188s, 0.229s, 0.338s), the same function made to stretch by a
constant instead of by the tile count, and each of the two malformed-number paths
in `main.c`. Re-measured afterwards: `make coverage` at `none` functions and
**525** unexecuted lines, the wide monkey clean at 128 seeds and 240 seconds a
sector, and the passive-climber sweep at nought on all five walls for thirty
seconds where sector 7 used to fail at seven.

**And then it was read from the outside once more with every gate green, and what
it found was a record spelled two ways and a harness that could not hit anybody
twice.** Every gate was green throughout and stayed green — `make test`, `make
lint`, `make sanitize`, `make win` at nought warnings, `make coverage` at `none`
functions and 525 lines, `make coverage-shell` at 41 with no drift, and clang's
static analyzer over the whole tree (its only two findings are `argv[i]` in
`main.c`, which C17 guarantees non-null below `argc`). Seven findings. The first
two are the ones to keep.

- **`BEST SCORE 0` and `DOCKET 0` on the manual's own record sheet, where every
  other screen says `--`.** [run_tally.c](src/run_tally.c) exists so that a
  record reads the same wherever it is read, and it had three readers: the end
  cards through `run_tally_format_score`, the options sheet's RECORDS page
  through `run_tally_format_record`, and THE RECORD sheet in the field manual,
  which spelled its two run figures with an `SDL_snprintf` of its own. So a
  fresh install opened that sheet and read nought twice, while the page beside
  it read the same file and said `--`, and while the seventeen sector cells on
  the very same card — which do come through that file — said `--:--`. The card
  contradicted the page, its own grid, and the rule the file it bypassed exists
  to hold. `DOCKET 0` is the one that costs something: it reads as "your best
  night carried no sheets" rather than "no night has finished", which is word
  for word the misreading the game-over card's `SCORE 0 - BEST 0` was fixed to
  stop, one screen over and surviving that fix.
  `test_no_end_card_quotes_a_record_before_there_is_one` closes its own body
  saying the card and the page are asked the same question in the same state,
  "which is the check that would have caught this" — and it asks exactly those
  two. **The third reader was a renderer literal on the far side of the SDL
  boundary, where nothing could compare it with anything.** What was missing was
  not the rule but a way in for a caller holding the number instead of the file:
  `run_tally_format_record_value` and `run_tally_format_record_line` are that,
  the card draws through them, and `test_every_screen_spells_a_record_the_same_way`
  asks the property of every figure at once with no list of which string is right.
  `RUN_TALLY_RECORD_LINE_W` is the card's own column, measured by a test of its
  own, because those two lines were a renderer's literals in a renderer's layout
  — the state the options sheet's footer was found in with the line already off
  the plate.
- **The monkey could not be hit twice on twelve of the seventeen sectors.**
  `invuln_timer` is decremented in [game.c](src/game.c) and by nothing in the
  SDL-free tree, so a harness that rebuilds the frame has to rebuild that too.
  `test_the_whole_frame_survives_a_monkey_on_the_controls` does it in the facade
  branch and did not in the interior one — the floors with all the men, the dogs,
  the mines, the fans and the spikes on them — so the timer latched at
  `INVULN_TIME` on the first contact and never expired. Measured: **110 hits and
  7 deaths where the real frame produces 803 and 236**, at eight seeds and sixty
  seconds over the twelve interiors, with eight of the twelve never seeing a
  single death. The death, the checkpoint restore the test performs itself, and
  the whole non-mercy side of `gameplay_combat_check_contacts` were reached by
  nothing on the half of the campaign that can hurt anybody, under a test whose
  name says it drives the whole frame. That is
  `level_update_moving_platforms` again — a step whose only caller is the shell
  is a step the suite does not take by linking the module — and it is this file's
  own recurring defect, a check reporting coverage it does not have.
  **It was not a live bug**: corrected, 48 seeds at 240 seconds over the twelve
  interiors — some 33 million interior frames — hold every invariant, including
  one the suite does not have (the *player* walled in; it checks guards and dogs
  and never the man being played). The guard is a duration rather than a death
  count on purpose: how many times a seed kills the monkey is a number somebody
  would have to re-guess whenever a map moved, while "no mercy window outlasts
  `INVULN_TIME`" is a property of the frame, and it fires on either branch.
- **The editor knew two of the four things that open a weak wall.**
  `check_weak_walls` asked for an `N` or a `Z` and reported "nothing to open them
  with" — a claim about the floor, and false on any floor with a gas canister
  beside the patch, since all four blasts go through `apply_blast` and that
  function exists precisely so that a blast does not pick which of the things
  beside it are real. Because the suite holds the campaign to nought warnings, a
  check wrong in that direction does not merely misinform: it **forbids a design
  the game supports**, and the better of the two, since shooting the canister
  next to a patch spends no explosive at all. Proved by running the editor's own
  validator over three variants of sector 2. The rule now asks the mechanic
  rather than a list: a grenade or a rocket can be carried anywhere so either
  settles the floor, a canister answers only for a patch inside its own radius,
  and **a mine answers for nothing** — nothing sets one off but the player's own
  weight or somebody else's blast, so a floor whose only opener is a mine offers
  one way through the wall and it costs two of three hearts. Telling an author
  that patch was provided for would have been the same class of wrong in the
  other direction.
- **`--screen` printed the list of screen names for eight faults that were not
  about a name.** `game_soak_screen` turns a request down for nine different
  reasons and each of the other eight already says which; then the caller
  appended the names anyway, so `--screen manual --page 99` answered "Sheet 99 is
  outside the manual's 10" and immediately told the caller their screen name was
  wrong, with `manual` sitting in the list it had just printed. One `bool` fed
  one message — the SPAWNS parser's own defect, on the switch whose whole job is
  telling a script which names exist. `GameScreenResult` is the third answer.
  Nothing in the suite can reach `main.c`, so the sweep asks it: four bad
  requests, each of which has to *fail* **and** not name the list, because a
  check that only asked for the failure passed throughout — the old code failed
  too.
- **Five animations ignored the capture clock.** A capture replaces the frame's
  `elapsed` with a synthetic `1 / --shot-fps` step, which is what makes a burst
  play back at the rate it was asked for; the backdrop, both world passes, the
  ACCESS lamp and the TRAIL meter read `SDL_GetTicksNS()` instead. Measured over
  a five-frame burst, the world moved by 40, 17 and 6 pixels of difference at 20,
  60 and 200 fps — scaling with the rate as designed — while TRAIL moved 12, 13
  and 17, which is to say by however long the machine happened to take. So
  `make press` produced GIFs whose HUD and backdrop animate at the capture
  host's speed rather than the GIF's. `PresentationState.render_clock` is banked
  from the same `elapsed` the simulation is fed, and it is deliberately still a
  wall clock rather than a simulation one so the strip keeps breathing behind a
  pause sheet. **Fixing it turned up the other half**: the two RNG seeds off
  `time(NULL)` and the tick count, which is what `--seed` is for. With both, the
  same press run twice is byte-identical in all 46 files. Worth knowing what is
  *not* affected: a ceiling fan's hazard box is `fan->x ±
  CEILING_FAN_BLADE_LENGTH` at all times and does not depend on the blade angle,
  so the drawing and the damage could never have come apart.
- **The sector strip printed `BAZOOKA` over six pistol cartridges.** The label
  names the active weapon and the pips are always the clip — "all carried
  ammunition remains visible" — so selecting the launcher put the word over the
  wrong number, with the one rocket the player had three slots to the right and
  nothing joining the two. A rule under the slot the label names is what says
  which; it is a slot's own ink wide, sits inside the console, and the geometry
  is in [game_config.h](src/game_config.h) with the rest of the row rather than
  as a literal in the renderer. `PISTOL` and `BOLTS` mark nothing, which is
  right: one is the pips and the other is a pocketful with no count anywhere.
  Every gate was green over it and always would have been — a counter cannot
  tell a frame that was drawn from a frame anybody could read, and what fixed it
  was looking at all seven staged poses side by side.
- **And the repair the climb-height fix rests on was never made a rule.** "The
  `S` moved to the bottom row" is what made `map.height` and the climb's rise the
  same number, and nothing held it, so sector 7 still carried a blank row under
  its spawn — the only wall of the five that did, making its map rise + 2 where
  the others are rise + 1. Nothing was broken by it: the rise was right, the
  order was right, and the camera clamp never showed the row. What was wrong is
  that the campaign kept one wall where the cheap reading and the true one
  disagreed, on the exact pair of quantities that had already been confused once,
  so the next reader of `map.height` would have been right about four walls.
  **A repair a check depends on is a rule, and wants holding like one.**

One thing was measured and deliberately left alone, because a decision nobody
wrote down is indistinguishable from an oversight. `CHUCK_APP_ORG` is `"rob"`, so
every player's disk gets a folder of that name beside their settings
(`~/Library/Application Support/rob/Chuck`, `%APPDATA%\rob\`). It is consistent
with `CHUCK_APP_ID` — `cz.rob.chuck` publishes under the same word — and SDL
recommends against an empty org, so the alternative is a publisher name, which is
a naming decision rather than a defect and not one a sweep gets to make.

All seven were **checked by breaking the thing they guard and watching the new
tests fail** — eleven mutations: the record value made to print nought (which
fails in four places, including the one that prints the figure back), each branch
of the monkey's mercy tick removed in turn, the weak-wall rule reverted to the
console-era `N`-or-`Z`, its canister and mine fixtures each moved out of and into
reach, the screen list put back on every refusal (which fails on three of the
sweep's four requests), and sector 7's blank row restored. Re-measured
afterwards: `make coverage` at `none` functions and **534** unexecuted lines,
nine of which are the new file's own defensive arms — two `case
RUN_TALLY_RECORD_COUNT`, two `snprintf < 0` blocks and two guards a validated
caller cannot reach — which is the same unreachable class the three formatters
beside them already carry, written down here rather than chased.

**And then it was read from the outside once more with every gate green, and what
it found was a rationale standing in for a check on the one switch whose whole
job is terminating.** Every gate was green throughout and stayed green — `make
test`, `make lint`, `make sanitize`, `make win` at nought warnings, `make
coverage` at `none` functions and 534 lines, plus a stricter compile than this
tree sets (`-Wshadow -Wformat=2 -Wcast-qual -Wwrite-strings -Wmissing-prototypes
-Wfloat-equal -Wswitch-enum`) which came back at nought real findings across the
SDL-free tree and **one** across the whole shell. That one warning is where this
starts. Three findings, and the order they are written in is the order of what
they cost.

- **`--soak inf` logged `Soaking for inf seconds, then closing` and then never
  closed.** `parse_soak_seconds` read through `SDL_atof`, `inf` is a thing
  `SDL_atof` reads happily, it is greater than nought, so it passed the range
  check and became a budget that counts down forever: no capture, no error, no
  exit, a process still running when something else kills it. `--soak 1e400` is
  the same hole reached by arithmetic instead of by spelling, and `--soak 3s` and
  `--page 3s` were quietly read as 3 — the second of those being a sheet number
  the sweep then logged as `manual sheet ok`, which is this file's own recurring
  defect reached by a typo rather than by a missing switch.
  **The comment on `parse_shot_number`, eighty lines below the call, had already
  written every sentence of this down**: that "`SDL_atof` cannot fail", that a
  non-finite lead-in meant the process "**never closed**", that this is "the
  failure every workflow under `.github` spends its `timeout-minutes` on,
  produced by a switch whose only callers are scripts", and — in as many words —
  "which is the same sentence this file already carries about `--soak`". That
  clause was not a comparison. It was a description of a live bug in the same
  file, and it sat there being right for a release. **A rationale is not a
  check**, which is this page's own defect with the object swapped one more
  time, and a rule stated beside one of its three call sites is a rule holding
  one of them.
  The fix is not a third strict parser, it is `parse_switch_number`: the
  strictness written down once and the *policy* left where it already was, since
  what range a number falls in and what a refusal costs the process differ per
  switch on purpose. All four numeric switches read through it now, `--level`
  included — its documented asymmetry is about the exit *code*, not about what
  counts as a number, and `--level 3s` was sector 3 in silence. `parse_seed`
  stays out on an honest reason: a seed is any 64-bit value and a `double` stops
  holding one at 2^53.
  **And the guard for it needs a clock rather than an exit status**, which is the
  part worth keeping. `make test` links no SDL, so `main.c` is reachable by
  `tools/soak.sh` alone — and the failure here is a run that produces no status
  to check and no line to grep, because it does not end. So `refuse()` runs every
  case under a watchdog and names an overrun as what it is; without one, a
  regression in this fix would hang `make sanitize` instead of failing it, which
  is the very thing being fixed, one level up.
- **A line on the net told the player two minutes where the dial says nine.**
  `TWO MINUTES AND THIS ROOF IS SOMEBODY ELSE'S.` is gated from sector 14, whose
  wall clock reads 00:51, and the comment above it explained the number as "two
  minutes to 01:00 is the penthouse and the roof, and nowhere else" — arithmetic
  on a campaign divided fifteen ways. It is the exact twin of the intel table's
  `THE SETTLEMENT CLOCK IS RUNNING` row, which said TEN for the same reason, was
  found, was corrected to the dial's own arithmetic, and is derived by
  `check_docs.py` — **whose docstring for the helper that derives it opens by
  calling that row "the one line in the game that states a remaining
  duration"**. This was the other one. A comment claiming to have enumerated
  something is the last place anybody recounts, and the same docstring also said
  eleven rows are suppressed by a window where it is ten.
  **The number was wrong and the sector was not, and it took a wrong fix to
  establish that.** The first attempt moved the line to 17, where the dial gives
  three and "this roof" is the one under the speaker's boots — and
  `test_no_two_sectors_in_a_row_go_quiet` refused it, because that gate is what
  carries sector 14: emptied, 13, 14 and 15 go quiet in a row. The beat coverage
  `crew.c`'s own header is about was resting on the line whose number was false.
  A duration in a sentence is a fact about the clock; where the sentence is said
  is a fact about the campaign, and only one of the two was broken. So the edit
  is one word, and `crew_duration_lines` holds the *pair* — the number in the
  words against the dial at the sector in the gate — because checking the string
  alone would pass the gate being put back to 14 without a murmur. It matches on
  the shape of the claim rather than on a line anybody listed, so a second clock
  line on the net is checked by having been written, and an empty match set is
  louder than a pass for the reason `check_lists.py` already has an empty-set
  guard.
- **The monkey's mercy cap was derived from a constant the monkey cannot
  produce.** `test_the_whole_frame_survives_a_monkey_on_the_controls` closes by
  requiring that no mercy window outlast `INVULN_TIME`, under a comment saying
  "the slack is a step either side of the rounding". `INVULN_TIME` is 1.5s and is
  written by exactly one line in the tree — `finish_player_death` in
  [game.c](src/game.c), on the shell side, which no harness in that file calls.
  What a hit opens is `PLAYER_HIT_INVULN`, 1.2s. So the slack was 73 steps rather
  than one, a quarter of the quantity being measured, and the failure message
  quoted a constant the run could not contain. Proved by mutation: a window made
  15% longer runs 332 steps, fails the cap derived from the constant the
  simulation writes, and **passes** the one that was there.
  The two constants are both right and neither is a duplicate — a respawn and a
  bruise are different events — but one is named for its mechanic and the other
  reads like the general case, and the general-sounding name is the one a reader
  reaches for. That is the whole of how it happened.
  **And the tight cap exposed the other half, which is a sampling bug in the
  counter.** `gameplay_damage_player` re-arms from nought and the decrement runs
  before the contact pass, so on the step a window reaches nought the player is
  hittable again *within the same frame*: two legitimate windows with no frame
  between them read as one of twice the length. Measured at 576 steps against a
  288-step window in the penthouse washroom, where a dog and two men make that
  ordinary rather than lucky. The campaign's seeds have not produced one yet,
  which is the whole argument for not leaving it — it is a false failure waiting
  for a map to move. A window is told from a continuation by the *value* and not
  by the sign: inside one the timer only ever falls, so a rise is a re-arm and
  nothing else is, which needs no constant and no tolerance. All four rooms
  report 288 against a cap of 290 now.

Two things were measured and deliberately left alone, because a decision nobody
wrote down is indistinguishable from an oversight.

**The four washrooms' reward is held equal by a test and their cost is held by
nothing.** `test_the_restrooms_are_four_rooms_rather_than_one` requires one gun,
one grenade and one medkit in each, arguing that the detour "pays the same
everywhere" on purpose. Nothing measures what it charges. Driven through the
interior frame — which nothing in the suite does, these four maps being simulated
by no test at all — the rooms off sectors 1, 5, 9 and 14 run 46, 100, 72 and 48
route steps for the round trip, and cost a passive player a heart on 22, 64, 64
and 22 of 64 seeds **inside thirty**, earliest at 16.3s, 9.9s, 4.9s and 9.4s.
(The window is part of the figure and was left out of it for a release, which is
how it came to sit two paragraphs from a claim about ninety seconds that turned
out to be measuring something else. At ninety the lobby's room is 57 of 64 and
the penthouse's 61; the earliest times are unchanged, which is what says the
budget rather than the seeds is the difference.) The roster ramps with
the sector (one man, one man, two, two and a dog) and neither of the other two
figures does: the archive's room, at two men and a heart on every seed inside
five seconds, will usually take the heart before the player reaches the medkit
they went in for. Every invariant holds on all four, including the one the
campaign monkey does not keep — the player himself walled in — so what is missing
is a shape somebody chose, not a fix.

**And the three seconds a sector owes the player has 0.83s of margin on three
floors.** At 512 seeds nothing dips under 3.83s, so `SPAWN_GRACE_SECONDS` holds;
but sectors 9, 10 and 17 all land on that same figure to three decimals, which is
not a spread of seeds — it is one deterministic chain, a guard who can see the
spawn from the first frame taking `ENEMY_NOTICE_TIME` plus `ENEMY_AIM_TIME` plus
a round's flight. `test_a_sector_gives_the_player_a_moment_to_read_it` is a cliff
(`lost_at < 0.0f`) and cannot say that, exactly as `fits` could not say how close
the options sheet was to the bottom of the frame before `spare_rows` was added.
The hazard budget curve is monotone across both modes (interiors 6 to 89, climbs
20 to 43), so what is un-shaped here is the reaction time and not the floors.

All three were **checked by breaking the thing they guard and watching the new
checks fail** — seven mutations: `--soak` reverted to `SDL_atof` (which fails the
two infinities as *overruns of the watchdog* rather than as statuses, and `3s` as
an acceptance), `--page` reverted to `SDL_atoi` (which fails both of its cases),
the crew line's gate put back to 14 with the corrected words, its words put back
to TWO with the corrected gate, the clock line reworded to name no number at all
so the check loses its subject, and the mercy window lengthened by 15% against
both caps in turn. Re-measured afterwards: `make coverage` at `none` functions
and 534 lines, unmoved — all three fixes are on the far side of the SDL boundary
or inside a check, which is the honest reason a figure this page keeps asking for
did not budge.

**And then it was read from the outside once more, and the one thing on this page
that was costing something on every machine was the switch that *produces*
rather than the switch that checks.** Every gate was green throughout and stayed
green — `make test`, `make lint`, `make sanitize`, `make win` at nought warnings,
`make coverage` at `none` functions and 534 lines with no drift, and clang's
analyzer over the whole tree with
`core,deadcode,security,unix,nullability` enabled (its only findings are the two
known `argv[i]` false positives in `main.c`, plus two `security.ArrayBound`
reports in `editor_doc.c` whose path assumes a negative `grid.height` — every
entry point clamps to two or more, verified). Five findings, and the order they
are written in is the order of what they cost.

- **A capture came out at whatever window the runner's settings opened, and the
  MANIFEST said 800x552.** `game_init` applies the saved `fullscreen`, and
  `screenshot_write` reads back the render target — which under letterbox
  presentation is the *window*, as its own comment says. So on a machine with the
  flag on, every `--shot` frame came out at that display's size with the logical
  frame scaled into it by whatever non-integer factor the display implied.
  Measured on this developer's machine: **800x552 with the flag off and 1024x706
  with it on**, a 1.28 scale, and the glyph stems in the enlarged one are visibly
  uneven — which is the exact pixel-art damage
  [tools/press_kit.sh](tools/press_kit.sh) goes to trouble over for the cover
  ("a single non-integer resize of pixel art either blurs it or…"), applied to
  every still *before* its own `-resize 200%` ever ran.
  **One file made both claims.** The MANIFEST written into every press kit said
  the captures were taken "at the window the game opens (800x552) and the
  settings it ships with"; the same script's header, seventy lines above, said it
  "reads and writes the settings and progress of whoever runs it, the same as
  playing the game does. So the title screen carries a resume chip if this
  machine has one, and the record sheet shows this machine's times." The second
  was true, which made it the bug and the MANIFEST the lie — and the leak carried
  the runner's CRT filter, reduced motion and *assists* in with it, so MORE
  HEARTS on meant five hearts in the HUD of every press still. This is
  `--seed`'s own argument left half-finished: a capture is a measurement and a
  measurement has to be repeatable, and a seed only pins the part of the frame
  that comes out of the RNG.
  **And it ran the other way too, which is what a gate must never do.** `make
  soak` and `make sanitize` banked their own numbers into the developer's
  `progress.cfg` — `--screen cleared` finishes a sector — so `best_score 3230`,
  `furthest_sector 2` and `sector_time 1 20.52` on this machine are the sweep's
  figures and not anybody's play. A test gate rewrote the player's records.
  The fix is one seam rather than six branches. `--shot`, `--soak` or `--screen` on
  the line makes the run a *scripted* one (`GameRunKind`, `PlatformState.scripted`), and
  `pref_file_path` — described in its own comment as "the only part of either
  file that needs a platform at all" — answers no path for one. Both loads apply
  their defaults *before* asking for a path and both saves return the moment they
  do not get one, so that single check is the whole of "shipped defaults, nobody's
  disk written", at six call sites with no new branch at any of them. The saved
  fullscreen is skipped beside it, which is what leaves the window at the logical
  frame. A run driven by a hand is untouched: `--level` is the editor's playtest
  button and the title screen is somebody's evening, and both are the player's
  display and the player's save.
  **The guard is a measurement, not a status.** `tools/soak.sh` already counted
  the capture's files off the disk because a capture that wrote *nowhere* must not
  pass; a capture of the wrong **size** writes a file and sails through that,
  exactly as one of the wrong **moment** does. It reads the BMP's own header now
  and holds it to `VIEW_W`/`VIEW_H` grepped out of
  [game_config.h](src/game_config.h) — and the MANIFEST derives the same two
  numbers instead of spelling them, because a resolution written in a shell
  script is a second copy of a `#define`. Reverted, the sweep prints
  `wrote a 1024x706 frame, not 800x552`.
- **A moving platform carried the player and nobody else, and the first diagnosis
  of that was wrong in a way worth keeping.** It looked like guards and dogs
  falling *through* a plate — a probe put one on each of the three shipped
  platforms and measured drops of 98 to 224px, which reads as the crate bug with
  the object swapped. It is not. `level_move` has always held every body up on a
  plate, in the same block as the falling panel beside it, and a trace shows the
  guard sitting at plate height with `on_ground` set for 0.44s. What happens at
  0.44s is that the plate has moved one tile and he has not: the *support* was
  never missing, only the **carry**, which was the player's pass alone.
  **A two-second probe cannot tell those two apart**, and the shape of the fix
  depends entirely on which it is.
  Reachable by design rather than by accident, which is what separates it from
  the crate's version of the same limit: `enemy_floor_in_col` counts a platform
  as floor *on purpose*, under a comment about a pursuing guard not mistaking one
  for a gap, so the AI walks bodies onto plates — and leaves them resting there
  for 716 frames on sector 14 and **1512 on sector 17** across twenty-four
  minutes of play a sector. Sector 17's plate is eleven tiles long and it is the
  roof. The crate's note reads "its own pass carries nobody, so it slides out
  from under a crate and drops it, and nothing on any shipped map can reach one";
  the second clause is what does not carry over, and it is why that note made
  this easy to leave.
  `carry_with_plate` and `plate_under` are one rule for every body now, so the
  clearance test the player already had — a tile is 32 and a body is 26, and being
  over the plate is not the same as fitting where it is going — is not written
  twice. Measured after: sector 17 goes from 1512 resting frames to **3631**,
  which is the same guards riding instead of falling. The janitor is deliberately
  left out and the reason is written beside the loop: he is the one body whose own
  floor test refuses every moving surface in the building, so he never steps onto
  a plate, and an arm nothing can reach is an arm nobody has checked.
- **The lift deck went through the man with the mop.** `janitor_has_floor_ahead`
  asks about masonry and ladders and nothing else, which is what keeps him off
  every moving surface — and a shaft is the one that slips through it, because
  the tile is passable and the *lowest* one in a run has the storey's own floor
  underneath. So on sector 5 he walked into the bottom of the goods lift and the
  descending deck crossed his chest: 15.9px of overlap in both axes for up to
  0.70s, twelve times a minute of play. Nothing carries a janitor and nothing
  crushes him, so the whole of it was a picture — drawn every single run, counted
  as covered, and looked at by nobody until somebody read the frame.
  **A guard in the same place is a different question and stays as it is**, which
  is a decision rather than an oversight. He has a ride, using the lift is what he
  is in the shaft *for*, and the deck brushing through him on the way down to
  collect him is ≤13px for ≤0.42s and drawn under the figure. Every alternative —
  boarding him on contact, stopping the deck for a body, widening the boarding
  window to the player's — changes how guards patrol three shipped sectors, and
  that is not a trade worth making for a fraction of a second of overlap.
- **Three halves of the prologue pursuit, run by nothing.** `chase.c` had 22
  unexecuted lines and they were halves rather than functions, which is why
  `-show-functions` said `none` throughout. `test_chase_kerb_scrape_bleeds_speed_without_damage`
  holds `input.right` and nothing else, so **steering left and the whole left
  kerb clamp beside it were nought** while their mirror images ran two hundred
  thousand times — on the first thing a player of this game does. **Ramming the
  SUV** was executed by nothing at all, which is the interaction a player will
  certainly try and the one the fiction's answer depends on; the branch even
  carries the sentence as its comment ("not a way to stop them, only a way to
  lose the car"). And **`CHASE_PHASE_DONE`** was reached by nobody: every chase
  test drives a handful of seconds and asserts on a state in the middle, so the
  last beat of the prologue was never walked to its end.
  None of the three was a bug — each was verified working before its test was
  written. What they were is three mechanics the suite would not have noticed the
  loss of, and the left kerb is the one to remember: a sign flipped in that clamp
  puts the car off the road and every assertion in the file still passes.
- **And the fuzz corpus was generating the tidy half of its own question.** Three
  thousand files nobody meant, and every one of them ended in a newline — so
  `level_load_data`'s arm for a final unterminated row, which is an ordinary file
  an editor opens, was nought against 5 970 calls. Both parsers handle it
  correctly, verified by hand; the corpus simply never asked. One row in eight
  goes without now.

**And the capture fix cost one branch its coverage, which is worth writing down
because the coverage it cost was luck.** `intro.c` gates five things on
`resume_offered` — the chip's own width, the prompt row's centring, the hit plate
and the drawing — and all of them come off `progress.furthest_sector`, read from
the runner's disk. A developer who had played to sector 2 drew the resume chip on
every sweep; a clean checkout drew it never, and neither of them could tell which
they were doing. Shipped defaults turn that into a guaranteed never, which is
worse only in that it is quieter. `--screen resume` is the answer, and it is a
*world state* rather than a screen in exactly the way `--screen aftermath` is:
`furthest_sector` set to the last sector — the widest two-digit number the chip
has to fit beside START — and `STATE_INTRO` entered. Verified by capture rather
than by counter, because a counter cannot tell a frame that was drawn from a
frame anybody could read: with it the row reads
`R RESUME SECTOR 17 / H FIELD MANUAL / J OPTIONS / ESC QUIT`, without it three
chips re-centred. Adding the name touched the three places the screen list is
written down and [check_lists.py](tools/check_lists.py) held all three, which is
that check doing its job.

All five were **checked by breaking the thing they guard and watching the new
checks fail** — nine mutations: the capture fix removed (which prints the
defect's own figures back, `1024x706 frame, not 800x552`), the guard carry and
the dog carry dropped in turn, the carry's clearance test dropped (which fails on
the *pre-existing* player-into-masonry check, the shared rule working), the
janitor's shaft rule removed, the left kerb's clamp and left steering each
disabled, ramming the SUV made free, and the arrival made never to finish. Two of
the nine are worth naming: the capture fix has two independent guards and
**either alone masks the other**, so the mutation had to remove both — they are
kept apart on purpose, because one answers "which files" and the other answers
"which window", and a later change to `settings_defaults` must not be able to
bring the capture bug back. And the wide monkey was re-run afterwards at 24 seeds
and 120 seconds over all seventeen sectors — some 23 million steps, with the
invariant the suite does not keep (the *player* walled in) — because two of these
changes move bodies on shipped maps and that is not a comment. Clean.

Re-measured afterwards: `make coverage` at `none` functions and **488**
unexecuted lines, down from 534. The moves are honest: `chase.c` 22 to 11,
`editor_validate.c` 101 to 74 and `editor_doc.c` 91 to 86 (the corpus reaching
further), `level.c` 37 to 33. And one went the other way — `gameplay_physics.c`
11 to 12, which is `plate_under`'s own "no plate here" return, the kind of
regression it is honest to have.

**Two things were measured and deliberately left alone, because a decision nobody
wrote down is indistinguishable from an oversight.**

**Spawn exposure is not a shape anybody chose in one place**, and this paragraph
is the page's own warning about a figure in prose coming true. It read: *"he is
never touched once in ninety seconds on any seed"* of sectors 2, 5, 10 and 12,
and re-measured it is true of **one of the four**. What follows is the reading
rather than the old one, with the measurement said out loud so the next person
can repeat it: `step_the_floor_around_a_still_player`, nothing pressed, 64 seeds,
four different seed bases, both readings.

| sector | budget | loses a heart | dies | earliest heart |
| --- | --- | --- | --- | --- |
| 2 | 14 | 0 of 64 | 0 of 64 | — |
| 5 | 26 | 1–5 of 64 | 3 of 64 | 4.80s |
| 10 | 58 | 11–20 of 64 | 0 of 64 | 3.83s |
| 12 | 65 | **46–51 of 64** | **37 of 64** | 6.92s |

Sectors 6 and 8 are as written — 64 of 64 deaths at 7.87s and 8.76s, which is
where the old 7.7 and 8.8 came from, so the *death* reading is the one that
paragraph was measured on. Under it the claim still fails for 5 and 12; under the
touch reading it fails for three of the four. Sector 12 is the one worth naming:
a player who presses nothing loses a heart on three quarters of seeds and loses
the **run** on more than half, on a floor this page listed among the safe ones.

Nothing is wrong with the game. `SPAWN_GRACE_SECONDS` holds everywhere — the
earliest first heart anywhere in the campaign is 3.83s against a rule of three —
and 12 sits on the curve between 10 and 14 rather than off it. What was wrong was
the sentence, and with it the conclusion drawn from it: *"varies by a factor of
infinity … does not track the floor's difficulty"* rested entirely on 10 and 12
reading nought. Read properly, exposure tracks the budget reasonably well and
sector 2's safe opening pocket is the one real outlier — which is the deliberate
one. **A figure this page keeps calling a reading is a reading**, and the
instruction three sections up applies to this one too: re-measure it in the same
commit as whatever moves it.

**The interiors have no bot.** The climbs are driven end to end by
`facade_bot_reaches_window`; every one of the twelve interiors is certified by the
route model alone, and the model's edges are claims about `PLAYER_JUMP_SPEED`,
`PLAYER_WALK_SPEED` and `GRAVITY` written in a different file. Two of those claims
are pinned. A greedy bot steered by the model's own distance field was written for
this pass and reached the way out on **one** interior of twelve, which says
nothing about the maps and everything about the bot — a route model is a graph and
following it needs a ladder policy, a lift policy and a door policy, none of which
greedy steering has. So the edges were driven one at a time instead, which is the
part that can be trusted: **all 63 ladder runs on the twelve interiors climb both
up and down**, all three lift shafts carry a rider to the exact top row of their
own shaft and back to the bottom with no heart lost, and all three moving
platforms carry one 5, 5 and 11 tiles.

**And then "one at a time" was taken literally and run over the whole graph,
which is what that sentence should have said and did not.** Three mechanics out
of a model with five and a half thousand edges in it is not "the edges"; it is
three of them, and the other kinds — the walk, the step up, the one- and two-tile
hole, the spike hop, the step off a ledge, the paired door and every duct edge —
were asked of nobody, on the model that certifies twelve of the seventeen
sectors and every rule built on top of it.
`test_the_body_delivers_every_route_the_model_promises` asks each one: the player
on the source cell, an empty floor, the presses a hand would try, and settled on
the destination at the end of it. **5771 edges, 219 of them mover-dependent and
skipped, nought undelivered** — the four washrooms included, which nothing in
this suite had simulated at all.

Two things make it a gate rather than a survey. What it skips is skipped **by
rule**: a lift deck (a shaft edge means "wait for it", and a deck parked in a
column also catches a fall short of the landing the model names) and a moving
plate (its whole run is floor to the model and one tile of travelling steel to
the game). A crate is not skipped but *removed*, because the model has never
counted one as floor, so the map it certifies is the map without them — which
turned 491 skips into 219 and asked the ordinary question of everything around a
box. And the failure path prints the counts, with a guard on the skip fraction,
because the way a check like this rots is not a false alarm: it is somebody
widening the skip until the sweep asks nothing, and a bound that had quietly
swallowed the campaign would read exactly like a clean run.

Worth knowing what it does **not** catch, because two mutations proved it.
Widening the model's hole rule to three tiles and its spike hop to two both pass
here — the model's own widths are pinned next door by
`test_the_route_model_and_the_body_agree_about_a_hole`, and this test's subject is
the other direction: not whether the model claims the right things but whether
the body does them. Mutated *there* it fires at once — the paired door stopped
teleporting and every `D` edge on sectors 2, 4, 8 and 14 went undelivered, and
trunking made solid to a crawler took every duct edge on sector 12 with it.

An interior bot is still the honest gap, and it is a smaller one than it was: a
greedy bot rewritten for this pass with ladder, lift, door and console policies
reached the way out on **six** of twelve rather than one, which still measures the
bot. What the bot would add over the sweep above is the thing a graph cannot be
asked — whether a *sequence* of legal moves is survivable end to end — and that
is the part nobody has measured.

**And then it was read from the outside once more with every gate green, and the
one shipped bug it found had been sitting in the half of the campaign nothing
simulates.** Every gate was green throughout and stayed green — `make test`,
`make lint`, `make sanitize` with the full sweep, `make win` at nought warnings,
`make coverage` at `none` functions and 488 lines with no drift, and clang under a
stricter dial than this tree sets (`-Wshadow -Wformat=2 -Wcast-qual
-Wwrite-strings -Wmissing-prototypes -Wfloat-equal -Wswitch-enum -Wconversion`)
at nought real findings across the SDL-free tree. What found the bug was running
the suite's own monkey invariants over the four washrooms, which the suite's
monkey does not visit.

- **A crate shoved against a wall put a live dog inside the masonry, and then a
  tile beyond the map, permanently.** `gameplay_resolve_dog_crates` ejects a body
  to the far side of a box by setting its x to one edge of the crate plus or
  minus its own width, and asked the building nothing. A crate is 28 wide against
  a 32 tile and `crate_position_clear` reads its far edge as `x + w - 1`, so one
  driven hard against a wall settles about a pixel inside the wall column; the
  ejection then put the dog's left edge at `crate->x + CRATE_W`, and a body 24
  wide starting a pixel inside a 32 tile is a body **wholly** inside it — the
  state `box_is_walled_in` exists to refuse, on an animal that is then invisible,
  unshootable and standing in a wall.
  What follows is worse than the overlap and is why this is an error rather than
  a picture. The next step of the animal's own walk back to its post runs
  `level_move`'s left clamp, which answers a solid left-edge column with
  `x = (col + 1) * TILE_SIZE` — one tile *further out* — and against a wall at
  the edge of the map that is **off the map**, for the rest of the visit: x =
  1056 on a map 1056 wide, dir flipped, ninety more seconds of it. A slot spent,
  an animal that still counts against `MAX_DOGS`, and nothing the player can see
  or shoot.
  Measured: reproducible by **holding one direction** for 1.44s in the penthouse
  washroom, and on sectors 4, 6, 8, 9, 10, 12, 16 and 17 in as little as 0.02s —
  11 of 20 crate-and-dog map/direction pairs before the fix and nought after.
  Found unaided by the monkey at 2 runs of 256 at 300s each, and only on the
  washroom.
- **Three things about why nothing caught it, and the third is the general one.**
  The suite's monkey keeps this exact invariant for dogs and does not run the
  four washrooms at all — this page has said so in as many words for a release,
  filed as *"coverage rather than a fix"* — and the washrooms are precisely the
  maps that put a crate, a dog and a hard wall on one row. `move_crate_x` asks
  `crate_blocking_enemy` about the men and nothing about the animals, so a crate
  passes *through* a dog, which is what makes the ejection load-bearing rather
  than cosmetic. And a crate shoved at a dog is a **mechanic** —
  `gameplay_kill_dog_with_crate` is what a box dropped on one does — so a player
  is being invited to do exactly this. **A gap filed as "only coverage" is a gap
  until somebody looks**, and the thing on the other side of it was a mechanic
  the game teaches.
- **The fix is one rule for both bodies, because the tail of
  `gameplay_resolve_player_crates` had the same shape.** `eject_from_crate` tries
  the side the body is leaving by and then the other one, and leaves it where it
  is when neither fits — a picture nobody can act on beating a state nothing can
  recover from. The other side rather than nowhere, because dropping the shove
  deletes the mechanic: measured, the fallback is the difference between nought
  frames of a dog drawn inside a crate and forty of them.
  The player's arm was **not** reproduced on any shipped map — `move_crate_x`
  stops a crate before it can carry him anywhere, since when pushing he is behind
  it — and it is in the test with a fixture that reaches it the other way round,
  a box already coasting arriving at somebody who got in front of it. It is there
  so "not reachable" stays a fact rather than an assumption, and its assertion is
  `gameplay_box_tiles_clear` rather than `box_is_walled_in`, because unchecked the
  ejection puts his right edge 14px past the wall: *half* inside it, which
  satisfies the walled-in test and still draws a man standing in the stonework.
- **And an authoring rule that had never been written down turned out to be one.**
  Sector 5 is the only interior above a hazard budget of 20 with neither a grenade
  nor a rocket on its plan, and sectors 2 and 4 under it carry both — so a reader
  counting supply finds it and either raises it again or "fixes" it. It is not an
  oversight: sector 5 has a `U`, every washroom hands out a grenade, and sector 1
  is the same shape. So the rule is **a floor with no explosive on its plan has a
  door to one**, which is a reachability property rather than ten floors and two
  exceptions, and on sector 5 it is what the detour is *for* — the one thing that
  makes that washroom worth more than a free medkit.
  `test_every_interior_can_reach_a_blast` resolves the room off the sector's own
  theme rather than by an index into the sublevel table, so it is a claim about
  the map rather than about the order of a list, and it requires both halves to
  occur: nought behind the door would mean the interesting case is untested, and
  nought on the plan would mean the campaign had been moved behind detour doors.
- **Two smaller things, both of the class this page is built on.** `cutscene.c`
  spelled `CAMPAIGN_SECTORS - CAMPAIGN_CLIMB_SECTOR_COUNT` out again under a
  comment saying "exactly as `sector_tally_set` does it, so the two screens
  cannot come to disagree" — a sentence whose whole content is that two copies
  agree, beside a `campaign_docket_sheets()` that existed and was not called. And
  the prose had drifted into two spellings: `colour` and `behaviour` and
  `synthesised` everywhere except eight places, two of them the store page in
  both of its copies, which is again the page held to the least. Identifiers stay
  American, because SDL's are.

Both real changes were **checked by breaking the thing they guard and watching
the new tests fail** — nine mutations: the dog's ejection reverted to the
unchecked form (which fails on the shipped campaign rather than on a fixture),
the fallback dropped, the ejection disabled outright, the side chosen the wrong
way round, the landing on top of a crate disabled, the player's ejection
reverted, the plant washroom emptied of its grenade, sector 5's washroom door
removed (caught by `check_docs.py` before the suite even runs, which is that
script doing its job), and the paired door and the duct crawl each broken for the
edge sweep written up above. Re-measured afterwards: `make coverage` at `none`
functions and **482** unexecuted lines, down from 488 — the four that moved are
`gameplay_interaction.c`'s door teleport, reached for the first time by the edge
sweep. The suite is 1.09s where it was 0.70s, and the sweep is 0.45s of that.

**And then it was read from the outside once more with every gate green, and
what it found was the building emptying itself.** Every gate was green
throughout and stayed green — `make test`, `make lint`, `make sanitize`, `make
win` at nought warnings, `make coverage` at `none` functions and 482 lines,
clang under a stricter dial than this tree sets (`-Wshadow -Wformat=2
-Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wfloat-equal -Wswitch-enum`)
at nought real findings across the SDL-free tree, and the monkey at 64 seeds and
240 seconds a sector — 44 million steps — with the invariant the suite does not
keep. None of that found anything, and none of it could: **every invariant in
this tree asks whether something is somewhere it must not be, and not one asks
whether anything is still moving.**

Two findings. They are the same defect in two files, and the shape is one this
page has a name for from the other direction: a state whose only exit leads back
into it.

- **A floor left alone stopped patrolling, and the worst of them stopped
  entirely.** `hemmed_in` counted a dog or another guard as a side, exactly as
  it counted masonry, and handed the pair to `enemy_update` as one flag meaning
  "no horizontal escape". The walker zeroes the step on that flag **and** gates
  every one of its reversals on it, so a man with a body against each side could
  no longer walk *or* turn. Two guards who met therefore stopped, the next man
  along walked into them and stopped, and the clump that made was **absorbing**:
  nobody inside one could ever leave, so it only grew.
  Measured over `update_playing`'s own order with the player put where he cannot
  be seen and nothing pressed: a worst unbroken stall of **133.9s on sector 17**,
  102.8s on sector 10 and 36-56s on 12, 14 and 16, with **all twelve of sector
  14's men in one two-tile pile inside ninety seconds** — three storeys' worth of
  garrison stacked six deep at x=829 and x=861, facing each other, for the rest
  of the night. With the player standing at his own spawn instead, six of sector
  6's seven were motionless after two minutes.
  The commit that introduced the flag is called *"Don't let enemies keep turning
  left and right when trapped on both sides"* and it is right about what it was
  for: a man wedged in masonry should stand rather than flicker. What it never
  was is what it reached. **Masonry on both sides of a 26px man is a hole no
  shipped map has, and a body is the common case** — and worse, no reversal in
  the walker is triggered by a body at all, since both of them ask
  `level_is_solid`. So the flag's stated purpose could never fire for the case
  that was firing it.
  The two questions travel separately now: `body_blocks_side` in
  [gameplay_ai.c](src/gameplay_ai.c), because who else is on the floor is that
  layer's question, and `enemy_tile_blocks_side` in [enemy.c](src/enemy.c),
  because the building is what `enemy.c` is handed. Only the building pins
  anybody. A body still *turns* him — with something against the side he is
  walking into and the other side clear he turns round, which is what keeps two
  men from merging at all — and that rule had to go in the walker rather than
  beside the probe that feeds it, which is the part worth keeping: **without
  `enemy_can_advance` vetting the new facing, the new rule and the unsafe-edge
  reversal take turns undoing each other every frame**, which is a man vibrating
  on the spot, the very thing the pin was written to stop. Found at a ladder head
  on sector 17 with the roof's fifteen men around it.
  Measured after: worst stall **9.9s**, and that one is a man waiting for sector
  10's lift, whose round trip is 10.5s.
- **And the animals, which had the same fault for two different reasons.** A
  dog's post is its handler, so the post moves, and a handler who takes a ladder
  leaves the animal on the storey below with an anchor it now has to walk the
  length of the floor to reach. The three questions asked before a step all look
  at the *floor* — is there one, can I drop to it, can I jump to it — and there
  was no fourth about what is in front of the animal's chest, which is a rule a
  guard has had since he was written.
  So masonry stopped the walk in `level_move`, and the line at the foot of
  `update_dog` noticed the step come back zeroed and answered by setting
  `DOG_RETURN` — **the order to walk at the wall again**. A crate did not even
  get that far: it is settled *after* the walk by `gameplay_resolve_dog_crates`,
  which puts the animal back where it started, so the step does not come back
  zeroed and nothing in the frame notices at all. Measured: unbroken stalls of
  **53 to 150 seconds on five of the ten sectors carrying a `W`**, and on sector
  6 an animal pressed into a pushed crate from thirty seconds in to the end of
  the run. `dog_blocked_ahead` is the missing question and it asks about both,
  because a dog climbs neither a wall nor a box. Measured after: **4.6s**, which
  is the animal's own idle beat.
  Its other half is one line and it is the interaction that makes the first half
  work: the forced `DOG_RETURN` at the top of the same block fires whenever the
  animal is past `DOG_RETURN_RADIUS`, which out here is always, so it cancelled
  the escape roam on the very next frame. Both are needed and each fails the new
  test on its own.

Four things are worth keeping, and the first two are the general ones.

**Every gate this tree owns asks where things are; none of them asks whether
anything is still happening.** The soak sweep drew these floors every single run
and `make coverage` counted every line of the AI as executed, because a clump of
twelve men is exactly as executed as a patrol. `--shot` cannot see it either —
this is the first defect on this page that a still frame is no use against, since
a photograph of a stopped floor and a photograph of a moving one are the same
photograph. What found it was asking a question with a *duration* in it, and the
two new tests are the first in this suite that have one.

**A rule about bodies and a rule about the building must not share a flag.** The
whole of both bugs is that one boolean answered "something is there" for two
things with opposite lifetimes. That is this file's "two documents, one question,
two answers" with the object swapped: one variable, two questions, one answer —
and the one it gave was right for the case that never happens.

**The staging is the reason these are gates rather than surveys.** Both defects
take seventy to a hundred and fifty-five seconds of random diffusion to emerge,
and a check that simulates that is a check nobody runs.
`test_a_patrol_does_not_stop_being_one` stages the pile the campaign arrives at —
every man on the floor put where the first of them stands — and
`test_a_guard_dog_does_not_stop_being_one` hands every animal a post inside the
left wall, which makes it walk the length of its own storey past everything
authored on it. Twelve floors and ten floors, thirty and twenty seconds each,
**0.12s and 0.05s of a 1.2s suite**, and both fail on the shipped behaviour by a
factor of two to fifty. It is the `--screen aftermath` move: stage the world
state rather than playing to it.

**And a check that passes because of the bug is worth more than a check that
fails.** `test_a_dog_with_nothing_to_chase_roams_around_its_handler` was green
throughout and had been sitting on the guard defect since it was written: its
handler patrols a twenty-four-tile corridor, walks the animal to within
`DOG_VIEW_RANGE` of a man standing at the far end eight seconds in, and the dog
spends the remaining twenty-two in `DOG_CHASE` — while the patch bound below is
asserted on every step, chase included. It passed for exactly one reason, and the
reason is the bug: the handler reached the left wall with his own dog pressed
against his other side, `hemmed_in` counted the animal as a pin, and **he stopped
there for twenty seconds**. A frozen handler is a `guard_x` that stops moving,
and a `guard_x` that stops moving is a gap that stops growing. The fixture now
puts the player off the floor so its own first sentence is true for all thirty
seconds. `test_dog_escapes_ladder_perch_without_spinning` was the same shape
one function down: it asserted `flips <= 3` over three seconds, and three was
what a dog that had given up produced. **Spinning is a turn that goes nowhere**,
so it counts the ground between turns instead — a tile, against legs that measure
a tile and a half and a spin that measures nothing — which is a property no
number has to be re-guessed for. A count cannot be made to work here at all:
`DOG_TURN_COOLDOWN` already bounds the rate at 2.5 a second, so any cap loose
enough for pacing is loose enough for the spin.

All of it was **checked by breaking the thing it guards and watching the new
tests fail** — seven mutations: a body made to pin again (which fails on its own,
at 21.4s against an 11.2s budget, with the turn rule still in), both halves of
the guard fix reverted together (three sectors at 20-21s, which is the shipped
behaviour), the `enemy_can_advance` veto dropped, the dog's wall-and-crate check
removed, its crate half removed alone, and the forced return put back over the
escape roam. Re-measured afterwards: `make coverage` at `none` functions and
**473** unexecuted lines, down from 482 — `gameplay_ai.c` 77 to 71 and `enemy.c`
15 to 12, which is the new rules being reached rather than anything else moving.
The wide monkey was re-run with both fixes in at 48 seeds and 240 seconds over
the twelve interiors — 33 million steps, with the invariant the suite does not
keep — and comes back clean, because both changes move bodies on shipped maps
and that is not a comment.

Two things were measured and deliberately left alone.

**The four washrooms' cost is still a shape nobody chose**, and the figures are
unchanged by this pass, which is what says the freeze was not what made them
what they are. Driven through the interior frame at 64 seeds with nothing
pressed, the rooms off sectors 9, 1, 14 and 5 cost a heart on 64, 23, 27 and 64
of 64 inside thirty seconds, earliest at 4.86s, 8.99s, 9.50s and 10.02s. The
archive's room is the one to read: two men, a heart on **every** seed, and the
earliest of the four — a detour that will usually take the heart before the
player reaches the medkit they went in for. Nothing is broken by any written
rule (`SPAWN_GRACE_SECONDS` holds in all four), and the roster ramps with the
sector on purpose; what is unshaped is that the *cost* does not ramp with
anything, and that is an authoring decision rather than a fix.

**And a guard hesitates at a ladder head for up to nine seconds.** It is the
longest legitimate hold left in the campaign and it is bounded — measured at
120s it is the same 9.5s it is at 30s — but it is a man standing at the top of a
run doing nothing visible while he decides. Worth a look if the patrols ever
read as sluggish; it is not what this pass was about and changing it would move
every patrol in the game.

**And then the fix for that was read the same way, and it had put the opposite
failure in the pile's place.** Every gate was green throughout and stayed green —
`make test`, `make lint`, `make sanitize` with the full sweep, `make win` at
nought warnings, `make coverage` at `none` functions and 473 lines with no
drift, and clang under a stricter dial than this tree sets (`-Wshadow
-Wformat=2 -Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wfloat-equal
-Wswitch-enum`) at nought real findings across the SDL-free tree. Two findings.

- **The men stopped piling up and started shuddering, and on the roof they did
  it 9.92 times a second.** `body_blocks_side` is a 4px band, and the reversal
  that reads it turns a man away from whoever is standing where he is walking —
  which walks him back out until his neighbour is *just* clear of the band. That
  equilibrium is the defect: it parks a body exactly on the boundary, where a
  fraction of a pixel of drift flips the flag, and with a man on each side the
  two flags alternate and the rule fires on whichever is momentarily set.
  Traced on sector 17: `L=0 R=1` then `L=1 R=0` on consecutive frames with a
  neighbour at d=+29.98 and another at d=-29.97 against a probe that ends at
  ±30, three reversals in 21ms with x unchanged to two decimals, and 325 turns
  in sixty seconds covering a **mean of 6.08px** each. Measured across the
  campaign with the player at his spawn: 8550 guard reversals in eight minutes
  on sector 17, 2498 of them going nowhere, and the dogs with them at 8.33/s —
  `dog_anchor_x` is derived from the handler's facing, so a shuddering handler
  throws his animal's post from one side of him to the other every frame. Six of
  the twelve interiors were affected; sector 12 was the worst per man at 68
  reversals in one second.
  **It is not only a picture.** A facing is what `enemy_has_los` reads, so four
  men on the last floor of the campaign had a sight cone sweeping both ways at
  9Hz, which is to say they could not be flanked — on the floor the roof
  escape is played on.
  The fix is `ENEMY_BODY_TURN_COOLDOWN`: the turn is a decision and has to hold
  long enough to be one. Measured after, the same runs: worst guard 2.93/s,
  128 dead turns where there were 2498, and the stall side unmoved — 240
  seconds by six seeds over all twelve interiors gives a worst unbroken stall
  of 6.65s, which is a janitor's own PAUSE, against a documented budget of
  11.2s.
- **And the loader had two refusals it could never print.** `level_load_data`
  asked the destination rule — how many `E`, `Y` and `R` a mode allows — *before*
  the three rules about how many of each there may be, and every arm of that
  rule already requires `window_count` to be nought or one. So `Y` twice was
  turned down by the sum, and the sentence naming the rule about `Y` was
  compiled and unreachable; `R` twice went the same way in the ordinary shape.
  What an author read was `invalid destinations for its mode (E=1, Y=2, R=0)`:
  three figures, one complaint, and no saying which. This is the SPAWNS parser's
  own defect one file over, and the `--screen` switch's before that — **one bool
  feeding one message** — and the fix is the same one, which is to ask the count
  before the sum. Nothing about which maps load changes; only which sentence an
  author is handed.

Four things are worth keeping, and the first two are the general ones.

**A fix for a liveness bug can be a liveness bug.** The pile was found by asking
"is anything still moving"; this was found by asking the same question the other
way round, and the two are one question — *is a body doing something that goes
somewhere*. `test_a_patrol_does_not_stop_being_one` and
`test_a_patrol_that_meets_another_does_not_shudder` are the two halves and share
their staging, because the crowd that produces a pile is the crowd that produces
a shudder.

**A threshold a rule pushes bodies onto will chatter, and a veto is not a
debounce.** The comment above this reversal already described a version of this
— it is where `enemy_can_advance` came from — and vetting the *new facing*
answers the rule fighting its neighbours and not the rule fighting itself.
Nothing in the walker's other two reversals needs one, because masonry and an
unsafe edge are asked of the tile map and the tile map does not drift; this is
the one reversal that answers a thing which moves, and it was the one with no
pacing at all.

**No gate in this tree could have seen it, `--shot` included.** The soak sweep
draws these floors every run and `make coverage` counts every line of the walker
as executed — a clump of twelve men is exactly as executed as a patrol — and
this is the first defect on this page a still frame is no use against, because a
photograph of a shuddering man and a photograph of a walking one are the same
photograph. What found it was a metric with a *duration* in it, which is what
the previous pass had to reach for as well.

**And the assertion is "a turn that goes nowhere", not a rate.** A plain cap
cannot be made to work: a man on the narrowest shelf a tile grid leaves him
legitimately shuffles three or four times a second, so any cap loose enough for
pacing is loose enough for the shudder — which is word for word why
`test_dog_escapes_ladder_perch_without_spinning` counts ground rather than
turns. Counted as reversals covering less than `ENEMY_SIDE_PROBE`, the campaign
reads **one** per man-second on all twelve interiors, that one being the wall
and the unsafe edge answering a single tile from two probes; the bound is
`ENEMY_BODY_TURN_COOLDOWN`'s own and the mutation reads 8 to 68.

Both were **checked by breaking the thing they guard and watching the new tests
fail** — four mutations: the debounce's gate removed and its arming removed,
each failing on five of the twelve interiors by name and figure, and the
destination check put back in front of the counts, which fails two of the new
test's four cases and prints the defect's own sentence back. Re-measured
afterwards: `make coverage` at `none` functions and **466** unexecuted lines,
down from 473 — the seven are `level.c`'s two refusals, now reachable — with the
debounce itself fully covered. The suite is 1.28s where it was 1.09s, the new
check being thirty seconds a sector on the twelve interiors, the same as its
twin's.

Two things were measured and deliberately left alone.

**The sector par is not a choice between speed and clearing the floor, and the
header says it is.** `SECTOR_PAR_SECONDS` is 134 seconds; the route model's own
shortest walk from spawn to the way out, priced at `PLAYER_WALK_SPEED`, is
between **5.9 and 32.2 seconds** on every interior — sector 10 at 5.9, sectors 4
and 16 the longest at 32.2 and 31.3. So the time bonus pays 2035–2561 of a
2680 ceiling for simply walking to the door, and the reasoning beside the
constant — *"speed is therefore a real alternative to clearing the floor rather
than a rounding error on top of it"* — describes a trade the numbers do not
make: there is time to do both on every floor in the game. The par is
comfortable, which is what that note claims and what it is for; what is not
established is that it costs anything. Nothing is broken and this is a balance
decision rather than a defect, which is why it is written down instead of
changed.

**And the reveal hold does not spend the par, which is worth recording because
it easily could have.** Stretching the between-sectors reveal to
`SECTOR_TALLY_HOLD_TIME` added 3.8s of non-interactive time to every sector, and
`campaign_award_sector_bonus` pays on `level_elapsed_time`. It is safe because
that field is advanced in `update_playing` and `update_facade_playing` alone and
`update_scene` handles `STATE_LEVEL_START` and returns, so the clock has not
started — checked rather than assumed, since a reveal that billed the player
3.8s of his own par would have cost 190 points a floor and appeared nowhere but
the score.

**And then it was read from the outside once more with every gate green, and the
thing it found was a box left on a floor taking a certified route away.** Every
gate was green throughout and stayed green — `make test`, `make lint`, `make
sanitize`, `make coverage` at `none` functions and 466 lines with no drift, and
clang under a stricter dial than this tree sets (`-Wshadow -Wformat=2
-Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wfloat-equal -Wswitch-enum
-Wstrict-prototypes -Wredundant-decls -Wundef -Wdouble-promotion -Wvla`) at
nought real findings across the SDL-free tree. Four findings, and the order they
are written in is the order of what they cost.

- **A crate shoved along the floor beside a ladder blocked the climb.**
  `CRATE_W` is 28 against a 32 tile and a ladder run ends at a slab, so the
  mouth is the tile *above* the top rung — which means a box resting on the
  masonry beside a mouth covers most of it while overlapping no ladder tile at
  all. Measured on sector 9: 22px of the column covered, 10px of gap for a 26px
  body, and the climb that makes two tiles with the box where the map puts it
  makes **31px** with the box shoved eight tiles along that floor. Neither
  `crate_position_clear` nor anything else had a reason to mention a ladder,
  because a ladder is not solid to anything else in the building.
  What it costs is the one thing this tree certifies. `route_never_strands` says
  the way out is reachable from anywhere the player can get to, it is run on the
  map **as authored**, and a crate is neither floor nor wall to it — the first
  half of that is written down and the second was not. Blocking the cell over
  the mouth makes the way out unreachable from the spawn on sectors 4, 9, 10 and
  16, and a pushed crate outlives a death (`LevelRuntime` says so in its own
  comment), so dying does not undo it.
  The rungs are refused the way a lift shaft already is, and the shaft's own
  note is the argument word for word: a column something else has to move
  through is not somewhere a box is parked. Nothing is taken away that anybody
  drew — no crate on any of the twenty-one shipped maps, the four washrooms
  included, is authored within a row of a ladder tile, measured at every
  combination of the padding.
- **The card the campaign ends on held for 1.2 seconds.** `SECTOR_TALLY_HOLD_TIME`
  is this tree's answer to "how long does a sentence stay readable", written when
  the line between sectors turned out to have no time of its own. That band has
  two placements. The reveal was stretched to hold it; the *card* kept a number
  of its own — and the card is the last screen a finished run ever draws, under
  `THE ROOF IS HIS`, carrying the seventeenth floor's stopwatch, the record
  beside it, both bonuses and `DOCKET n/12`: ninety characters, then a cut to the
  outro on a timer no press can extend. The card a run **loses** on has always
  held for three seconds. Every gate was green, and the reason is the one this
  file keeps writing down: the fit checks measure whether a line is too *wide*,
  the sweep draws the frame so `make coverage` counts it, `--shot` proves the
  pixels are there, and none of the three can see how long anybody had to read
  them. **A placement is not readable because its twin is**, which is the same
  sentence this file already carries about the sweep that draws them. It is
  `SECTOR_TALLY_HOLD_TIME` now, for the reason that constant is
  `CHATTER_HOLD_TIME`.
- **The game went on being played while its window was behind something else.**
  Nothing in this tree handled a window event of any kind, and the world is
  driven by `SDL_AppIterate` rather than by input — so alt-tabbing, switching
  desktops or clicking on a browser left the floor running. Measured with
  nothing pressed, 64 seeds a sector: **ten of the twelve interiors** cost a heart
  inside thirty seconds, the earliest at 3.83s, and sectors 6, 8 and 17 cost the
  whole life on 64, 64 and 59 of the 64. Ten seconds of reading an email is a
  life on three floors of twelve. The two that never touch him are sectors 2 and
  5, which is the safe-opening-pocket shape this file has already measured
  twice.
  The argument was already written down four lines from the key that makes it:
  ESC pauses "instead of being thrown away; an accidental ESC must never cost the
  run". A window losing focus is that accident with the hand nowhere near the
  keyboard, and it was the one this game answered by playing on.
  `game_pause_on_focus_lost` goes through `game_toggle_pause` rather than beside
  it, so the states in which there is a run to protect are named once — and a
  **scripted** run is exempt, which is what lets this be a rule rather than a
  risk: a pause sheet over a capture is a picture of a menu and a soak that
  paused itself would spend its budget on a still frame. Focus coming back
  deliberately does not resume.
- **And the editor threw away an afternoon on the close box.** `SDL_EVENT_QUIT`
  returned `SDL_APP_SUCCESS` on the spot, while `ed_open_file` two hundred lines
  up refuses the *same loss* the first time it is asked and explains itself in
  the status bar. One question, two answers, and the dangerous answer was on the
  path everybody uses: there is no autosave here, `editor_doc_save` has to be
  asked, and the title bar's `*` is the only warning. It asks twice now, with the
  same `confirm_discard` and the same idiom — pressing it again is the
  confirmation. `EditorApp.quit` went with it: a field read once by
  `SDL_AppIterate` and written by nothing at all, which is a claim about how this
  program closes that was not true of it.

Two smaller things came out of the same read. The prologue and the ending spelled
one prompt two ways — `ENTER / SPACE TO SKIP` on three screens and
`SPACE / ENTER: SKIP` on the other two, the same words in a different order with
different punctuation, on screens a player sees within a minute of each other;
they are one spelling now, verified by capture rather than by counting, since the
longer form was already drawn at the same x on three of the five. *(They were
not. That fix reached the four beats in `cutscene.c` and left the roll, the
drive's second prompt and the continue card — three more spellings, two of them
naming fewer buttons than work. See the pass at the end of this file: a sentence
written up as fixed is a sentence nobody counts a second time, which is the
defect this very paragraph is recording.)* And the crew's
net was measured: **60.7% of the lines that reach earshot are replaced before
their 3.8s is up**, mean 3.06s, eighteen of 1634 cut under a second and one
replaced inside a single step, so drawn never. That is the documented behaviour —
"a second line inside the window replaces the first outright" — and the figure is
written down rather than changed, because a queue would still be printing the
lobby's jokes two rooms later, which is what the note beside it says.

Three things are worth keeping, and the first two are about the checks rather
than the code.

**The first version of the crate rule was worse than the bug, and only a
measurement said so.** Refusing every overlapping position outright also refuses
the steps that *leave* one, because every step out is still overlapping until the
last of them — which turned nine fall-ins across the campaign into nine boxes
nothing could ever shove out of the rungs again: a soft-lock where there had been
a blocked climb the player could at least undo. A rule of this shape has to be
asked which way it points: a push may not put a box in there, and a box that is
in there may be pushed either way.

**And the first version of the test could not tell either half of the fix from
its absence.** Asked only where a hand lets go, the sweep passes with the
shove-refusal deleted — the topple tidies up after it — *and* passes with the
topple deleted, because the refusal keeps the box out. Two mutations, neither
failing, which means neither half was tested. It asks the invariant on **every
frame** now, with `on_ground` as the only exemption, and both mutations fail on
the shipped campaign. That is this file's oldest defect in the place it is
hardest to see: a check that agrees with the thing it is checking.

**And an arm that heals itself between frames is not an arm.** The topple's
second attempt was written with the span and the distances measured again between
the two tries, on the strength of a case where the near side is walled a tile
out. Mutated, that recompute made no difference to anything: the function runs
every frame, and a box that moved part of the way has a *nearer* side next frame.
What does need answering in one frame is "it did not move at all", which is why
there are two attempts and no recompute — and the fixture that reaches it is a
mouth in a recess, staged rather than pushed, because what has to be exercised is
a geometry the shove-refusal now keeps a player out of. The *fall* fixture beside
it does not reach the topple at all, which is worth writing down rather than
implying: what clears the box there is its own momentum carrying it out the far
side. It earns its place twice over anyway — it is the case a horizontal rule is
structurally blind to, and it is the only drop in the suite long enough to reach
`MAX_FALL_SPEED`, which is a line that stopped being run the moment boxes started
stopping at ladders.

All of it was **checked by breaking the thing it guards and watching the new
tests fail** — seven mutations: the shove-refusal removed, the row of padding
removed, the topple removed, the topple cut to one attempt, every box frozen (the
rule taken too far, which fails on the *old* crate tests rather than the new
one), the cleared card put back to 1.2 seconds, and the reveal's own hold made a
no-op. The wide monkey was then re-run with the player-walled-in invariant the
suite does not keep, at 24 seeds and 120 seconds over all seventeen sectors —
some 11.7 million steps — because a crate rule moves boxes on shipped maps and
that is not a comment. Clean.

Two things were measured and deliberately left alone.

**Ten crate positions still break the route model and none of them blocks the
body.** With the rungs closed, the model — asked as though a box were a wall,
which is the only way to ask it at all — still calls the way out unreachable at
ten positions across sectors 4, 5, 10, 12 and 16. Driven with the real physics
they are the model being conservative rather than a floor cut in half: five of the
six worst were walked over on a jump (a box is 28 tall and a body clears it with
a tile of headroom), and the sixth is a box on a paired door, where the door
still opens because `gameplay_use_door` asks for an overlap rather than for the
tile. Which is worth writing down for its shape as much as its result: **the
route model has no crate in it and cannot be given one**, since where a box ends
up is the player's decision and the model is a statement about the map. What can
be held is the mechanic, and that is what the fix holds.

**And a crate can still be left in a one-tile-high passage.** The rungs are the
case that had a demonstrated cost; a box parked in a corridor with a tile of
headroom is the general version of the same worry, and measured across every
crate on every map pushed both ways, no shipped position wedges one — every one
of them can be shoved on, and every interior can reach a blast
(`test_every_interior_can_reach_a_blast`). Neither of those is what makes it
safe, though: what makes it safe is that no map draws that corridor. A floor plan
that did would want this paragraph re-read.

**And then it was read from the outside once more with every gate green, and
what it found was three claims about the body and one switch that reached half
of the thing its own row names.** Every gate was green throughout and stayed
green — `make test`, `make lint`, `make sanitize`, `make win` at nought
warnings, `make coverage` at `none` functions and 466 lines with no drift,
`make coverage-shell` at 41 with no drift, clang under a stricter dial than this
tree sets (`-Wshadow -Wformat=2 -Wcast-qual -Wwrite-strings
-Wmissing-prototypes -Wfloat-equal -Wswitch-enum -Wstrict-prototypes
-Wredundant-decls -Wundef -Wdouble-promotion -Wvla`) at nought findings across
the SDL-free tree, and the wide monkey at 48 seeds and 180 seconds over all
seventeen sectors — some 35 million steps — plus 24 seeds and 120 seconds over
the four washrooms, with the invariant the suite does not keep (the *player*
walled in). None of that found anything, and none of it could: **every check on
this page asks what the simulation does, and three of these four are places the
tree says out loud what it does and is wrong.**

- **The field manual told the player a two-tile hole needs a ladder, and the
  body clears three.** `ON FOOT`'s movement bullet read "A jump clears a
  one-tile hole in the floor. Two tiles needs a ladder or a lift shaft", the
  authoring page said the same thing conditionally — "In a two-row band … enough
  to clear a one-tile hole in the floor, not a two-tile one. Give him a second
  open row (~87px) before asking for a two-tile jump" — and the illustration
  beside the bullet was captioned `ONE TILE IS A JUMP. TWO IS A LADDER` over a
  drawn gap one tile wide. Measured: **two tiles in a two-row corridor, three in
  a three-row hall**, and more headroom buys nothing because the ceiling stops
  being the limit at three rows.
  The airborne figures both pages are built on are exactly right — 47.8px of
  ground under a two-row ceiling, 86.1px with a row to spare. What the
  arithmetic forgets is the player's own box. **A hole is landed past, not
  cleared**: `level_move` holds a body up while any part of it is over solid
  tile, so he leaves the near lip with his left edge on it and lands the moment
  his right edge reaches the far one, and the ground a gap asks for is
  `width - PLAYER_W` — 38px for two tiles, 70px for three.
  The spike bullet three lines below it in the same file has had that same 26px
  **right** since it was written — "covering 58px of ground while the whole
  26px-wide player box is above floor level" — because a spike does have to be
  cleared entirely. Two adjacent rules about one body, one adding the box and
  one forgetting to subtract it, and only one of them had ever been measured.
  Four things are worth keeping. `docs/levels.md` has carried the true figure
  the whole time ("the physics clears a three-tile hole"), which makes this
  **three documents, one question, three answers** — this file's most reliable
  smell, and the player-facing copy was the wrong one, which is where it always
  is. The direction is the one LEGEND.md already has a paragraph about for the
  *step up* — "do not draw a pocket two tiles up and call it unreachable,
  because it is not, and nothing will tell you" — sitting thirty lines above the
  identical mistake about a hole: **fixing one half of a symmetric defect is the
  most reliable way to stop anybody looking at the other half**, one more time,
  in the same bullet list. Nothing in the campaign rests on the old reading — the
  twelve interiors carry seventy-three one-tile gaps, two two-tile gaps that both
  have three open rows, and four six-tile ones that are storey breaks — so this
  cost an author a rule rather than a player a run. And **the fix landed on the
  words and the picture kept the number**: the caption is a claim about the
  simulation rather than a label on a drawing, so it is derived now, and the
  vignette's gap is two tiles at its own scale, because a picture showing a notch
  under a line saying two tiles is the reader's first reason to doubt the line.
- **SLOWER GUARDS reached a dog only while it was chasing.** The row reads
  `GUARDS AND DOGS MOVE AT 80% SPEED`. A guard carries
  `gameplay_enemy_speed_scale` in every state he has, because it goes into
  `enemy_update` as the speed argument and that function owns his whole walk.
  The animal picks its own speed out of four — patrol, roam, return, chase — and
  the scale was written into exactly one of them. Measured on a flat corridor:
  chase 165/132/194.7 px/s plain/assisted/veteran against return
  135/**135**/**135** and roam 90/**90**/**90**. So a player who asked the game
  to be easier got a dog that closed on them at the full pace right up until it
  saw them, and VETERAN's faster crew left the animals patrolling at the
  ordinary speed.
  There is no argument for the asymmetry — a patrolling guard is no more of a
  threat than a patrolling dog, and he is scaled — and nothing in a file where
  every deliberate asymmetry carries a paragraph said a word about this one, so
  it is the missing half of one rule rather than a decision. The scale is one
  line at the point the speed is spent now, so a fifth dog state inherits it the
  way the guard's does. `test_the_difficulty_switches_reach_a_dog_in_every_state`
  asks the property rather than the four numbers: whatever speed a dog moves at,
  the assist makes it slower and veteran makes it faster, by the scale the sheet
  names, in every state it has. **A switch is worth testing through the half of
  the mechanic nobody happened to look at** — which is the `settings_value_bool`
  shape one file over, on the other kind of row.
  The other direction was measured rather than assumed, because it is the half
  that can cost something: veteran now moves the animals as well, and
  `SPAWN_GRACE_SECONDS` is checked only on the shipped settings. Driven with the
  flag on at 512 seeds the campaign's earliest first heart is **3.42s** against a
  rule of three, where the default run's is 3.83 — narrower and still outside.
  And a slower dog is a dog that could stall: staged the way
  `test_a_guard_dog_does_not_stop_being_one` stages it, the worst hold on all ten
  floors is 0.04s with the assist on against a `DOG_TURN_COOLDOWN` of 0.40.
- **Sector 13 broke the climb's own grace rule on about one seed in three
  thousand, and the check could not have seen it at any sample it was taking.**
  `test_a_climb_does_not_bite_before_its_first_gust_blows_out` ran 64 seeds off
  one arithmetic family (`7000 * (n + 1) + i`). Re-measured at 4096 seeds across
  four independent families, sector 13 costs a passive climber a heart at 8.06s,
  inside a window of 11.05s — confirmed at 1 of 2048 on the test's own family,
  seed 1307. Every other wall is nought at 4096.
  The mechanism is the one this page already wrote up for the thrower, arriving
  at the hazard the rule exempts. `levels/LEGEND.md` said in as many words that
  "a **bird** costs nothing from six rows up at *any* column" — a 128-seed
  reading, presented as a rule, and false at 4096. The first gust carries a
  still climber five to six tiles sideways, out from under the cornice that was
  sheltering him, and a bird descending on his column is the one hazard out here
  with no windup to answer. Sector 13's lowest `v` moved four columns; it is
  nought at 4096 on all four families and the wall's thirty-second profile is
  unchanged — 59 hearts against 63 and the same 42 deaths — which is what says
  this was a grace fix rather than a difficulty one.
  **The seed count is the finding and the map is the symptom.** 64 → 2048 is an
  order of magnitude off the measured *rate* rather than the seed that caught
  today's map, because fitting it to that seed is the shape this page warns
  about everywhere else; it takes the suite from 1.3s to 2.3s, which is what
  this class of check costs and is worth saying out loud. And it is still a
  sample: what the check promises at 2048 is that a wall does not
  bite often, and the campaign is at nought rather than at few. A pinned-wind
  sweep and a staged-drift sweep were both written and both thrown away — the
  first because the seed also decides when the bird launches, the second because
  staging a climber at columns the drift cannot reach in the time available says
  every wall is unsafe. **An over-approximating gate is not a cheaper gate.**

All of it was **checked by breaking the thing it guards and watching the new
tests fail** — nine mutations: the dog's scale reverted to the chase branch
alone (which fails on the returning *and* the roaming dog and prints the
defect's own figures back, `135.0/135.0/135.0` and `90.0/90.0/90.0`), sector 13's
`v` put back four columns (which fails naming the sector, the second and the
seed), the manual's caption put back to ONE and TWO (which prints the two
measured widths back), `PLAYER_WALK_SPEED` cut to 110 and raised to 260, and
`PLAYER_JUMP_SPEED` cut to 300 and raised to 430 — the last four because a width
test that only pins what the body *can* do passes a jump retuned upward, at
which point the prose is wrong again in the other direction, which is the
mistake the first draft of
`test_the_route_model_promises_only_moves_the_player_can_make` made. The wide
monkey was re-run afterwards at 24 seeds and 180 seconds over all seventeen
sectors and 16 × 120s over the four washrooms, and the dog-liveness staging was
re-run with both switches on (0.04s against a 0.40s budget), because a speed
change moves bodies on ten shipped maps and a map edit is not a comment.
Re-measured: `make coverage` at `none` functions and **466** lines, unmoved —
two of the three fixes are prose and a map, and the third is one line inside a
function the suite already walks, which is the honest reason a figure this page
keeps asking for did not budge.

Two things were measured and deliberately left alone.

**A pickup rule stated in the manual is a sector list nothing holds.** `FIGHTING`
says `BAZOOKA: one rocket, even sectors only`, which is true today — sectors 2,
4, 6, 8, 10, 12, 14 and 16 carry one `Z` each and nothing else does — and
[check_docs.py](tools/check_docs.py) cannot see it, because it reads `docs/`,
`README.md`, `LEGEND.md`, `itch/`, `tests/` and two named comments under `src/`,
and a *string table* in `manual_pages.c` is none of those. It is the same class
as the stale sector lists that script was written for, on the copy a player
reads. Left because adding one entry for one claim is worth less than the
paragraph saying where the hole is; the day a rocket moves, this is the sentence
that goes stale first.

*(Both halves of that were wrong, and the shape of being wrong is the reason it
is corrected here rather than quietly deleted.* **A note reporting an absence of
cover it has is the mirror of a rationale reporting agreement it has not, and it
costs the same thing:** the item goes on the list of known holes, the paragraph
explains why leaving it is reasonable, and nobody looks again. `check_campaign_position`
in [editor_validate.c](editor/editor_validate.c) has refused any sector whose
bazooka count is not `number % 2 == 0` as an **error** since twenty-seven commits
before the paragraph above was written, and
`test_the_editor_has_nothing_to_say_about_the_shipped_campaign` holds the
campaign to nought errors — so a rocket cannot move at all without failing `make
test`, which is the opposite of the sentence it was filed under. What *was*
unheld is the direction the note did not consider: the rota and the **wording**
are two copies of one rule, so changing the rota leaves the sheet describing the
old one, and no gate could see that. The phrase is derived from the maps the
editor pins now, and a rota that stops being "even sectors only" is reported with
the list it has become — because no wording is automatically right for that and
the author has to pick one.)

**And the mercy window does not travel through a washroom door.** `invuln_timer`
is a `GameplayState` field and `swap_gameplay_areas` exchanges the whole struct,
so the hearts and the loadout are carried across by hand — with a comment saying
so, "the door is not a heal" — and the blink is not. Take a hit outside, step
through, and the room is free to hit you again on the next frame; come back out
and the sector's own window resumes where it was frozen. All four rooms have men
in them and one has a dog. It is a fraction of a second and it may well be the
right answer, but it is the one field on either side of that door whose
behaviour nobody chose.

*(Chosen since, and it travels: see the pass at the end of this file. Worth
noting what the shape of the note above got wrong, because the note is the kind
this page keeps writing — it reported one direction of a symmetric defect. The
blink was not only lost on the way in, it was *granted* on the way out, from a
window banked whenever the player happened to step through. A field that is
frozen rather than carried is wrong in both directions by construction, and half
of that was invisible for as long as the paragraph described the half somebody
had noticed.)*

**And then it was read from the outside once more with every gate green, and
what it found was a man walking through the furniture on six shipped maps and a
rule about a doorway this page had already written down and left.** Every gate
was green throughout and stayed green — `make test`, `make lint`, `make
sanitize` with the full sweep, `make win` at nought warnings, `make coverage` at
`none` functions and 466 lines with no drift, and the wide monkey at 48 seeds and
180 seconds over all seventeen sectors *and* the four washrooms, some 45 million
steps, with the invariant the suite does not keep. None of that found anything,
and none of it could: two of the three below are pictures, and the third is a
field on the far side of the SDL boundary. Three findings, and the order they are
written in is the order of what they cost.

- **The man with the mop walked straight through the crates.**
  `janitor_side_blocked` asks about masonry, a `D` and — since the goods lift
  went through his chest — a `V`. A crate is the one solid thing on an interior
  that is **not a tile at all**, so a rule written in tiles could not see it
  whatever it asked, and there was no fourth question. Measured with nobody
  pressing anything: on the four floors where the two ever met he spent up to
  **19% of a two-minute visit inside a box**, at the full 26px of his own width,
  and 5.2% of a visit to the lobby's washroom.
  **A guard doing the same thing is deliberate and this was not**, which is the
  whole of why it is a defect rather than a second copy of a decision. The note
  beside `gameplay_resolve_enemy_crates` says a guard is drawn *after* the
  crates, so his side overlap reads as the foreground route past one instead of
  mounting every box on the floor. The janitor is on the ambient-staff layer,
  drawn *before* the floor props for reasons of its own — he passes behind
  counters, planting and ladder rails — so the same overlap is the crate drawn
  over him, and 26 into a 28-wide box leaves the top four pixels of his head and
  the cart. Two bodies, one overlap, and the layer decides which of them is a
  picture and which is a hole.
  **The fix's whole risk is in the other direction, and the first version
  shipped it.** Nothing prevents a *crate* arriving at him: `move_crate_x` asks
  `crate_blocking_enemy` about the men and a janitor is not one, which is right,
  because he may never block anything. So a shove or a fall can leave a box
  sitting on him — and with the new clause applied blindly the strip ahead of him
  is inside that box in both directions. Measured, he turned on the spot for the
  whole of a sixty-second probe on all five floors and never got clear: a rule
  against walking into something has to say that a thing you are already in does
  not count, which is the direction the crate-at-a-ladder rule had to be given
  one release earlier and the same sentence — a push may not put a box
  somewhere, and a box that is already there may be walked out of. With it, the
  worst escape in the campaign is 6.02s against a bound of 8.29s derived from his
  own loop, and his pacing is untouched: worst unbroken stall 7.06-9.37s where
  the shipped figure is 6.77-9.37s.
  Two smaller things came with it. The three beats of that loop were literals
  inside `janitor_set_activity`, and the escape bound is a claim with a duration
  in it, so they are named in [game_config.h](src/game_config.h) now — through a
  helper that rounds rather than truncates, because `0.9f` is not 0.9 and the
  spread would otherwise have lost a hundredth and moved a shipped RNG draw. And
  the test walks the campaign **and the washrooms**, indexed the way the route
  sweep indexes them, because the room is one of the six maps that carry both a
  `J` and a `B` and the four rooms are the maps this suite has historically
  simulated least.
- **The mercy window did not travel through a washroom door, and this page had
  already said so.** That is the part worth keeping: the paragraph above,
  written two passes ago, describes the defect accurately, calls it "the one
  field on either side of that door whose behaviour nobody chose", and leaves it
  — and a thing filed as *possibly right* is a thing nobody looks at again. It
  is the same shape as "a gap filed as only coverage is a gap until somebody
  looks", one directory over.
  It is not a fraction of a second in one direction. `invuln_timer` is a field
  of `GameplayState`, so the swap took it with the frozen area: a player who had
  just been hit arrived in a room holding two men and a dog with **no blink at
  all**, hittable on the first frame, on the detour that costs a heart on every
  seed inside five seconds on one of the four; and coming back out he was handed
  the sector's *old* window, banked at the moment he went in, however long the
  visit had taken. A frozen field is wrong in both directions by construction,
  and the note had only the direction somebody had noticed.
  **The fix is not three assignments, it is deleting the seam** — the same move
  the flash charge's own doorway needed. `gameplay_carry_through_doorway` in
  [gameplay_state.c](src/gameplay_state.c) is the whole of what a `U` hands over
  in either direction, and `game.c` has no opinion about it any more, which is
  also the only reason a test can reach it: `make coverage-shell` has
  `leave_restroom` down as executed by neither gate.
  `test_the_doorway_hands_over_the_blink` drives both worlds through that one
  call and requires them to differ — with the blink handed over the guard
  standing on him costs nothing, and with it dropped, which is exactly what
  shipped, the same guard on the same frame takes a heart.
- **And `--screen reveal` drew a frame the game cannot produce.** The band over
  a reveal names the sector that has just been cleared, and this staged it with
  `current_level` — the sector the reveal is *of*. So every frame that screen has
  ever drawn read `SECTOR 10 CLEAR` over the reveal of sector 10 with the HUD
  beside it also reading 10, and `load_level` has already stepped the counter by
  the time `STATE_LEVEL_START` is entered. It is the impossible time-and-best
  pair the report's own fixture was corrected for, one screen over.
  The default was worse than the pairing. `reveal` was not in `NEEDS_LEVEL`, so
  the sweep drew it on sector 1 — and sector 1 leaves by its stair door, which
  means it shows a *report*, and the report clears the band. The one frame this
  screen exists to photograph was staged on the one boundary in the campaign
  where it never appears. `soak_reveal_sector` derives the first boundary that
  really carries one, for the reason `soak_aftermath_sector` is derived: which
  sectors leave by a window is a property of the maps and has moved once
  already. Sector 1 asked for by hand now gets no band, which is the truthful
  answer rather than a special case.

Three things are worth keeping, and the first two are general.

**A drawing order decides whether an overlap is a decision or a defect**, and
nothing in this tree can see a drawing order. The guard's overlap and the
janitor's are the same numbers; what differs is which pass runs second, three
thousand lines away in [game_render.c](src/game_render.c). The soak sweep drew
these floors every single run and `make coverage` counts a man inside a box
exactly as it counts a man beside one — a counter cannot tell a frame that was
drawn from a frame anybody could read, for what is now the sixth time on this
page, and it is `--shot` that pays for itself again.

**A note that says "this may well be the right answer" is a note that closes the
question.** Both of the first two findings had been *described* in this
repository before they were fixed — one as a paragraph on this page, one as a
rule about a `J` that listed three obstacles and stopped. Writing a decision down
is what this file is for; writing an *undecided* thing down in the voice of a
decision is how it stops being looked at. The honest form is the one the balance
notes use: say what was measured, say what was not, and say what would change the
answer.

**And a rule that changes when a body turns changes every seeded draw behind
it.** Naming the janitor's three beats kept the draw count and the values
identical on purpose — verified, `351/241/91` before and after — but the crate
clause moves *when* he turns, and a turn draws from the shared stream. So six
shipped maps have a different night for the same seed, which is a map edit rather
than a comment: the wide monkey was re-run at 48 seeds and 180 seconds over all
seventeen sectors and the four washrooms with the player-walled-in invariant the
suite does not keep, and comes back clean.

All three were **checked by breaking the thing they guard and watching the new
tests fail** — five mutations: the crate clause removed (which fails on the
shipped campaign, naming the map and the second), its "already inside a box" arm
removed (23.78s against an 8.29s budget), the blink dropped at the doorway, the
blink *granted* at the doorway rather than handed over (which fails the control
that makes the assertion about the window rather than about the fixture), and the
new janitor sector list made wrong in [levels/LEGEND.md](levels/LEGEND.md), which
[check_docs.py](tools/check_docs.py) prints back as a disagreement with the maps.

**And then it was read from the outside once more with every gate green, and
what it found was one offer spelled four ways and a check that had counted three
of twenty-one.** Every gate was green throughout and stayed green — `make test`, `make
lint`, `make sanitize` with the full sweep, `make win` at nought warnings, `make
coverage` at `none` functions and 466 lines with no drift, and clang under a
stricter dial than this tree sets (`-Wshadow -Wformat=2 -Wcast-qual
-Wwrite-strings -Wmissing-prototypes -Wfloat-equal -Wswitch-enum
-Wstrict-prototypes -Wredundant-decls -Wundef -Wdouble-promotion -Wvla`) at
nought real findings across the SDL-free tree. Both findings are player-facing
text, which is the half of this game no counter can read, and both are shapes
this page already has names for.

- **The way past a scene was spelled four ways, and this file says it was fixed.**
  `state_accepts_confirm` in [game_input.c](src/game_input.c) takes SPACE, RETURN
  and KP_ENTER in every state a scene can be skipped from, and START on the pad
  wherever START is not the pause button. That is one answer, and nine prompts
  reported it in four spellings: `ENTER / SPACE TO SKIP` on the four cutscene
  beats and the drive's departure, `SPACE / ENTER: SKIP` and
  `SPACE / ENTER: MAIN MENU` on the credits roll, `ENTER: SKIP THE DRIVE` on the
  drive, and `PRESS ENTER OR SPACE` on the continue card.
  **Two of the four named fewer buttons than work.** The roll offered `$A: SKIP`
  where START skips as well, and the drive's second prompt named ENTER where
  SPACE does the same thing — on the same screen as the departure prompt that
  names both. And the roll is the screen *directly after* the outro: the outro
  and the roll are the two halves of one ending, thirty seconds apart, spelling
  the same offer in a different word order with different punctuation.
  The paragraph further up this file that closes "they are one spelling now,
  verified by capture rather than by counting" is about the same defect and it is
  wrong. The four beats it fixed are all in [cutscene.c](src/cutscene.c); the roll
  is in [game_render.c](src/game_render.c) and the drive's two are in
  [chase_render.c](src/chase_render.c), so nobody counted them — **a fix landing
  on one file's copy of a sentence**, which is this page's oldest shape, in the
  paragraph that records the last time it happened.
- **Nothing in the tree could have said so, and that is not the usual reason.**
  Everywhere else on this page the answer is that a counter cannot tell a frame
  that was drawn from a frame anybody could read. It is true here — the sweep
  draws all five of these screens every run — but the sharper point is that
  **there is no fit check on any of them**: every prompt is a literal inside a
  renderer, on the far side of the SDL boundary, which is exactly the state
  `settings.c`'s footer and the sector HUD's weapon row were each found in. A
  table of words that stays inside its renderer is a table nothing measures, and
  a prompt is a table of words with one row.
- **The fix is deleting the seam rather than guarding it.** `PAD_CONFIRM_KEYS`
  and `PAD_CONFIRM_PAD` live in [pad_hint.h](src/pad_hint.h) — the file that
  exists to spell a button for whatever is in the player's hands, SDL-free and in
  `TEST_SOURCES` — and every one of the nine prompts is now that macro plus its
  own offer, because *what* is being skipped is that screen's business and *which
  buttons say so* is not. `PAD_CONFIRM_DRIVE` is the one deliberate exception and
  it is not a spelling: during the drive A and B are the pedals, so the skip is on
  Y and START is pause, and a prompt offering either would name a button that does
  something else.
  What is left to guard is a *tenth* prompt arriving spelled by hand, which is
  how all nine of these did, so [check_lists.py](tools/check_lists.py)
  asks it: a `pad_hint` call whose pad form names `$START`, or whose key form
  names both ENTER and SPACE, must build that half from the macro. Deliberately
  **not** "a key form mentioning ENTER" — `PRESS $Y TO ENTER WC` and
  `PRESS $Y TO ENTER DOOR` are the verb, and a check that cannot tell those apart
  is a check somebody turns off. That control is one of the mutations.
- **And the string fix alone would have shipped a line off the edge of the
  screen.** The drive's two prompts were drawn at `win_w - 180` and
  `win_w - 196`, two magic numbers each measured by hand against whatever string
  happened to be there. Naming both keys makes the longer one 31 characters,
  which is 248px against a 196px allowance — on the prompt a player only ever
  sees after failing the drive several times, which is to say the one nobody
  would have photographed. Measured rather than reasoned about, by forcing
  `CHASE_SKIP_AFTER_ATTEMPTS` to nought and reading the frame: right-aligned the
  line's ink runs x=540 to x=759 of 800 and reads
  `ENTER / SPACE TO SKIP THE DRIVE`; at the old fixed x it runs 604 to 759 —
  **the same right edge, so the tail is simply gone**, which is what a clipped
  line looks like rather than one hanging off the frame. Both are right-aligned
  off the drawn line now — and with the shorter of the two the arithmetic lands on
  exactly the old position, `168 + 12 == 180`, which is what says the margin is
  the one that was always there rather than a number picked to make this come out
  even.
- **The manual's tuning check held three claims and there were twenty-one.** The
  comment over `test_the_sheets_spell_the_tuning_they_quote` opens *"Three numbers
  on the same sheets are facts about the tuning"* — a sentence claiming to have
  enumerated something, which this page already calls the last place anybody
  recounts. The three it names are the guard's cone (twice, sentence and picture),
  `FACADE_CHECKPOINT_STEP` and the pause sheet's `THE TEN SHEETS`. Read off the
  tables, the same sheets also spell `TERMINAL_HACK_TIME`, `ENEMY_TALK_DURATION`,
  `PLAYER_MAX_HP`, `EXPLOSION_DAMAGE`, `PLAYER_CONTINUES`, `MAX_AMMO` on two
  sheets, `ENEMY_HP`, the relationship `ENEMY_HEAVY_HP` has to it, and how many
  interiors leave by a window — all in hard-coded English, and every one of them
  correct on the day it was checked, which is the point: what was missing is a
  guard rather than a fix.
- **And six more that are numerals rather than words, which is why the second
  reading found them and the first did not.** `EXTRA_LIFE_SCORE_STEP` —
  *"every 10000 points is a spare life"* — four clock readings (the front door at
  `00:22` in a bullet **and again in the same sheet's strap**, Voss on the roof at
  `00:57`, the money leaving by air at `01:00`) and one percentage. A check built
  to compare a *spelled* number cannot see a figure printed as digits, so
  widening this one to "every claim on the sheets" meant looking for both shapes;
  the first pass over it only looked for one — and the strap was found last of
  all, by looking at the rendered sheet rather than at the table, because a strap
  reads as a title rather than as a claim.
- **And the class was not confined to the manual: the options sheet carries five
  more.** DIFFICULTY's three switches print `FIVE HEARTS PER LIFE INSTEAD OF
  THREE`, `GUARDS AND DOGS MOVE AT 80% SPEED` and `FASTER CREW, ONE LIFE, NO
  CONTINUES` — `PLAYER_ASSIST_MAX_HP`, `PLAYER_MAX_HP` again,
  `ASSIST_ENEMY_SPEED`, `VETERAN_LIVES` and `VETERAN_CONTINUES`, on the one
  screen whose entire job is reporting state. Every check that screen has asks
  whether a line *fits*: this file's own conventions already name `settings.c` as
  the table of words that had its geometry in the renderer and no fit check, and
  the *numbers* in it were the half nobody looked at after that was fixed. The
  new check walks `settings_rows` over all four pages, so a claim added to any of
  them is checked by having been written. `SLOWER GUARDS` is the row worth
  noticing twice: its words were right the whole time while the switch behind
  them reached one of a dog's four states, which is two defects on one row found
  a release apart.
  The clock ones are the sharper half. `check_docs.py` derives every dial it can
  find in `docs/`, `README.md`, `levels/LEGEND.md` and the store page — the note
  above its own `dial()` records three pages and a renderer comment caught quoting
  the fifteen-sector night — and it does not read `manual_pages.c` at all, holding
  two named comments under `src/` and nothing else there. So the hour the night
  starts on was derived on every page a reader of *this repository* sees and on
  none of the sheets a player opens mid-run, which is the same swap the hearts
  bullet below describes, three sentences long instead of one. Moving
  `NIGHT_CLOCK_FIRST_MINUTE` now fails on three sheets at once.
- **The one that stings is `Three hearts a life`, because the *store page's* copy
  of that exact sentence was already derived** — `check_docs.py` has held
  `itch/page.md` to `PLAYER_MAX_HP` since it learned to read that directory. So
  the sentence was checked on the page a stranger reads before playing and
  unchecked on the sheet a player opens mid-run, which is this file's
  "the page written for people who are not reading this repository was the page
  held to the least" **with the audiences swapped**. It was found by mutation
  rather than by reading: raising `PLAYER_MAX_HP` failed in `make lint` before the
  suite ran at all, and isolating which copy had caught it is what turned up that
  the manual's had not.

Three things about the shape of the new check are worth keeping.

**It matches over a sheet's whole body rather than line by line, and that is not
a style choice.** These tables are hard-wrapped by hand, so `That is four` ends
one row and `seconds of a man facing the wrong way` begins the next; two of the
ten claims straddle the wrap, and the three checks above it — all per-line —
could not have seen either. `manual_sheet_body` joins the title, strap, caption
and every line, because a claim is a claim wherever on the sheet it is printed.

**One of the ten is a relationship rather than a number, and a sentence can hold
one where a `#define` cannot.** *"A heavy takes twice the rounds"* is a claim
about `ENEMY_HEAVY_HP` and `ENEMY_HP` together; the words are required to say
TWICE and the arithmetic is required to *be* twice, because either half moving
alone is the sheet lying. A heavy at three times the rounds under a sheet still
saying "twice" and a sheet saying "three times" over a doubled constant are the
same defect from opposite ends.

**And the welded-stair count is derived from the maps rather than from
`CAMPAIGN_CLIMB_SECTOR_COUNT`.** They are the same number today and they are not
the same fact: this one counts the *floors* whose stair core is welded, and it
only equals the number of climbs while no two climbs are adjacent — two in a row
would leave one welded interior below the pair and the second climb reached from
the first. The sheet is making the claim about the floors, so
`interiors_that_leave_by_a_window` counts floors. It is five.

All of it was **checked by breaking the thing it guards and watching the new
checks fail** — twenty mutations and one control: the drive's prompt put back
on its fixed x, which is the one of them no check can catch and which was read
off the frame instead; a `$START` literal put back in
the continue card, a both-keys form spelled by hand beside a `$Y` pad form, the
`TO ENTER WC` verb prompt required *not* to trip the lint, each of
`TERMINAL_HACK_TIME`, `ENEMY_TALK_DURATION`, `PLAYER_MAX_HP`,
`EXPLOSION_DAMAGE`, `PLAYER_CONTINUES`, `MAX_AMMO`, `ENEMY_HP`,
`ENEMY_HEAVY_HP`, `EXTRA_LIFE_SCORE_STEP`, `NIGHT_CLOCK_FIRST_MINUTE`,
`NIGHT_CLOCK_TOTAL_MINUTES`, `PLAYER_ASSIST_MAX_HP`, `ASSIST_ENEMY_SPEED`,
`VETERAN_LIVES` and `VETERAN_CONTINUES` moved in turn — `MAX_AMMO` failing on
**both** sheets by name and the first minute on **four** claims across two, which
is the several-copies case working — and two rewordings, one that drops the number and one that drops the
sentence, the second failing as
`no sheet makes the claim 'sectors the stair door' any more`. Re-measured
afterwards: `make coverage` at `none` functions and **466** lines, unmoved, which
is honest rather than disappointing — one fix is a macro on the far side of the
SDL boundary and the other is a check.

Four things were measured and deliberately left alone, and one of them is a
figure this page should keep.

**A floor whose alarm has gone up still comes back down, and an alarmed crowd
does not pile up.** The pile and the shudder written up above were both found by
asking whether anything is still moving; an alarm points every man on the storey
at one point, which is the same crowd arriving by a different route and nothing
had driven it. Driven — the alarm raised on the frame the sector loads, four
seeds, forty seconds, all twelve interiors and the four washrooms — the worst
stall a guard takes that is not aiming, talking, riding a lift or using a switch
is **1.7s to 9.2s**, the two long ones being sectors 6 and 10 waiting on a lift
whose round trip is 10.5s, and dead turns run at 0.03–0.28 a second against the
2.93 the debounce allows. The alarm expires where the player loses sight and
stands while he is in the open, which is what `ALARM_CALM_TIME` is for.

**An animal whose handler is dead stands still for up to 12.9 seconds.** That is
the one figure here worth writing down. With a live handler the anchor moves
every frame, so the dog trots; orphaned, `dog->guard_x` freezes where the man
fell and the *only* thing that makes an anchored animal move is a 45% roll
against a 0.5–1.7s timer, whose tail is geometric. Measured over ten floors at
eight seeds and thirty seconds: 7.2s to 12.9s, against the 4.6s this page records
for a dog with somebody to follow. It is not a stall in the sense the two
liveness fixes were — nothing is blocked, the roll always lands eventually, and
`anim_time` advances regardless so the drawing is an idle animal rather than a
frozen one. What it is is a beat nobody chose: the quiet route's whole point is
handlers going down, so this is the state a well-played floor *ends* in, and the
animal's pacing there is a side effect of an anchor that has stopped moving.
Changing it would move every dog in the game.

**The lobby's evacuation is clean and the man can always get out from behind a
box.** Both were asked because nothing had. Over 128 seeds all **512** civilians
reach the doorway and dissolve in it — nought stuck, nought fading anywhere but
the door, slowest clearance 8.45s — so `CIVILIAN_STUCK_TIME`'s fade, which exists
so that a blocked runner leaves the shot rather than running on the spot, fires
on nothing in the shipped lobby. And a determined hand — one direction plus jump
held for twelve seconds, from both sides of every crate on every interior and in
all four washrooms, then the opposite direction for four — always walks back out,
593px at worst. That last figure is the one that matters: the first version of
the probe measured the *push* and reported sector 12 at 234px, which is not a
wedge but a box correctly refused at a duct, and reading it as one would have
been the "check satisfied by making the map worse" mistake at the diagnosis end.

**And no body ever leaves the map it stands in.** The suite's monkey asserts that
nothing alive is inside masonry and says nothing about the world's edges, which
is the invariant the dog-ejected-past-the-wall bug broke — x = 1056 on a map 1056
wide, for the rest of the visit. Driven at four seeds and sixty seconds over all
seventeen sectors and the four washrooms with a monkey on the controls, the worst
overhang for a guard, a dog, the janitor or the player is **0.00px** on every
map.

**And then it was read from the outside once more with every gate green, and
what it found was the one state in the AI with no way out but success.** Every
gate was green throughout and stayed green — `make test`, `make lint`, `make
sanitize` with the full sweep, `make win` at nought warnings, `make coverage` at
`none` functions and 466 lines with no drift, clang under a stricter dial than
this tree sets (`-Wshadow -Wformat=2 -Wcast-qual -Wwrite-strings
-Wmissing-prototypes -Wfloat-equal -Wswitch-enum -Wstrict-prototypes
-Wredundant-decls -Wundef -Wdouble-promotion -Wvla`) at nought real findings
across the SDL-free tree, and the wide monkey at 64 seed bases and 240 seconds
over all seventeen sectors — some 65 million steps — with the invariant the suite
does not keep. None of that found anything, and none of it could: **every
liveness question this tree has learned to ask is about a body that has stopped
moving, and this is a man who never stops.**

- **A guard who runs for a wall switch he cannot reach runs for it for the rest
  of the sector, and mutes the floor while he does.** `raising_alarm` is the only
  "go somewhere" state in this AI with no clock on it. A suspicion expires at
  `ENEMY_INVESTIGATE_TIME` and the man turns on the spot and goes back to patrol;
  an encounter expires at `GUARD_ENCOUNTER_RESET_TIME`; the alarm itself calms at
  `ALARM_CALM_TIME`. This one cleared on arrival, on a flash in the face, on a
  takedown, on somebody else raising it — and on nothing else.
  The walk has no pathfinder in it, and that is right rather than wrong:
  `enemy_update_walking` only follows the target's column once the guard is
  already on its floor, and otherwise keeps his patrol direction so the ladder
  rule can find a way up, which is what a man chasing Chuck needs and a coin flip
  for a fixed point three storeys away. Measured over the twelve interiors,
  **151 of 388 staged commitments never left the state in three minutes**; a
  guard on sector 1's upper corridor walked *down* to the lobby and paced it for
  the whole run with the switch four rows above where he started.
  **What it cost is not one lost man.** `another_guard_is_raising_alarm` holds the
  roll to one runner at a time, so a runner who never arrives mutes the *floor*:
  a body left where somebody would find it committed a guard in **231 of 368**
  seeds and raised the alarm in **134**, and on sector 16 it committed one in 25
  of 32 and raised the alarm **nought** times. `GUARD_BODY_ALARM_CHANCE` is 65 and
  `GUARD_ALARM_CHANCE` is 45, so the risk the whole quiet route is played against
  was being handed, two times in three, to a man who might spend the rest of the
  sector walking. And `if (!switch_pursuit)` means a committed man ignores the
  player for targeting, so he is not a pursuer either.
  **`update_body_discovery`'s own comment closed the case on it**: "the moment
  anybody reaches a switch the alarm goes up and this function returns at the
  top". That is a rationale resting on its own success, which is this file's
  "a rationale reporting agreement it does not have" with the object swapped
  again — and the sentence reads as a proof, so nobody looked.

Three things about the fix are worth keeping, and the second is the general one.

**The clock is derived twice over and picked nowhere.** `ALARM_RUN_DETOUR_ALLOWANCE`
is four because of the runs that *do* arrive: their time is a median 1.35 and a
p90 3.60 of the straight-line walk the distance implies, so four keeps 92% of
them. The budget is then divided by the speed the man will actually travel at,
so `SLOWER GUARDS` buys a slower runner rather than a shorter run — an assist
that also quietly muted the alarm would be a fourth switch nobody asked for. And
it is capped at the time it takes to walk the length of the floor, because past
that he is not on an errand, he is lost; the cap comes off `map.width` rather
than off a number typed in a header, or the widest sector in the game would buy
the longest silence — sector 17 is 58 tiles across and the distance alone granted
one guard 86 seconds.

**A clock on its own does not bound the errand, and the first version of this fix
shipped exactly that.** A man with two switches and no memory of which he has
already failed at picks the near one, times out, picks the near one again from
wherever he now stands, and shuttles for the rest of the sector with a fresh
budget every time: the same one-way door with a timer bolted to it.
`alarm_switches_tried` is the memory and `nearest_alarm_switch` takes it as the
set to skip, so it is one refusal per switch and then back to his floor — which
is also what hands the roll to the next guard. Measured after: **0 of 388** posts
still in the state at three minutes, and a body left on the floor now raises the
alarm in 148 of 368 seeds against 134, with sectors 4 and 14 going from
13-of-19 and 24-of-32 committed-and-arrived to all of them.

**And a hand-set flag is not a commitment, which is why the fix needed a public
entry point rather than a field.** Two existing tests staged `raising_alarm =
true; alarm_switch_index = 0;` by hand, and a run staged that way carries no
budget — the state the game cannot produce. `gameplay_ai_send_to_alarm` is what
both the floor's decisions and both tests go through now, which is the
`player_carry_loadout` move one file over: the rule exists once and the suite
reaches the same one the game does.

Two more came out of the same read and neither was a shipped bug. Both are the
"reached but never acted on" kind, on mechanics with a duration in them.

- **The mine's delay had never been a delay.** `MINE_TRIGGER_DELAY` is the whole
  of what a mine is — the step arms it, the blast comes a beat later, and the
  comment beside it spends three lines on what the beat is for: "long enough to
  run out of and long enough for whoever is chasing him to run into". Two hearts
  of three, on the most expensive tile in the game.
  `test_mine_damage_emits_feedback` swallows the delay in a single
  `MINE_TRIGGER_DELAY + 0.01f` step, so the branch that holds a triggered mine
  with time still on its clock was executed **nought** times: a duration handed
  to the simulation as one step is not a duration, it is an instant, which is
  the conventions page's own "a frame count is nearly always a duration somebody
  has already divided by a rate" arriving from the other end.
  Driven at `SIM_STEP_DT` all three claims hold — armed for 0.454s against a
  0.45 rule, two hearts for standing on it, untouched for walking off
  (`PLAYER_WALK_SPEED` for the delay is 60.8px against a `MINE_RADIUS` of 36),
  and a guard standing over it when it finally goes off is killed by it. The
  escape needs the control beside it, which is that standing still on the same
  fixture does lose the hearts; otherwise it is an assertion about where the mine
  happens to be.
- **A dog had never bitten twice.** `test_dog_bite_is_announced_and_survivable`
  walks the growl, the escape and the bite and stops on the bite, so both
  `dog->bite_cooldown -= dt` and `dog->attack_timer -= dt` in `update_dog` were
  executed nought times. Without a cooldown a dog standing on Chuck is not a
  hazard, it is a death.
  **And which constant governs it is not the one named for it.**
  `gameplay_combat_check_contacts` returns before the dog loop while the mercy
  window is up, so the rhythm a player feels is `PLAYER_HIT_INVULN` plus a fresh
  `DOG_BITE_WINDUP` — the bite has to be announced again — which measures 1.504s.
  `DOG_BITE_COOLDOWN` is 0.75 against a mercy window of 1.2 and therefore decides
  nothing at all: deleting its guard entirely changes no observable behaviour,
  verified by mutation. It is kept rather than deleted because it is the animal's
  own clock and would bind the moment the mercy window came down, and it is now
  pinned from the outside by the rhythm, so either constant moving past the other
  arrives as a changed rhythm rather than as a silent one. The lunge the renderer
  draws was a literal `0.18f` beside that named cooldown — the fifth of the class
  the throw branches were named for — and is `DOG_BITE_ACTION_TIME` now, with a
  note saying it is *not* the knife's identical 0.18.

All three were **checked by breaking the thing they guard and watching the new
tests fail** — nine mutations: the alarm deadline removed (which fails on 31 of
the campaign's guard posts by name and second), the tried-switch memory forgotten
(which fails every assertion of the fixture test), the floor-length ceiling
removed, `MINE_TRIGGER_DELAY` set to nought, `MINE_RADIUS` cut to one, the mine's
timer left un-ticked, the dog's bite left un-announced, its lunge timer latched,
and `PLAYER_HIT_INVULN` cut below `DOG_BITE_COOLDOWN` — the last of which is the
interesting one, because it is the mutation that makes the dominated constant
start binding and the rhythm test is what notices. Two further mutations
deliberately do **not** fail and are the reason the write-up above says what it
says: the dog's cooldown guard removed, which nothing can see, and either of the
two constants the rhythm is derived from moved, which moves both sides of a
derived assertion. Re-measured afterwards: `make coverage` at `none` functions
and **458** unexecuted lines, down from 466, and the wide monkey clean at 32 seed
bases and 240 seconds over all seventeen sectors, because the fix moves bodies on
shipped maps and that is not a comment. The suite is 2.7s where it was 1.3s, and
1.4s of that is the alarm sweep: it commits every guard post in the campaign and
most of the cost is the posts that run their whole budget out, which is the
honest price of asking whether a clock exists.

Two things were measured and deliberately left alone, because a decision nobody
wrote down is indistinguishable from an oversight.

**Half the campaign's guard posts cannot reach an alarm switch at all, and that
is an authoring gap rather than a bug.** With the clock in, a committed man tries
every switch and gives up; what he still cannot do is get there. Measured post by
post, 30 of 97 guards on the twelve interiors reach **no** switch in ninety
seconds, and the storeys are the shape of it: sector 1 has guards on a corridor
with no `A` on it, sector 5 has three such storeys, sector 12 four, sectors 16
and 17 two and three. The consequence is per floor rather than campaign-wide —
after the fix a body raises the alarm on 19 of 19 committed runs on sector 4 and
32 of 32 on sector 14, against 7 of 29 on sector 9, 5 of 16 on sector 12 and
**nought of 25 on sector 16**, where the risk the manual sells is simply absent.
The rule the maps would have to obey is "a storey that carries a guard has an
alarm switch he can reach", `levels/LEGEND.md` has never said it, and applying it
means adding an `A` to fifteen storeys across nine maps — which is a difficulty
change to most of the campaign rather than a defect fix, and the author's call.
Fixing only sector 16 would be this page's own "one half of a symmetric defect"
in advance.

**And the score buys about five spare lives for doing nothing but finishing.**
`EXTRA_LIFE_SCORE_STEP`'s comment says a life every 10000 points "gives the score
a mechanical meaning: better play literally buys more attempts". Priced off the
route model's own shortest walk at `PLAYER_WALK_SPEED`, a run that walks to every
door without dying banks about 42500 in time bonus, 8500 in clean bonuses and
4100 in cards and docket sheets: **55000, or five extra lives**, before a single
guard is shot at 150 a piece, a terminal hacked at 250 or a takedown scored at
250 — against three starting lives, three continues and a `MAX_LIVES` of nine.
This is the comfortable-par reading one section up arriving at the other end of
the same arithmetic: the par is 134 seconds and the walk is 5.9 to 32.2, so the
time bonus is very nearly a constant. Nothing is broken and the mode still pays
better play more; what is not established is that it costs anything to miss.

**And then it was read from the outside once more with every gate green, and the
one hazard on the legend with a placement rule and nothing measuring it turned
out to be pointed at nothing on the floor named after ducts.** Every gate was
green throughout and stayed green — `make test`, `make lint`, `make sanitize`
with the full sweep, `make coverage` at `none` functions and 458 lines with no
drift, the campaign's hazard budgets monotone in both modes (interiors 6 to 89,
climbs 20 to 43), the five climbs at 39/42/45/48/51 with their facade sources
evenly spaced and no gap over six rows, and every `SFX_` in the enum played from
somewhere. Two findings, and they are one tile apart.

- **Sector 12's two cameras watched no floor a standing player could occupy.**
  `camera_sees_player` refuses anything at or above the lens (`dy <= 0`), and
  `PLAYER_H` is `TILE_SIZE` — so in a **one-tile-high space a camera is level
  with a standing man's chest to the pixel**. Both of that floor's cameras hung
  in the service gap over a duct run, where the tile below the lens is trunking
  and therefore answers `level_is_solid`: measured, each watched **nought**
  standable cells against five to fourteen for every other camera in the game,
  and the only thing either could see at all was a **crawler** within about a
  tile — on the fitting whose own manual sheet promises *"A CAMERA has no back
  and no ears, and it looks down - crawling is no help."* The sheet was not
  merely unhelpful there, it was inverted: crawling was the only thing that made
  the lens work. Reaching either beam cost a detour of +78 and +101 steps on a
  133-step floor.
  `levels/LEGEND.md` has said since the mechanic existed to place one "where the
  beam crosses ground the player has to walk, not over a dead end: a camera
  nobody has to pass is a fitting rather than an obstacle". That is the docket
  sheet's rule and the weak wall's rule a third time — **prose for exactly as
  long as it went unmeasured** — and the second half of the same paragraph
  guessed the wrong failure mode, warning that "a mounting more than five storeys
  above the floor it is meant to watch is watching nothing" when what the
  campaign actually shipped was a mounting with no air *under* it.
  Both moved, and the direction is the finding rather than the coordinates: a
  duct is **cover** from a camera, so a lens watching the corridor beside a mouth
  is what makes the shaft worth crawling. That is the placement the original was
  reaching for, upside down. Each now watches 14 and 8 standable cells, both on a
  shortest route, and the first of them covers the ladder run out of the storey.
  The floor's budget is unchanged at 65, which is the point: it was claiming four
  points of pressure it did not deliver.
- **And the camera's own hanging rule was checked by nothing, under a comment
  saying it was.** `level_load_data` drops a camera with no slab above it, and
  the loop's comment closed "the editor says so while it is being drawn". It did
  not. `editor_symbol_hangs` answers for the wall clock `w` and the camera `I`,
  and its **only** caller sits inside `check_decorations`, which filters to the
  four decoration groups — an `I` is `ED_GROUP_FITTINGS`, so the clock was caught
  and the camera was never asked. One function answering for two things, reached
  from a place that can only ever see one of them, with the rationale claiming
  the cover the call site did not deliver. That is this page's own recurring
  defect in a new file.

Four things about the check are worth keeping, and the middle two are general.

**No gate in this tree could have seen it, `--shot` included.** The soak sweep
drew sector 12 every single run and `make coverage` counted every line of
`update_security_cameras` as executed, because a camera sweeping an empty duct
roof executes exactly as much code as one watching a corridor. A photograph does
not help either: a beam drawn over trunking and a beam drawn over floor are the
same picture unless somebody works out where the floor is. What found it was
asking the geometry a question — the same move the docket detour and the weak
wall's saving each needed, and the third hazard rule on that page to have been
settled prose.

**A loop bound that restates the rule under test is a check that agrees with the
bug.** The first draft of `check_cameras` scanned `row + 1` upward, which reads
as an optimisation and is really a second copy of `camera_watches_cell`'s own
`dy > 0` clause: with the loop bounded that clause was dead code, and deleting it
changed no answer at all. Mutating it passed. The loop walks every row now and
the geometry is the single authority, which is what makes the mutation fail.

**And the sweep needs no sampling, which is the pleasant half.** The first draft
walked the arc in two hundred steps; the dot-product test is
`cos(theta - a) >= cos(CAMERA_CONE_HALF_ANGLE)` and `gameplay_camera_angle`
covers every `a` in `[-arc, +arc]`, so "seen at some moment" is exactly
`|theta| <= arc + half`. The closed form is the loop's limit rather than an
approximation of it.

**Two clauses were measured and deliberately left as they are, and one of them is
an honest negative.** The arc is **dominated**: tile centres are 32 apart, so the
only offset it excludes that the range does not is five tiles across and one down
— `atan2(160, 32)` is 1.3734 against a limit of 1.37, a margin of 0.2% — and
reaching that cell needs the tiles between it and the lens open, which on a grid
puts nearer standable floor in shot first. So no fixture isolates it without
balancing on that margin, and one that did would fail on a hair's change to a
constant rather than on a defect. It is kept because it is the lens's own rule
(a check that drops a clause of the thing it models disagrees with the game the
moment a constant moves) and the *relationship* is pinned instead — the arc must
bite at five tiles across and not at four. And `doc_can_stand`'s generous arms —
a rung, a shaft, a plate — are **not load-bearing on the campaign as it ships**:
cut back to "the solid tile below", the check still flags nothing. They are there
for the author who points a lens down a rung column, and they get a fixture of
their own rather than an implication, because an arm nothing reaches is an arm
nobody has checked.

Both were **checked by breaking the thing they guard and watching the tests
fail** — six mutations that fail and two that deliberately do not: sector 12's
cameras put back where they were (which now names both by coordinate, because
that loop used to print `report.warnings == 0` over seventeen maps and nothing
else), the `dy > 0` rule removed, the sight-line walk removed, the range removed,
the hanging rule removed, and `doc_can_stand` narrowed to masonry floors — that
last one failing only the rung fixture, which is what says the fixture reaches
the arm. The two that pass are `doc_can_stand` widened to accept any open tile,
which can only ever make this check quieter, and the sweep arc, for the reason
written down above.

**And the same read turned up the mechanic whose every rule is about the map and
none about the player, on the floor the campaign ends on and the floor before
it.** Falling panels. Every rule this tree owns about an `F` — in
[levels/LEGEND.md](levels/LEGEND.md), in the parser, in the route model — is
about the **fallen state of the map**: the tile is air to `route_map_init`, so a
sector is certified as though every panel had already gone, which is the whole of
what "a shortcut, never a lifeline" buys. Not one of them asks what happens to
the man on the way down. Two of the five panelled floors were paying for that,
and the two costs are an order of magnitude apart.

- **Sector 9's pair was an outright death, not a heart.** A player standing on a
  panel has his feet on its *top* edge, so he is standing in the row above it and
  the fall is measured from there: 192px into the archive hall against a
  `PLAYER_FATAL_FALL_HEIGHT` of 160. Driven rather than reasoned about — walk
  right along that corridor from any starting column and it is `hp 3 -> 0`,
  arriving at **592.1px/s against a `PLAYER_FATAL_FALL_SPEED` of 560**, every
  time. A fatal fall costs everything the hearts say, so this was the run.
  The corridor it serves is a dead-end mined pocket with no pickup in it, whose
  only other exit is walking back out, and **every column of that slab drops the
  same 192px** — the hall's floor is one row for its whole width — so there was
  no better place to put it. The two tiles are masonry now, which also extends
  the pocket by the two cells the model already refused to stand in.
  **The route model knew the whole time.** `route_survivable_fall(5, 11)` is
  false, so the flood refused the hole and certified the sector by another path;
  the sheet, the terminal and the alarm switch below are all reached along row 11
  instead. *A model that refuses a move is not a warning to whoever drew it* —
  which is a new sentence for this page and the sharpest thing in the pass. Every
  gate was green: the route check passed because it routes *around* it, the
  monkey never walked that corridor, and `make coverage` counts a panel that
  kills exactly as it counts one that does not.
- **And sector 17's dropped the player through a ceiling fan** — one heart, every
  single time the mechanic was used, on the last floor of the campaign.
  `check_fans` asks five questions and **all five of them look down from the
  blades**: a hole in the floor below, a crate below, a ladder beside, a floor to
  be caught over, the standing row. A hole in the slab directly *above* a fan is
  the one arrangement none of them can see. That is this file's most reliable
  shape — one half of a symmetric pair, checked — and the fan moved two columns
  rather than away, because two columns is what clears
  `CEILING_FAN_BLADE_LENGTH` (23px either side of the tile centre, 1401..1447)
  of the falling player's box (1475..1501). One column would not have: the bands
  overlap by 4px, which is a heart.

Three things are worth keeping, and the first is the general one.

**Severity followed the existing argument rather than a new one.** A fatal drop is
an **error**, on `check_opened_walls_leave_a_way_out`'s reasoning word for word:
this is not "the floor will not play the way it reads", it is a floor that can eat
a run. Blades or a spike bed on the way down are a **warning** — they cost a
heart, the sector still finishes, and an author may well mean it. The rule itself
is `route_survivable_fall`, called rather than re-derived, because it takes two
row numbers and nothing else and a second copy of `v² = 2gh` here would be a
second thing to keep in step.

**The off-by-one is the whole finding and it gets an assertion, not a comment.**
Four open rows below a panel is 160px measured from the row above and 128px
measured from the panel's own row, so a check that forgot where the player's feet
are passes a deep fixture *and* a shallow one and still reports nothing about
sector 9. There are two fixtures at the boundary for exactly that reason, and
the mutation that sets `from_row = row` fails on the second of them.

**And `check_docs.py` did the rest of the work, which is what that script is
for.** Removing sector 9's panels made three sentences stale in one commit — the
`F` list in `levels/LEGEND.md`, the same list in a test comment, and the
`Makefile`'s own count of the floors carrying a platform — and all three failed
`make lint` before the suite ran. A balance change being a two-minute job is that
script working; so is the fact that adding the new paragraph to LEGEND.md broke
the list check a second time, because `claimed_sectors` takes the **last** list
before its anchor and the new prose had put a "sector 17" between them. The
write-up moved below the anchor. A page that explains its own history has to be
written in the order the checker reads it.

All of it was **checked by breaking the thing it guards and watching the tests
fail** — five mutations: sector 9's panels restored (which fails on the campaign
*and* prints `192 px onto row 11, past the 160 px` back at both tiles), sector
17's fan restored (naming the fan at (46,17) rather than the panel, because the
fan is the tile that has to move), the fall measured from the panel's own row,
the fan clause removed, and the spike clause removed. Re-measured afterwards:
`make coverage` at `none` functions, the hazard budgets unmoved and still
monotone in both modes, and every panel in the campaign now costs nothing —
peak arrival speeds of 318 to 470 against a fatal 560.

**And writing that check turned up the same defect inside it, which is the
footnote worth keeping.** The first draft looked for blades in the panel's *own*
column, on the unexamined assumption that a body falls down one column. A fan's
hazard band reaches `CEILING_FAN_BLADE_LENGTH` — 23px — each way from its tile
centre, and the falling box is 26 wide in a 32 tile, so a fan **one column over
still overlaps it**. Swept across every x a player can rest on a panel at, ±1
costs a heart and ±2 costs nothing: the check reads ±1 now, and the same
measurement is what says the roof's fix — the fan moved two columns rather than
one — is far enough by design rather than by luck. One column would have left a
4px overlap, which is a heart.

Deliberately **not** the ±2 `check_fans` uses for a ladder, and the difference is
the one `check_mines` is already written up for: that span is about a climber
stepping *off* into blades that may overhang his arrival tile, this one is a body
falling down a known column, and borrowing a number whose justification does not
come with it is this repository's most reliable way of being wrong. Both
directions are pinned — narrowed to one column the ±1 fixture fails, and widened
to ±2 the control fixture fails **and so does the shipped campaign**, which is
the "a rule taken too far" mutation: at that span the check forbids the roof's
own corrected layout.

**And then it was read from the outside once more with every gate green, and
what it found was one sentence corrected on one of its two copies in the very
commit that corrected it.** Every gate was green throughout and stayed green —
`make test`, `make lint`, `make sanitize` with the full sweep, `make coverage` at
`none` functions and 458 lines with no drift. Nothing in this pass cost a player
a run: three of the four are prose, and the fourth is latent. What they have in
common is that all four are this page's *own* recurring defects turned back on
the page and on the checks, which is the one direction a sweep of the code
cannot look.

- **The count of the floors carrying a platform was fixed in the `Makefile` and
  left standing in `AGENTS.md`.** Removing sector 9's cracked panels took it from
  seven floors to six. `check_docs.py` caught the `Makefile`'s copy, the fix
  landed there, and the identical sentence on the page that is loaded in full at
  the start of every session went on stating the old figure — in the same commit,
  under a write-up that quotes the `Makefile`'s wording and reads as an account
  of having fixed it. **A check that holds one copy of a sentence written down
  twice is how the other copy stops being looked at**, which is the sentence this
  file already carries about the medkit list, about the veteran row and about the
  store page, arriving this time on itself.
  It is held in both places now, and the fix needed one more thing than an
  entry: the write-up of the sector 9 pass quoted the old wording verbatim, so
  the flattened page carried both spellings and a "must contain" would have
  passed with the live claim still stale. That is the `veteran_row_detail` trap
  reached by **a page explaining its own history** — a shape the other entries in
  that script never meet, because no other page it reads narrates its own
  corrections. The quotation names the count instead of spelling it, so the phrase
  has exactly one home and presence is a real test again.
- **A note said the rocket rota was held by nothing, and it had been an editor
  error for twenty-seven commits.** The manual's `FIGHTING` sheet says `BAZOOKA:
  one rocket, even sectors only`; the paragraph filing it as an unheld sector
  list closed by saying that the day a rocket moved this was the sentence that
  would go stale first. `check_campaign_position` refuses any sector whose bazooka
  count is not `number % 2 == 0` as an **error**, and the suite holds the campaign
  to nought errors, so a rocket cannot move without failing `make test`.
  **A note reporting an absence of cover it has is the mirror of a rationale
  reporting agreement it has not, and it costs exactly the same thing**: the item
  goes on the list of known holes, the paragraph explains why leaving it is
  reasonable, and nobody looks again. This page has a standing instruction to
  write decisions down; what it did not have is that **a written-down gap needs
  re-measuring as much as a written-down figure does.**
  And the note was wrong about the direction as well as the fact. The rota and
  the sheet's *wording* are two copies of one rule, and that pair was genuinely
  unheld: changing the rota leaves the sheet describing the old one, and no gate
  could see it. The wording is derived from the maps the editor pins now, and a
  rota that stops being "even sectors only" is reported with the list it has
  become, because no wording is automatically right for that and the author has
  to pick one.
- **The moving-platform list was written down three times and stale on the copy a
  player's own sheet is justified from.** The comment above `PAGE_MOVEMENT` in
  [manual_pages.c](src/manual_pages.c) explains why the sheet does not list a
  platform beside the ladder and the lift shaft, and it named two floors where
  the maps have three — sector 14's plate arrived and the argument for the
  omission did not move with it. `levels/LEGEND.md` had it right and was also
  unheld; the only held copy was the one in the test.
  Both had to be **reworded to be checkable at all**, and that is the finding's
  general half: `SECTOR_LIST` needs the word in front of the numbers, and neither
  sentence had it — `moving platforms (`P`) on 5, 14 and 17` is a sector list that
  the script scanning for sector lists cannot see, and it would have matched the
  *trunking* list earlier in the same sentence instead. A list written in prose
  costs a rewording to hold, and that is cheaper than the list being wrong. The
  two other lists in that same LEGEND.md sentence went in beside it, because a
  paragraph whose entire argument is a count is one map edit from being wrong
  about it.
- **And a bitmask over an index with a cap that nothing tied to its width.** A
  guard remembers the alarm switches he has failed to reach in one `uint32_t`;
  `MAX_ALARM_SWITCHES` is 16, and both call sites carried an `i < 32` of their own
  that reads exactly like a decision. Raise the cap past 32 and the failure is
  **silent in the worst direction**: a switch past the mask can never be marked
  as tried and can therefore never be skipped, so a man who cannot reach it goes
  back to it for the rest of the sector — which is precisely the one-way door
  `alarm_switches_tried` was written to close, returning on the one map wide
  enough to hide it. This tree asserts `CAMPAIGN_SECTORS <= PROGRESS_MAX_TRACKED_SECTORS`
  in two separate files; the same idiom was missing here.
  The runtime bounds went with it rather than staying beside the assertion, and
  that is the point rather than tidiness: with the relationship asserted they are
  provably true, and **an arm nothing can reach is an arm nobody has checked** —
  the same reason the dead `else` came out of `level_update_moving_platforms`'s
  sign normalisation. What replaces silent degradation is a refusal to compile.

All four were **checked by breaking the thing they guard and watching the checks
fail** — seven mutations: `MAX_ALARM_SWITCHES` raised to 33 (which fails in the
compiler, naming the field and what to do about it), the platform count in
`AGENTS.md` put back to the old figure, the manual's comment restored word for
word to the state it was found in (which prints `prose says sectors [17]` against
`maps say sectors [5, 14, 17]`, the defect quoting itself), the LEGEND.md
platform list and its lift-shaft list each shortened by one sector, and a rocket
moved from sector 6 to sector 5 (which prints the rota the sheet would have to
describe, `one rocket on sectors 2, 4, 5, 8, 10, 12, 14, 16`). Re-measured
afterwards: `make coverage` at `none` functions and **458** lines, unmoved, which
is the honest reading rather than a disappointing one — one fix is a compile-time
assertion and the other three are prose and a script.

Two things were measured and deliberately left alone.

**Nothing in the campaign puts a pickup where reaching it costs a heart.** The
camera pass asked whether a beam lands on floor a standing player can occupy, and
the same question has an item-shaped half nobody had put: a `C`, `G`, `N`, `K`,
`Z`, `*` or `!` inside a fan's blade band or on a spike bed is an optional detour
priced in hearts rather than in steps. Swept over all seventeen sectors and the
four washrooms at the band the fix above measures — the blades reach a column
either side, so ±1 is a hit and ±2 is not — the answer is **nought**. Written
down rather than turned into a check, because what a hazard band is worth is a
question this page has now answered three different ways for three different
mechanics, and a fourth rule borrowing one of those numbers is how the mine's
first draft came to use the fan's.

**And the event buffer's every kind is drained.** `dispatch_events` is the one
seam where a gameplay module's feedback becomes sound and light, and a kind the
shell's switch does not name is an event that is emitted, asserted on by the
suite, and silent in the game — the `THEME_MUSIC` failure mode on the other side
of the same boundary. All seven kinds in `GameEventKind` have a case. It is a
thing worth re-reading whenever one is added rather than a check, because the
compiler already offers `-Wswitch-enum` for it and this tree does not set it.

**And then it was read from the outside once more with every gate green, and
what it found was a collection of twelve that could be made to hold thirteen.**
Every gate was green throughout and stayed green — `make test`, `make lint`,
`make sanitize` with the full sweep, `make coverage` at `none` functions and 458
lines with no drift, the changed files cross-compiled under mingw-w64 and put
through a stricter dial than this tree sets (`-Wshadow -Wformat=2 -Wcast-qual
-Wwrite-strings -Wmissing-prototypes -Wfloat-equal -Wswitch-enum
-Wstrict-prototypes -Wredundant-decls -Wundef -Wdouble-promotion -Wvla`) at
nought findings, and a monkey over all seventeen sectors and the four washrooms
at eight seed bases and sixty seconds each with four invariants the suite does
not keep: the event buffer never within a hundred of `MAX_GAME_EVENTS`, no corpse
inside masonry or off the map, no entity array past two thirds of its cap. None
of that found anything, and none of it could: **every check this tree owns asks
what happens inside one sector, and the defect below needs the sector to be
loaded twice.**

- **The docket could be made to hold more sheets than the campaign lays out, and
  the game then wrote it to the player's disk as a record.** Twelve sheets, one
  to an interior, and `campaign_docket_sheets()` derives the twelve off the maps
  so that every screen printing the count prints it against that total. Nothing
  held the count to it.
  A sector cannot hand out the same pickup twice, which is why nobody looked. A
  **continue** can. `campaign_accept_continue`'s own comment says "retrying the
  sector is always on offer" — it is unlimited, the score merely stops surviving
  past the third — and the retry goes through `load_level`, which parses the map
  again, while `CampaignState.evidence_collected` carries on from where it was.
  Measured: five retries of sector 1 give `evidence_collected` five and
  `campaign.score` five times `EVIDENCE_SCORE`, off a floor that holds one sheet.
  `game_record_run_score` then banks it through `progress_note_evidence`, whose
  ceiling is `PROGRESS_MAX_EVIDENCE` — ninety-nine, on purpose, because the slack
  is what lets a longer campaign ship without a new save format.
  Worth naming what a **veteran** run does to it, since that is the cheap route:
  `VETERAN_LIVES` is one and `VETERAN_CONTINUES` is nought, so the cycle is one
  death rather than three, and the score reset a continue past the last one
  performs does not touch the sheets.
- **The rationale is where it hid, and it is this page's most familiar shape.**
  `item_would_be_wasted` filed `ITEM_EVIDENCE` with `ITEM_CARD` and `ITEM_GUN`
  under *"paper cannot be wasted: there is no counter for it to be full of, and
  two sheets are two sheets"*. That is exactly right about the card — a wrong
  card is a second card and scores as one — and false about the docket in both
  clauses: there **is** a counter, it **has** a full, and two sheets off the same
  floor are one sheet, because the collection is laid out one to an interior
  precisely so that a missed one is always a floor the player can name. **One
  rationale covering two things and true of only one of them**, one item apart in
  the same `switch`.
  So the fix needed no new mechanism: the docket is a *set* now
  (`CampaignState.docket_sheets_held`, one bit to a sector, `_Static_assert`ed
  against `CAMPAIGN_SECTORS` the way `MAX_ALARM_SWITCHES` is against its own
  mask), `campaign_take_docket_sheet` is the only thing that raises the count so
  the two cannot come apart, and a sheet the run already holds is a **wasted
  pickup** — left lying on the floor exactly as a second grenade is, which is
  also the honest thing to show a player walking a floor they have already
  stripped.
- **And the row that prints it was the `20 / 17` defect again, three cases from
  its own fix.** `RUN_TALLY_RECORD_DOCKET` clamped its numerator to
  `PROGRESS_MAX_EVIDENCE` and printed it against `campaign_docket_sheets()`, so
  the RECORDS page and the manual's own `THE RECORD` sheet could read `13 / 12`.
  `RUN_TALLY_RECORD_SECTORS_TIMED`, three cases below it in the same `switch`,
  carries the correction in as many words — *the numerator and the denominator
  have to be asked the same question* — and was fixed for a numerator that came
  off the array where the denominator came off the campaign. **A fix that lands
  on one arm of a switch is a fix nobody applies to the arm above it.**
  The file's ninety-nine stays, deliberately, and the *reader* is what changed:
  the pickup rule stops a played run reaching it, and a file from a longer build
  or from anybody with a text editor reaches it anyway.
- **A staged screen paid a bonus its own clock cannot earn, in the one frame the
  store page is cut from.** `--screen report`, `--screen cleared` and `--screen
  reveal` each spelled out `91.0f, 74.0f, false, 1200, SECTOR_CLEAN_BONUS, 7`.
  The clock pays `SECTOR_TIME_BONUS_PER_SECOND` a second under a par of 134, so
  91 seconds pays **860** — and 1200 is exactly what **74** pays, which is the
  record printed beside it. Somebody swapped the two clocks to fix the
  impossible `best_is_new` pair this page already records, and left the bonus
  computed from the old elapsed: **one half of a symmetric defect, in the
  paragraph written to record the last one.**
  Nothing failed and nothing could. Every assertion this band has is a *width*,
  and a wrong number is exactly as wide as a right one — which is the same
  sentence as "a counter cannot tell a frame that was drawn from a frame anybody
  could read", one axis over. What it cost is that
  [tools/press_kit.sh](tools/press_kit.sh) cuts `12-report` out of the first of
  the three, so the contradiction shipped in `dist/press/` and on the shop rather
  than to a player.
  The fix is the usual one: **delete the seam.** `campaign_time_bonus_for` is the
  only place a clock is priced now — `campaign_award_sector_bonus` reads it and
  so do the three fixtures — and the fixtures themselves are `SOAK_TALLY_ELAPSED`
  / `SOAK_TALLY_BEST` / `SOAK_TALLY_DOCKET` in
  [sector_tally.h](src/sector_tally.h), beside the band they stage, where the
  suite can reach them. `test_the_sector_tally_fits_the_frame_it_is_drawn_in`
  requires the ordinary shape to be one a clear can produce and prices its bonus
  through the same function, so the two cannot come apart again; the widest-field
  shapes are exempted **by name**, because the longest clock and the fullest par
  bonus genuinely cannot co-occur and that is exactly right for measuring a
  column.
- **And the continue card showed three full hearts beside `x0`.** The only way
  anybody reaches that prompt is `gameplay_damage_player` taking hp to nought,
  and `finish_player_death` leaves it there — so the staged frame was a run with
  everything to lose and nothing left to lose it with. `--screen continue` is in
  no press list, which is the only reason this one is a footnote rather than a
  finding; it is fixed on the same principle the two above are, and verified by
  capture rather than by a counter, because nothing in this tree can reach it.
- **And the press kit described a screen that had stopped existing.**
  `15-options`'s MANIFEST caption read *"The options sheet: assists, veteran,
  records"*, and the sheet became four pages: the assists and VETERAN moved to
  DIFFICULTY, the figures to RECORDS, and `--screen settings` draws the front
  page — audio, display, and three rows that open the other three. So all three
  nouns named things the picture does not contain, on the one file in a press kit
  whose whole job is saying what the reader is looking at. Every gate was green
  and always would have been: `check_docs.py` holds sector lists and derived
  figures, and a caption is neither.
  Corrected to the frame rather than by adding a shot of DIFFICULTY, and the
  reason is written beside it: **which pictures the shop shows is the author's
  call, and an audit does not get to make it.** The line naming how is there if
  it is ever wanted.

Three things are worth keeping, and the first two are general.

**A gap that needs a sector loaded twice is invisible to everything here.** The
suite drives one sector per fixture, the soak sweep opens one sector per process,
the monkey stages one sector and runs it, and `make coverage` counts a pickup
taken five times exactly as it counts one taken once. Every liveness question
this page has learned to ask is about a body inside one visit. The retry is the
one thing in the game that puts the *same* floor in front of the player again,
and it arrived as "the run never goes back to the lobby" — a generosity — with
nobody asking what carries across it. **Whatever else is banked on the campaign
rather than on the floor wants that question put to it**: `hostiles_down` was
asked and is fine, because the lines that quote it carry a ceiling of five and a
campaign holds ninety-seven posts.

**A rationale that groups two things is a rationale that has to be true of both.**
This is the third time on this page and the first time inside a single `switch`
statement. The tell is the same every time — a sentence with a plural subject
covering cases that are not alike (*"paper cannot be wasted"*, *"the only line in
the game that states a remaining duration"*, *"the only row on either sheet that
cannot be undone"*) — and the cure is to split the case rather than to widen the
sentence.

**And the fixture that teaches the staging is the fixture that has to be real.**
The suite's shape table is a *width* fixture and is allowed to be synthetic; the
row inside it labelled "the ordinary one" is not, because `game.c` copies it and
`press_kit.sh` photographs the result. The two live one comment apart and the
comment was already right about which is which. What was missing is that the
ordinary row was only held on one of its five numbers.

All of it was **checked by breaking the thing it guards and watching the new
tests fail** — six mutations: `ITEM_EVIDENCE` made never to be a wasted pickup
again (which fails on the shipped campaign, four times over, once per retry),
`campaign_take_docket_sheet` made non-idempotent, the RECORDS row clamped back to
`PROGRESS_MAX_EVIDENCE` (which fails eighty-seven times, once per value a foreign
file can hold), the ordinary shape given back its hand-written 1200,
`campaign_award_sector_bonus` priced by a sum of its own, and the pricing's floor
at par removed. The continue card's hearts are the seventh and no test can reach
them — read off the frame instead, which is what `--shot` is for. Re-measured
afterwards: `make coverage` at `none` functions and **458** lines, unmoved, which
is the honest reading rather than a disappointing one: the two new functions
arrived fully covered, and the third fix is a caption.

Two things were measured and deliberately left alone.

**A retry is a score farm and it is bounded, which is why it stays.** The three
score-preserving continues reload the floor with its guards, its terminals, its
cards and its bonuses all back, so a player who dies on purpose can bank a
sector's worth of points three times over before the fourth retry zeroes the
score. That is the retry doing what it says — the sheets were the only thing on
it that is a *set* rather than a tally, and a set is the only kind of thing a
second visit can lie about. Capping the rest would mean a floor that pays nothing
for being cleared again, which is a worse answer than the arithmetic it prevents.

**And `run_tally_format_docket` is deliberately not clamped to the campaign.**
The end cards print `DOCKET n SHEETS` with no denominator, so the rule the row
above was fixed under does not apply to it: there is no whole for the number to
be over. Clamping it would hide a count the game should never produce rather than
report it, and with the pickup rule in place the only way to a figure past twelve
is a file this build did not write.

**And then it was read from the outside once more with every gate green, and
what it found was one armed capture answered by half a pad and one story beat
told three times.** Every gate was green throughout and stayed green — `make
test`, `make lint`, `make sanitize` with the full sweep, `make win` at nought
warnings, `make coverage` at `none` functions and 458 lines with no drift, and
clang under a stricter dial than this tree sets (`-Wshadow -Wformat=2
-Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wfloat-equal -Wswitch-enum
-Wstrict-prototypes -Wredundant-decls -Wundef -Wdouble-promotion -Wvla`) at
nought real findings across the SDL-free tree — its only reports are switches
that carry a `default`, `value == SDL_floor(value)` in `switch_number_is_index`,
which is a wholeness test and exact by construction, and `soak == SOAK_MALFORMED`,
which is the sentinel compare this page already argues for. Two findings, and
they are in the two halves of the tree no gate reaches at all: the pad, and the
plot.

- **The options sheet's capture was armed on one half of the pad, and the
  comment describing the other half was unreachable from its only caller.**
  `handle_settings_gamepad` opened `if (settings_capturing && slot_is_pad)`, so
  a button pressed while a *key* cap was armed fell straight past the capture
  into the sheet's navigation — which made `game_settings_capture_pad`'s own
  mirror guard, whose comment says in as many words that "a pad press while a
  key cap is armed cancels", **dead code**: its one caller had already asked the
  same question. So the documented mirror of the keyboard's rule did not exist.
  What happened instead is worse than nothing happening. `game_settings_adjust`
  on a binding row walks `settings_bind_slot` along the row's four caps, so two
  nudges of the d-pad carried an armed **key** capture onto a **pad** cap the
  player never chose: after that the next keyboard press cancelled with the
  "not this" noise and the next button wrote a binding nobody asked for. Up or
  down carried the same armed capture to a different action entirely.
  **And it needed no deliberate press.** `menu_stick_step` carries the identical
  half-rule — it refuses to hand a stick push into a capture armed on a pad cap,
  for the reason its own comment gives (a thumb resting on the stick would bind
  DPAD UP), and handed it straight through when the armed cap was a key. A
  resting thumb was enough.
  The fix is the one this page keeps arriving at: **delete the seam.** Both
  places ask `settings_capturing` and nothing else now — a button is answered by
  the capture, which takes it or cancels it, and the stick is never answered by
  it — so there is one description anywhere of what an armed sheet does with a
  press, and the guard that documents the cancelling is the code that performs
  it. The keyboard half has swallowed the whole keyboard while armed since it
  was written; this is the other half of it, and the asymmetry is exactly the
  shape this file names every time it finds one.
  **No gate reaches it and none can**, which is worth saying rather than
  implying: `make coverage-shell` has the whole gamepad path down as executed by
  neither gate, the suite links no SDL, and `--screen` names a state where this
  is a *transition*. It is verified by reading and by the call graph — two
  callers, both in [game_input.c](src/game_input.c) — and it sits in the 41.
- **The campaign's one turn was told three times, and the file arguing for where
  it goes says it is told once.** `intel.c` spends a paragraph placing `NOT FOR
  RANSOM. THEY TOOK HER TO OPEN A DOOR.` on sector 8, on the grounds that it "is
  told nowhere else while a sector is being played" and "is the only line that
  overturns what the player has assumed since the kerb". Both halves had come
  apart, in opposite directions.
  The kerb is where they are **told**. `render_kerb_ui` holds
  `NO WORD // NO RANSOM // THEY CAME FOR HER` on the glass from 5.40s to 8.05s
  of the prologue's second beat, before the drive has started — so there is no
  assumption left for a report seven sectors later to overturn, only a mechanism
  to supply, which is the clause that actually earns the screen. **A rationale
  that argues from a state the player was never allowed into** is this page's own
  recurring defect wearing the plot's clothes.
  And sector 7's row said it again, one sector early:
  `THEIR DEMAND WENT OUT AT 00:04. IT ASKED FOR NO MONEY.` — the first half a
  near-verbatim restatement of sector 5's report, the second half sector 8's
  turn, delivered on the reveal **into** sector 8. It was harmless for exactly
  as long as a window suppressed it. The change written up further up this page
  — the one that found ten of the sixteen rows "written and read by nobody" and
  gave them the tally to ride on — did not go back and re-read the arc as a
  whole, so a row written on the standing assumption that nobody would see it
  became one of sixteen that everybody sees. **A fix that makes something
  visible owes a second look at what it is now standing next to**, which is the
  same sentence as this page's "a placement is not covered because its twin is",
  read from the other end. Six whole-screen story beats in the campaign, and one
  of them arrived as a restatement of a line read ninety seconds earlier.
  The row says what a climb can see instead, the rationale says what sector 8
  actually adds, and the turn is told once.

**The check is the property rather than a list of forbidden phrases, and it had
to be, because every other check on that table is blind to this.** The fit test
asks whether a row is too wide; `test_the_arc_lands_on_the_sectors_that_show_a_report`
asks whether a row is *reached*; neither can see a row saying something twice.
`test_no_two_lines_of_the_arc_say_the_same_thing` requires that **no two rows
share more than one word of four characters or more** — no stop list anywhere in
it, because four characters is what separates a subject from grammar and a stop
list is a thing that needs maintaining. Measured over the table as it ships,
every legitimate pair sits at exactly one shared word (`VOSS` across 10 and 16,
`DOOR` across 8 and 16, `LIGHTS` across 13 and 15) and the pair that was wrong
sat at three, so the bar is the table's own and not a number somebody picked. A
digit run counts as a word, `00:04` and `01:00` included, which is deliberate:
two rows quoting the same minute of the night is exactly the doubling this is
for, and it is how the bad pair scored three rather than two.

Three things were measured and found to be **fine**, which is written down so
that the next reader does not spend the afternoon re-asking them.

**Every ceiling fan in the campaign clears both of the player's postures.** All
59 of them, on eleven floors: the first solid tile below a fan is two or three
rows down on every single one, and the blade band is `CEILING_FAN_HIT_HEIGHT`
tall at `fan->y - 2`, so neither a standing box nor a crawling one reaches it
from the floor beneath. A fan is a **jump-and-ladder** hazard and never a
walking one, which is what `check_fans`' five questions are all shaped around
and what `levels/LEGEND.md`'s own "46px-wide, 8px-tall band" paragraph says. The
question is worth having asked because the arithmetic is not obvious from either
end and because "duck under the blades" is a natural misreading of the mechanic:
a crawl clears a fan by two pixels *in the fan's own row*, and no shipped map
puts one there.

**And every mine can be walked off.** A mine's free answer is that only the
player's weight arms one and `MINE_TRIGGER_DELAY` at `PLAYER_WALK_SPEED` covers
60.8px against a `MINE_RADIUS` of 36 — so the placement that costs two hearts
with no answer is one with nowhere to walk *to*. Measured across all 46 mines,
the clear floor run either side, the worst has more than the 49px a body needs
to get its own centre out of the radius: **nought with nowhere to go**.
`check_mines` asks four questions and this is not one of them, and it does not
need to be while every mined floor is a corridor.
Worth knowing what the *first* reading of this was, because it was wrong in the
instructive direction: blocking every mine tile out of the route flood makes the
way out unreachable on all seven mined sectors, which reads as forced damage and
is really a model that has no move for the thing a mine is answered with. **A
hazard measured with the wrong verb measures nothing** — the spike wants
hopping, the mine wants walking, and treating the second as the first reports
seven floors cut in half that are not.

Both changes were **checked by breaking the thing they guard and watching the
new test fail** — two mutations on the half a test can reach: sector 7's row put
back word for word, which fails naming the two sectors and prints
`sectors 5 and 7 share 3 words, '00:04' among them` with both lines under it,
and a second, unrelated pair doubled up (sector 13 reworded onto sector 15's
words), which fails at four and names that pair instead — so the check is about
the property and not about the row that prompted it. The pad fix has no
mutation, and saying so is the honest form: there is no gate on that side of the
boundary to watch fail. Re-measured afterwards: `make coverage` at `none`
functions and **458** lines, unmoved, which is the honest reading rather than a
disappointing one — one fix is a string and the other is on the far side of the
SDL boundary.

**And then the staged screens were read again, and the one every gate has drawn
on every run since it existed was carrying a number that contradicts the caption
beside it.** Every gate was green throughout and stayed green —
`make test`, `make lint`, `make sanitize` with the full sweep, `make coverage`
at `none` functions and 458 lines with no drift, and a monkey widened to 64
seed bases and 240 seconds over all seventeen sectors *and* the four washrooms
(77 million steps) with twelve invariants the suite does not keep: score never
falling, every carried count inside its own cap, no collected item
un-collecting, no crate off the map or inside masonry, every projectile of all
six kinds on the map, the hack progress inside its own clock, and nothing
neutralised that was never on the floor. None of that found anything, and none
of it could. **Six findings. Four are one family — the frames `--screen`
stages — and the other two are the list and the sentence that describe them.**

- **The report between sectors labelled the stair door `02` on every floor of
  the campaign.** `draw_transition_door_background` in
  [cutscene.c](src/cutscene.c) drew the plate over the door Chuck walks through
  as the two-character literal `"02"`, while the same frame prints `TRAIL LEADS
  TO SECTOR 05` two hundred pixels to the right of it. A report is shown after
  six sectors, so five of the six carried two numbers about the same door
  disagreeing with each other — in an ordinary run, on the screen the plot is
  told on. It is `next_level` now.
  **The reason it survived is the next finding**, and it is the sharpest thing
  on this page in a while: the sweep staged this screen on sector one, and
  sector one is the *one* sector the literal was right for. A frame drawn by
  the gate on the only floor that agrees with it is a frame nobody has a reason
  to read.
- **`--screen report` ignored `--level`.** It is in `NEEDS_LEVEL`, so naming a
  sector loaded one — and then the staging spelled the completed sector and the
  next one as the literals `0` and `1`. What that cost is the table: the report
  is where six of `TRANSITION_INTEL`'s sixteen rows are given a whole screen,
  and five of the six had never been rasterized by anything, because the fit
  check measures how *wide* a row is and `INTEL_ARC_SECTORS` pins which sectors
  reach one, and neither of those puts a line on the glass. `--screen reveal` —
  the other placement of that same table — was given exactly this treatment one
  release earlier, and the comment on it closes *"a placement is not covered
  because its twin is"*. This is the twin.
  A sector that shows no report is **refused** rather than staged, off the same
  two things `try_finish_current_level` reads: is there a next sector, and does
  this one leave by its window. Every frame the switch produces is now one the
  game produces.
- **The staged clear's own fixture broke its own rule on two figures of five.**
  `SOAK_TALLY_*` in [sector_tally.h](src/sector_tally.h) opens by calling itself
  "the one clear the staged screens show" and has been corrected twice for
  printing a pair no clear can reach. Both fixes landed on the clocks. The
  docket was a flat seven and one sheet is laid out per interior, so the report
  after sector 1 read `DOCKET 07/12` where a run can be holding one, and the
  reveal at the campaign's first window boundary read the same where it can hold
  two. The body count was the same defect one column over: `HOSTILES 06` on a
  floor with two men, no dog and no `D` for a console to call anybody out of.
  Both are clamped now — the docket to `campaign_docket_sheets_by`, the tally to
  `level_authored_hostiles` — and the score is deliberately left a lump, because
  it is a sum of kills, hacks, cards, sheets and both bonuses with no
  denominator anywhere on the screen and therefore nothing to contradict.
- **And the game-over card showed three full hearts and `x3`.**
  `--screen gameover` was `game_enter_state` and nothing else. The only way
  anybody reaches that card is the continue countdown running out, so the lives
  are gone and `finish_player_death` has left the hearts at nought — which is
  written out in full four lines above, on `--screen continue`, where it was
  found and fixed. One half of a symmetric defect, in the same function, in the
  paragraph written to record the other half.

- **And the page that tells a reader which screens exist was missing two of
  them.** `docs/tooling.md` spells the list out by hand, and `reveal` and
  `resume` — the two names added in the last two releases — are not on it. Both
  went into `game_soak_screen`, into `soak.sh`'s array and into the sentence
  `--screen` prints when it turns a name down, because
  [check_lists.py](tools/check_lists.py) holds those three against each other;
  neither came here, because nothing did. That script's own write-up two
  sections up found the refusal message, called it *"a third copy of the screen
  list"*, and closed with **"a list written down twice is usually written down
  three times"** — one directory from a fourth. It is held now, and the summary
  line that has said `across two` since there were three counts the copies
  instead of naming them.

- **And [docs/story.md](docs/story.md) still quoted the intel row's old
  number.** The row after sector 11 states the one duration on that table, it
  was corrected from TEN to FOURTEEN when the night was divided seventeen ways,
  and `check_docs.py` gained an entry holding it against `NIGHT_CLOCK_*` in the
  same commit. The page that *cites* that row — in the middle of the argument
  for the report having a clock on it at all — was not touched and nothing read
  it. **A fix that lands on one copy of a sentence, and a check that lands with
  it, is how the other copies stop being looked at**: this file has that written
  on it for the medkit list, for the veteran row, for the platform count and for
  the store page, and here it is again with the fix and the check arriving
  together and still counting one. Derived now, off the same `minutes_left_at`
  the row is.

Three things are worth keeping, and the first two are general.

**A gate that stages one instance of a screen has checked the instance, not the
screen.** Four of these six are a *frame*, and the sweep drew every one of them
on every run: `make coverage` counted the draws, the fit checks passed because
nothing was too wide, and `--shot` proved the pixels were there. What none of
them could see is a number that is wrong. The door plate is the extreme case —
correct on precisely the one sector the sweep stages — and it is the same lesson
`--screen reveal`, `--screen cleared` and `--screen aftermath` each arrived at
separately: *which* instance a screen is staged on is part of naming it.

**A fixture with a stated rule owes that rule to every field in it.** "Numbers a
real clear could produce" was applied to the two figures somebody had already
been caught on and to none of the others, twice running. The general form of
this page's most-named shape: fixing one half of a symmetric defect is the most
reliable way to stop anybody looking at the other half — and a *fixture* is a
place where the halves are all in one initialiser.

**And the sweep now walks every report rather than one.** Which sectors show one
is grepped off the maps, the way the restrooms and the duct aftermath already
are, so a map that gains or loses a `Y` is walked by having been edited. It runs
at the ordinary budget rather than the beat's own length — what these add is the
line and the plate, both on the glass inside a second — and it is skipped whole
in `smoke`, because a mode that has just said it held no timed sequence must not
then walk six short ones.

All six were **checked by breaking the thing they guard and watching the checks
fail** — eleven mutations: the report's staging reverted to `0, 1`, which fails
on *both* new refusals, because a request the game cannot honour was being
accepted; the docket clamp removed and then set to nought; the hostile clamp
removed and then set to nought; `level_authored_hostiles` made to forget the
dogs; `campaign_docket_sheets_by` made to count climbs as interiors; the page's
screen list given a name the game does not answer to, and then left in the state
it was found in, which prints both missing names back; the story page's
quotation put back to TEN; and `NIGHT_CLOCK_TOTAL_MINUTES` moved, which fails
that claim along with seventeen others.

The door plate and the game-over card have no mutation, and saying so is the
honest form: both are drawings on the far side of the SDL boundary, and what
verified them is a capture — `--shot` paying for itself for the seventh time on
this page. Re-measured afterwards: `make coverage` at `none` functions and
**458** lines, unmoved.

One thing was measured and deliberately left alone.
[tools/press_kit.sh](tools/press_kit.sh) cuts `12-report` from the default
frame, which is sector one's — so the shipped still now reads `HOSTILES 02` and
`DOCKET 01/12` where it read six and seven. Both new figures are true and both
old ones were not, so the picture is honest and duller, and `--screen report
--level 9` is a truthful frame with a fuller row on it. Which pictures the shop
shows is the author's call and an audit does not get to make it; the line naming
how is here if it is ever wanted.

**And then it was read from the outside once more with every gate green, and the
one thing costing a player something was on the third screen of the game.**
Every gate was green throughout and stayed green — `make test`, `make lint`,
`make sanitize` with the full sweep, `make win` at nought warnings, `make
coverage` at `none` functions and 458 lines with no drift, `make coverage-shell`
at 41 with no drift, and clang under a stricter dial than this tree sets
(`-Wshadow -Wformat=2 -Wcast-qual -Wwrite-strings -Wmissing-prototypes
-Wfloat-equal -Wswitch-enum -Wstrict-prototypes -Wredundant-decls -Wundef
-Wdouble-promotion -Wvla`) at nought real findings across the SDL-free tree —
its only reports are switches that carry a `default` and two `double` compares
against `float` constants in `progress.c`, which are deliberate. Two findings,
and they are the same shape: **a frame nothing can read.**

- **The prologue's arrival printed `SAME SUV // KESSLER TOWEKESSLER TOWER`.**
  The red caption over the parked SUV is drawn at `target_x + 3` and runs 200px
  (25 cells) to `target_x + 203`; the building's own parapet nameplate is fixed
  by `render_tower` at `win_w - 390 + 209` and runs 104px from 619. With the SUV
  stopped at 438 those are 441..641 against 619..723 — **22px of overlap** — and
  the two baselines are five pixels apart, so the glyph rows collide over three
  cells. The last `R` of the caption is printed on the `K` of the sign and the
  two strings interleave into one doubled word.
  It is not a moment of the beat. The caption is up from t=3.0 to t=6.5 and the
  SUV finishes parking at t=3.30, so `target_x` is 437-438 for the whole window:
  **every run, every seed, on the third screen a player sees.** Sampled at 3.1,
  3.6, 4.2, 5.0, 6.0 and 6.4 the frame is identical in the respect that matters.
  Right-aligned to the target bracket now — `bracket_x + bracket_w -
  text_width(label)` — rather than nudged, because a caption belongs to the
  bracket under it and a hand-picked x would come apart again the day the SUV's
  stop moves. The kerb beat one screen later carries the same idiom against a
  `COFFEE` sign and was measured clean, which is why the fix is here and not in
  both.
- **And `--screen cleared` drew a floor finished through a door nobody had
  opened.** The card the campaign ends on is staged on the last sector, and the
  strip behind it read a blinking red **ACCESS LOCKED**. Sector 17 has no `Y`,
  so `gameplay_player_reached_exit` refuses it until the card is found and
  `try_finish_current_level` cannot run: a real `STATE_LEVEL_CLEARED` has always
  had `exit_unlocked` set. This is the elapsed-and-best pair this screen's own
  fixture was corrected for, and the full hearts beside `x0` on the two end
  cards, arriving a third time in the same staging — **a fixture with a stated
  rule owes that rule to every field in it**, and "numbers a real clear could
  produce" had been applied to the tally band and to nothing behind it.

Three things are worth keeping, and the first two are the general ones.

**A caption and a piece of scenery are two renderers' literals, and nothing in
this tree compares two renderers.** Every check the prologue has asks whether a
*table of words* fits a plate; there is no plate here and no table — a caption
is a table of words with one row, drawn at a hand-picked x, on the far side of
the SDL boundary. The sweep drew this frame every single run and `make coverage`
counted both `draw_text` calls as executed, because a glyph printed on top of
another glyph executes exactly as much code as one printed beside it. That is
the same sentence this page has written seven times about the HUD and the
options sheet, arriving in the one place a player meets before the HUD exists.

**A beat is not photographed because a screen is.** `--shot --screen opening`
at the default lead-in is the first frame, where nothing is on the glass;
[tools/press_kit.sh](tools/press_kit.sh) photographs the *kerb* and the drive
and not the arrival, so the one switch in this binary that can see a defect like
this had never been pointed at the 3.5 seconds that carry it. Which is the
`--screen reveal` lesson — *a placement is not covered because its twin is* —
with the axis swapped from where to **when**.

**And neither fix has a mutation, which is the honest form rather than a
shortfall.** Both are drawings on the SDL side: `make test` links no SDL,
`--screen` names a state and cannot name a moment inside one, and no counter can
tell a frame that was drawn from a frame anybody could read. What verified them
is a capture in each direction — the defect photographed at six times across the
beat before the change and at three after, and the strip read as `ACCESS LOCKED`
before and `ACCESS GRANTED` after. `--shot` paying for itself for the eighth
time on this page. Re-measured afterwards: `make coverage` at `none` functions
and **458** lines and `make coverage-shell` at **41**, both unmoved, which is
the honest reading rather than a disappointing one — both fixes are on the far
side of the boundary either figure can see.

Four things were measured and deliberately left alone, and the first is the
largest open question on this page.

**A third of the campaign's guard posts cannot raise the alarm.** Driven post by
post — every guard on the twelve interiors sent to every switch on his floor in
turn, ninety seconds each — **31 of 97 reach none of them**, and the shape is
per floor rather than campaign-wide: sectors 6, 8 and 14 are at 7/7, 7/7 and
12/12, against 1/2 on sector 1, 3/7 on sector 12, 6/14 on sector 16 and 5/15 on
sector 17. The risk the whole quiet route is played against — a witness running
for a wall switch — is therefore absent on the floors with the most men on them,
and `GUARD_BODY_ALARM_CHANCE` hands the roll to one runner at a time, so a post
that cannot arrive also mutes the floor while it fails. Nothing is broken by any
written rule: `levels/LEGEND.md` has never said that a storey carrying a guard
needs an `A` he can reach. Making it one means adding a switch to roughly
fifteen storeys across nine maps, which is a difficulty change to most of the
campaign rather than a defect fix, and fixing only sector 16 would be this
page's own "one half of a symmetric defect" committed in advance.

**The ending's `NO CLEAR SHOT // ELLEN IN LINE OF FIRE` is printed across the
moon.** The caption sits at x=34, y=78 and the outro's moon is a 21px disc
centred on `win_w * 0.205`, y=86 — so three cells of the line, the second `/`
and the `E` of `ELLEN`, are laid over it. The text is drawn *after* the sky and
wins the pixels (verified by sampling the frame: red glyph strokes are present
inside the disc), so the line is legible and this is a smudge rather than the
collision above. Left because moving either one is a composition decision about
the campaign's last beat, and an audit does not get to make it.

**Five tuning numbers in `gameplay_climb.c` are literals beside named
constants** — the thrown object's `-330.0f`/`150.0f` rise clamps, the bird's
`±48.0f` and its `0.28f` descent factor. Both upward clamps are compiled and
never run, because a facade hazard always spawns above the player, which is why
they have gone unnamed: the class this page named the throw branches for, in the
one file where half of it is unreachable.

**And the sidearm ammunition on every interior is one `G`.** Rounds needed if a
floor is cleared by shooting run from 4 on sector 1 to 38 on sector 17 against
an opening clip of six; the floors are playable because ammunition respawns and
the dead drop it, which the manual says in as many words. Worth knowing rather
than changing: the supply is a *rate* and not a count, so the shooting route's
real cost is the walk back to the box, and nothing in the tree states that as a
rule.

**And then it was read from the outside once more with every gate green, and
what it found was the plot's own clock, right on one of the four sectors it is
spoken on.** Every gate was green throughout and stayed green — `make test`,
`make lint`, `make sanitize` with the full sweep, `make win` at nought warnings,
`make coverage` at `none` functions and 458 lines with no drift, and the changed
SDL-free files under a stricter dial than this tree sets (`-Wshadow -Wformat=2
-Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wfloat-equal -Wswitch-enum
-Wstrict-prototypes -Wredundant-decls -Wundef -Wdouble-promotion -Wvla`) at
nought diagnostics. Three findings. All three are the same shape and this page
has a name for it: a fix that landed on one of two symmetric halves, with the
paragraph explaining the fix sitting directly over the half nobody touched.

- **`NINE MINUTES AND THIS ROOF IS SOMEBODY ELSE'S` is sayable on sectors 14,
  15, 16 and 17, where the dial gives nine, seven, five and three.** `CrewLine`
  had a floor and a ceiling for the *tally* (`after_down`, `until_down`) and a
  floor alone for the *sector*, so every gated line on the net ran from its gate
  to the end of the campaign. The one line in the game that states a remaining
  duration was therefore correct on one of the four sectors it could be heard
  on, and on the roof itself a man announced nine minutes standing next to
  another saying `THERE IS NO UPSTAIRS LEFT. THIS IS UPSTAIRS.`
  **The comment above the field had already written the argument out.** It reads
  "A line that names a number is true over a *window*, not from a moment onward,
  and `until_down` is that window's far edge" — a sentence about windows,
  written directly above the axis that had none, in the commit that gave the
  other axis its ceiling. It even opens `**A floor with no ceiling only fixed
  half of that.**` That is this page's most-repeated shape arriving *inside the
  paragraph that names it*.
  **And `check_docs.py` could not see it**, which is the part worth keeping.
  `crew_duration_lines` was written for precisely this line and holds a pair —
  the number it spells against the dial at the sector in the gate — and its own
  regex stops at the first number after the string, because there was no second
  number in the table to read. A guard on a floor is a guard on a floor: the
  check validated one of the four sectors and reported a pass. It reads both
  numbers now and walks the whole window, so a ceiling that closes too late
  fails with the readings printed back.
- **The net reported three stages of the heist at once.** `BRUNO IS ON THE SIXTH
  LOCK. ONE MORE AND WE LOAD` is gated from 8; the seventh lock is open by 10,
  which is where `VAULT IS DRY. LOAD IT AND GET IT ON THE ROOF` is gated and
  what it says. With no ceiling on either, sectors 10-16 could hear the sixth
  lock still being worked *and* the vault already empty, and on 17 both stood
  beside `CASES ARE ON THE PAD. WE ARE WAITING ON THE BIRD`. The locks run 8-9,
  the vault 10-16 and the pad at 17 now, and they hand over rather than pile up.
  **The third of those was found by the check written for the first two**, which
  is the whole argument for asking the property instead of fixing the rows:
  `VAULT IS DRY` outliving its own completion was not on the list that prompted
  any of this. `test_the_net_reports_one_stage_of_the_job_at_a_time` finds the
  three by what they claim rather than by index and requires each to have been
  found at all, so rewording one fails loudly instead of quietly being checked
  no longer.
  Ceilings are deliberately still only ever set on a line that **counts** or
  reports a **stage of the job**. An opinion does not go stale and an *event*
  does not either — `WHO BROKE THE GLASS ON THE STAIR` and `HE CAME UP THROUGH
  THE DUCTS` keep their open tops, because something that happened goes on
  having happened.
- **A run parked at the lives cap got a free life back after every death.**
  `campaign_check_extra_life` moves the threshold whether or not there was a
  life to give, under a comment saying that is what stops a run at `MAX_LIVES`
  banking every milestone it passes — and every call site is `while
  (campaign_check_extra_life(...))`, so the drain stops on the first `false` and
  the threshold advanced exactly one step per call chain. Measured: at nine
  lives with a score four milestones clear, the next death dropped the count to
  eight and the next ten points handed a life straight back, twice over.
  **The comment described the function and the bug was in the seam between the
  function and its loop** — the rule was right about itself and wrong about how
  it is spent, which is the `level_update_moving_platforms` lesson with the
  object swapped: not a function whose only caller is elsewhere, but a
  *guarantee* whose only enforcement was elsewhere. The catch-up now happens
  inside the rule, so the cap costs the milestones it swallows however a caller
  loops. The existing test could not see it and it is worth saying why: its cap
  block lands the score exactly *on* a threshold, so there was never a spare
  milestone to bank, and the claim being made is about a run that is several
  ahead.
- **And two staged frames scored nought over the bodies they were paid for.**
  `--screen aftermath` sets `dead = true` on two men and a dog by hand, which
  skips the path that tallies and scores them, so the strip above them read
  `SCORE 0000000` on a floor with two corpses and a blown wall on it. That
  staging is the one that actually **ships**: `02-alarm` and `13-duct` in
  [tools/press_kit.sh](tools/press_kit.sh) are both this screen, so the
  impossible pair went into `dist/press/` and onto the shop rather than to a
  player. It reads `SCORE 0000375` now — `2 * ENEMY_SCORE + DOG_SCORE` — through
  `gameplay_record_neutralized`, so the crew situation the net reads agrees with
  the floor too. `--screen cleared` and an interior `--screen reveal` had the
  same field at nought under a band quoting a docket and paying two bonuses;
  `sector_tally_soak_score` is what both stage it from, derived off the same
  fixture the band is drawn from.
  This is the **fourth** field of that one fixture to need this — after the
  elapsed-and-best pair, the docket over sector one and the hostile count on a
  two-man floor — and the first one found on the aftermath's. **A fixture with a
  stated rule owes that rule to every field in it**, and three of those four
  corrections were written into the paragraph directly above the field that was
  still wrong.

Three things are worth keeping, and the first two are general.

**A gate with a floor and no ceiling is half a gate, and the half that is
missing is invisible from the table.** Reading `{"...", 14, 0, 0}` tells you
nothing is wrong; what is wrong is the column that is not there. The fields are
written floor-then-ceiling in pairs now, for both axes, because the whole of
this defect was that one axis had a ceiling and the other did not and four
numbers in a row could not show you which.

**A check written for one line holds the line and not the claim.** `check_docs.py`
was pointed at this exact sentence, derives the dial correctly, and passed
throughout — because the question it asks is "does the number match the gate"
and the question that matters is "does the number match every sector it is
spoken on". This is the page's own recurring defect, a check reporting coverage
it does not have, on the check most recently written to end it.

**And nothing in this tree could have heard any of it.** The soak sweep draws
these sectors every run and `make coverage` counts a stale line exactly as it
counts a live one; the fit checks measure whether a line is too *wide*;
`--shot` is no use, because a photograph of a wrong sentence and a photograph of
a right one are the same photograph. What found it was asking, of every line and
every sector, *which sectors can hear this* — and then reading the answers.

All of it was **checked by breaking the thing it guards and watching the checks
fail** — nine mutations: the clock line's ceiling removed (which prints the
defect's own figures back, `sector 15 reads 00:53 and has 'SEVEN' left`), its
gate moved with the words kept, a wider-but-still-wrong window, the sixth-lock
ceiling removed, the vault ceiling removed, the stages put out of order, a stage
line reworded so the check loses its subject, the extra-life catch-up removed,
and the staged score set back to nought and then with its docket term dropped.
Re-measured afterwards: `make coverage` at `none` functions and **458** lines,
unmoved — every line of the four fixes is reached by the suite, which is the
honest reason a figure this page keeps asking for did not budge.

Two things were measured and deliberately left alone.

**A hand-edited `settings.cfg` can produce a control scheme the sheet refuses.**
`keybind_set` swaps a key off whoever held it and refuses any change that would
leave an action with nothing, so the options sheet cannot bind one key to two
actions or empty a row. `settings_parse` enforces neither: `bind_left A -` on
nine lines gives nine actions one key, and it round-trips stably. `NONE` is
rejected and the defaults survive, so a truncated file is safe; what is not held
is a file somebody has had an opinion about. Left because it is recoverable —
every row can be rebound and `RESET CONTROLS` is one press — and because
tightening the parser means deciding what to do with a file the player wrote on
purpose, which is a decision rather than a fix.

**And the ammunition, the sounds and the music are all reachable.** Every `SFX_`
in the enum is played from somewhere outside `audio.c`, every `MusicTrack` is
selected from somewhere, the synthesis `switch` carries no `default` so a new
sound cannot be silently silent, and `MUSIC_PLANS` and `THEME_ART` are both held
by `check_lists.py` against their enums. The weapon ring skips whatever is out
of ammo in both directions, so no bumper can put an empty weapon in the hand.
Asked because `audio.c` and the editor are the two least-audited files in the
tree by this page's own count; the answer is that nothing is missing.

**And then a player said the building takes a long time to appear, and both
halves of why were the same shape: an animation nobody could see.** Every gate
was green throughout and stayed green — `make test`, `make lint`, `make
sanitize` with the full sweep, `make coverage` at `none` functions and 458 lines
with no drift. Nothing here is a crash and nothing costs a run. What it costs is
a third of the campaign's opening beat, every time one of those sectors is
entered.

- **A climb drew nothing at all until the reveal had finished.**
  `render_world` dispatches on the mode, and the interior branch draws its
  structural tile layer *inside* the reveal — each tile as `tiles_visible`
  reaches it — and returns before the lighting, the props and the cast.
  `render_facade_world` opened `render_background(...); if (!reveal.done)
  return;`, so a wall was its flat backdrop for the whole animation and a
  finished picture on the frame after: no windows, no ledges, no hazard mounts,
  no open window to start from. Two branches of one function, one question, two
  answers — and the branch that had the answer is the one written first.
- **And the front was sweeping the axis the camera cannot hold, which is why
  fixing the first half alone still showed a blank wall for 2.3 seconds.**
  `level_reveal_step` walks row-major, and row-major is right for what it was
  written against: an interior is 34 to 60 tiles wide and 17 to 23 tall against
  a 25x16 view, so a front moving down covers the screen for nearly the whole
  walk. A climb is 25 wide and 41 to 53 tall, the camera sits at a spawn on the
  bottom row, and the first two thirds of the walk are therefore above the top
  of the frame. Measured — the longest stretch during which nothing inside the
  viewport changes, at the held interval the ten window transitions use —
  sectors 3, 7, 11, 13 and 15 came in at **2.30, 2.40, 2.49, 2.54 and 2.65
  seconds of a 3.80-second beat**. It sweeps along the map's *shorter* axis now,
  which is the same rule stated as the reason rather than as the case: the
  interiors keep the row-major walk they have always had, byte for byte, and the
  five climbs go to **0.11 to 0.14s**, from the worst reveals in the campaign to
  the best.

Four things are worth keeping, and the second is the general one.

**No gate in this tree could have seen either half.** The soak sweep drew
`--screen reveal` on every run and `make coverage` counted every line of both
renderers as executed, because a wall that draws nothing executes exactly as
much code as one that draws everything — for the ninth time on this page, a
counter cannot tell a frame that was drawn from a frame anybody could read. Nor
could `--shot` on its own: a still of a blank wall is a still of a wall the
reveal has not reached yet, and the two are the same photograph. What found it
was a *player*, and what proved it was six stills across one beat.

**The direction was keyed on the shape and not on the mode**, though
`LEVEL_MODE_FACADE` agrees with it on all seventeen maps today. The mode is a
proxy; the tall narrow map is the thing the rule is about, and this page already
has `THEME_CORDON` written up for keying a per-climb value on a per-backdrop
field that happened to agree — four backdrops against five climbs, and the
highest wall in the game answering as the second one. A value that is one per
*shape* is keyed on the shape.

**The check is comparative, because there is no number here anybody could
derive.** How much of a wipe a camera catches is a fact about the map's
proportions against the view's, so
`test_a_reveal_is_watched_from_where_the_player_stands` measures every shipped
map **both ways** and requires the order it uses to be the better of the two. It
fails from either side — down the rows on the five climbs, across the columns on
the twelve interiors — and it needs no list of which map is which. It also
requires the two readings to *differ*, for the reason
`test_a_stretched_reveal_lasts_the_same_on_every_map` next door requires its
unstretched spread to be wide: a comparative check whose two sides agree is a
check that cannot fail, and a viewport grown to hold a whole map would quietly
make it one.

**And the two fixes are independent, so each was broken on its own.** The gate
alone leaves 2.3s of blank wall; the axis alone leaves a wall that draws nothing
until it is finished. Only the first is something the suite can reach — `make
test` links no SDL — so the renderer half is verified by capture, which is the
honest form rather than a shortfall.

All of it was **checked by breaking the thing it guards and watching it fail** —
four mutations: the sweep reverted to row-major everywhere (which fails on
sectors 3, 7, 11, 13 and 15, printing the defect's own figures back), forced to
column-major everywhere (which fails on all twelve interiors), keyed on the
*longer* axis (all seventeen), and the renderer's gate put back at the top of
`render_facade_world`, photographed at 1.2s of sector 3's reveal against the
fixed frame. The finished picture is byte-identical before and after on both
modes, and so is every interior frame mid-reveal, which is what says this
changed the animation and nothing else. Re-measured afterwards: `make coverage`
at `none` functions and **458** lines, unmoved — the new branch is reached by
the test driving both axes.

One thing was measured and deliberately left alone.

**The interiors have up to a second of the same silence, at the front of the
beat.** The reveal starts at row 0 and the camera's top row is wherever the
spawn puts it, so a floor whose `S` sits low opens with the front walking rows
nobody can see: sector 16 at **1.06s** of the 3.80, sectors 5, 10 and 14 at
0.91-0.93, and sector 1 at 0.08 because its spawn is at the top of the frame.
It is the same defect the climbs had and a quarter of the size, and the two
answers to it are both worse than leaving it. Starting the front at the visible
edge puts the wipe's origin in the middle of the screen; sweeping from the edge
nearest the camera flips every interior's reveal to run bottom-up, which is a
change to how twelve sectors open in exchange for moving the silence from the
start of the beat to the end of it — and trailing silence is the better half of
that trade only if a finished picture under a line being read is better than a
blank one, which is a composition decision rather than a defect. What the
comparative check does hold is that row-major is the *better* of the two axes
for every one of them, by 0.9 to 1.4 seconds.

**And then a player said the background of a climb was badly animated, and the
one layer that answers for the whole city outside was keyed to the wrong thing
twice over.** Every gate was green throughout and stayed green — `make test`,
`make lint`, `make sanitize` with the full sweep, `make coverage` at `none`
functions. Nothing here is a crash and nothing costs a run. What it costs is
every second of the five climbs, which is a third of the campaign, and the
report came from somebody playing rather than from anything in this repository.
Two findings, and they are the same eight lines of `facade_skyline`.

- **The city repainted its lights about thirty times a second, and only while
  the camera moved.** Which windows are lit was
  `((int)(wx + wy) + i) % 3 == 0` — a *screen* position, and the layer's own
  parallax moves it. Measured over a climb at `FACADE_CLIMB_SPEED`, that is
  **1899 to 2016 window state changes a second** against some sixty lit windows
  on screen: the pattern steps every fourth frame and two thirds of the
  skyline changes state when it does. Measured in pixels off the frame instead,
  by capturing a twelve-frame burst under both builds and differencing the sky
  either side of the wall, **720 pixels of visible skyline switch at once,
  three times in a fifth of a second**, and nought of them do after the fix.
  It also drew the lights in diagonal stripes, because the bay and floor pitches
  are both 1 modulo 3.
  **Three backdrops in this same file already key their city lights to the
  repeat rather than to the screen** — `backdrop_lobby`, `backdrop_office` and
  `backdrop_roof` — and one of them says why in as many words: *"hashing the
  screen position made the whole skyline switch its lights while Chuck
  walked"*. The fourth is the one drawn behind the only sector type with
  nothing else on the glass. `art_repeat`'s own comment is the
  general form of the rule and predates all four. **A rule written down three
  times is a rule nobody looks for a fourth time**, which is this page's oldest
  shape with the copies inside one file.
- **And the layer it was keyed to ran the wrong way and jumped.** The offset
  was `fmodf(s->cam_y * 0.14f, 110.0f)`, and both halves of that are wrong.
  `+ cam_y` moves the skyline *up* the frame as the camera rises, while the wall
  drawn in front of it moves down — a backdrop sliding the wrong way past the
  thing it is meant to be behind, when what height does to a distant city is put
  it further **down** the frame. And the wrap is a snap: measured, the entire
  skyline jumps **109.74px** partway up three of the five walls, and the HIGH
  climb's cloud deck **59.81px**, which is the whole city moving half a storey
  sideways in one frame. `facade_news_helicopter`, sixty lines away, already had
  the argument written above it: wrapping the height of one recognisable object
  is a visible jump. A skyline is one object.
  The storm's low cloud had the sign right and the same latent wrap, three
  hundred pixels away from biting on the tallest wall in the game. All three
  layers read through one rule now, which moves that band down by the width of
  its own old offset — 43px on the storm wall and 58px on the sleet one, so it
  starts the climb inside the frame instead of clipped by the top of it. That is
  a shipped picture changed with no bug behind it, which is worth saying rather
  than leaving to be discovered: the alternative is two spellings of one
  sentence in one file, which is how the other two came to disagree.

Three things are worth keeping, and the first two are general.

**No gate in this tree could have seen either of them, `--shot` included.** The
soak sweep draws all five walls every single run, so `make coverage` counts a
boiling skyline exactly as it counts a still one — a strobe executes the same
lines as a picture. And a photograph of a blinking skyline and a photograph of a
steady one are *the same photograph*, which is the second defect on this page
that a still frame is no use against; the first was the patrol that stopped
being one, and both were found by asking a question with a **duration** in it.
What actually found this one was a player, one release after a player found the
reveal on the same five walls.

**The one axis nothing had ever exercised out here was the only one that
moves.** A facade map is exactly one viewport wide — 25 tiles against `VIEW_W`
— so `camera_axis_target` pins `cam_x` to nought on every climb, and every
parallax helper in the file takes `cam_x`. So the horizontal term in this layer
is inert on all five shipped walls and the vertical term, which is the entire
motion a climber sees, was hand-rolled at each of the three places that wanted
it. **A helper that only serves the axis that never moves is not coverage of the
layer**, and the tell was there to be read: three call sites, three spellings,
no shared function.

**And half of it left the renderer, which is the only reason half of it has a
test.** `level_backdrop_sink` is in [level.c](src/level.c) beside
`level_theme_cordon` and for the same reason — a value a picture depends on that
nothing on the SDL side can be asked about — and what it is asked is a pair of
properties rather than a number, because the distance and the rate are taste and
neither was the defect: it only ever runs one way, and one pixel of camera never
moves a layer by more than its own factor.
`test_a_backdrop_layer_sinks_as_the_climb_rises` walks every shipped map at a
pixel a step, finer than any frame of a climb, so a jump cannot hide between two
samples. The keying stays a hash in a renderer and stays untestable, which is
worth saying plainly rather than implying otherwise.

Both were **checked by breaking the thing they guard and watching it fail** —
three mutations on the tested half and two measurements on the half no test can
reach. `level_backdrop_sink` reverted to the shipped wrap fails all three of its
properties; reverted to the inverted sign without the wrap fails the direction;
and cut to half its rate fails the whole-run figure, which is what stops the
test from passing a layer that has quietly stopped moving. For the keying, the
arithmetic was re-run with the parallax **fixed** and the shipped hash left in
place — still 1530 to 1900 changes a second, so the two halves are independent
and each needed its own fix — and the pixel burst above is the same statement
off the frame. Re-measured afterwards: `make coverage` at `none` functions and
**458** lines, unmoved, which is the honest reading rather than a disappointing
one — the new function arrived fully covered by the test that came with it, and
the other half of the fix is on the far side of the SDL boundary.

Two things were measured and deliberately left alone.

**The moon climb's ground haze is pinned to the frame.** Three bands at
`win_h - 150 + band * 34` and a moon at a fixed screen position, on the one wall
whose towers are drawn as silhouettes. The moon is right and must not move: it
is far enough away that a forty-storey climb is nothing to it. The haze is
described in its own comment as lying "on the air near the ground", which is a
distance, and it does not move at all — so it is not animated wrongly, it is
simply the one layer out there with no opinion. Giving it a sink changes the
composition of a beat nobody has complained about, and that is an author's call
rather than a sweep's.

**And a staged climb is always the foot of the wall.** `--screen aftermath
--level N` on a facade stages the thrown object, the bird and the gust at the
player's spawn, which is the bottom row, so every frame that switch has ever
produced of a climb has `cam_y` at its maximum — where the old wrap happened to
be nearly harmless and where the skyline sits highest. The whole of this pass
was measured by moving the camera up the wall with a throwaway patch, because
nothing in the shipped switches can. A `--page` on that screen that staged the
same world a few storeys up would have cost nothing and is the obvious next
thing to want; it is not this pass.

**And then a player said the news helicopter flies backwards, and it had, for
the whole of every climb it appears on.** `facade_news_helicopter` traverses
left to right — `x` runs from -90 to `win_w + 90` over the 22.2s its drift
implies — and the hull under it was laid out nose-left: the cabin at `x - 7`,
the tail boom at `x + 7` reaching to `x + 20` and the fin at the far end of
that, with the lit cabin door on the leading edge behind the nose. So the one
aircraft the cordon lets near the tower crossed the sky tail-first, on three of
the five walls. The offsets are mirrored, and the coupling is written at the
traverse rather than left to be inferred: a nose and a tail boom are the only
thing on this layer that says which way an aircraft is pointing, so reversing
the drift means mirroring them with it.

Three things are worth keeping, and the second is the one this page has not
written down before.

**Two drawings of one aircraft, one of them right.** `draw_helicopter` in
[cutscene.c](src/cutscene.c) is nose-left too, and the outro flies it in from
`win_w + 155` toward 675 — leftwards, nose-first, correct since it was written.
Same shape, same convention, one direction of travel each, and only the backdrop
copy disagreed with its own motion. That is this file's most reliable smell,
except that the two answers are a *picture* rather than a sentence, which is why
none of the checks that hold two copies of a number could have looked at it.

**A still frame is enough to see this one, and that is the point: nothing had
ever been pointed at the strip it happens in.** A facade map is exactly one
viewport wide, so `facade_shell` covers everything but about fifty pixels of sky
at either side, and the craft is inside one of those strips for roughly 1.3 of
every 22.2 seconds. The soak sweep drew all five walls every run and `make
coverage` counted every line of the function; `--shot` *could* have caught it —
unlike the strobing skyline and the stopped patrol, both of which photograph the
same as the working version — and did not, because no capture in this repository
has ever been cropped to the sky beside the wall. **A defect a photograph can
show is only found by somebody looking at that part of the photograph**, which
is the fit checks' own lesson (they measure a line's width and cannot see its
order) arriving in the backdrop.

**And it is the third player report on this layer in three releases** — the
reveal that drew nothing on a climb, the skyline that repainted its lights
thirty times a second, and now this. What they have in common is that the facade
backdrop is the one part of the game with no simulation behind it and no words
on it, so every gate this tree owns is blind to all three by construction. What
found each of them was somebody watching a climb.

The fix has no mutation and saying so is the honest form: `level_art.c` is on
the far side of the SDL boundary, `make test` links no SDL, and nothing here can
be asked which way a rectangle points. What verified it is a capture on both
call sites — the night wall and the storm wall, at 18.5s and 19.5s of one cycle,
where the boom and fin now trail the cabin and the craft moves the way it is
facing.

**And then a player said every guard dies to three free jumps on the head, and
the guard turned out to have been answering the whole time — at the one place
the boot cannot come from.** Every gate was green throughout and stayed green.
Nothing in this pass is a crash. What it is, is a mechanic that was wired up,
telegraphed, drawn, and aimed at empty air, on the answer a player reaches for
the moment a floor gets busy.

- **A stomped guard fired two rounds a kill, 43px under the player's boots.** A
  stomp calls `damage_enemy`, which calls `gameplay_provoke_enemy`, which starts
  the aim telegraph — so he has *responded* to every stomp since the mechanic
  existed. That function wrote `aim_vdir = 0` flat, and `fire_enemy_bullet`
  clamps a horizontal round into the guard's own
  `ENEMY_MUZZLE_MIN_Y_FACTOR`..`MAX` band whatever the aim said. Measured on a
  flat floor with one guard: he fired at 0.73s and 1.33s of the 1.5 seconds a
  three-stomp kill takes, both times at chest height, while the player sat two
  tiles over his helmet — **and the kill cost nought hearts.**
  **A response aimed at the one place the attack cannot come from is worse than
  no response**, because the telegraph is drawn on every one of those frames and
  says otherwise. That is this file's own oldest shape with the object swapped
  one more time: not a check reporting coverage it does not have, but a
  *defence* reporting a defence it does not have. And the machinery was all
  there — `enemy_shot_solution` has fired up and down a guard's own column since
  the vertical lane existed, `fire_enemy_bullet` has had the branch for it, and
  the one caller that could not see the player was the one caller that does not
  need to look.
- **What decides "roughly level" is the muzzle clamp, and the first draft of the
  fix borrowed a distance instead — so it changed nothing at all.** Reusing
  `enemy_shot_solution`'s own `TILE_SIZE * 1.2f` preference band reads as the
  obvious answer and puts a man standing on the guard's *head* inside it: the
  band is 38px and a body is 32 tall, so the two centres sit 32 apart and
  measure as level. Rebuilt on the clamp — is there any part of the player's box
  inside the strip a horizontal round can occupy — it fires up the column, and
  that is the version that asks the thing which actually makes the shot miss.
  Both drafts are mutations below and both fail on the same two assertions,
  which is what says the second one is doing the work rather than agreeing with
  the first.
- **And the fix is one function rather than one line, because there were two
  places a man begins an aim and only one of them chose an axis.**
  `gameplay_ai_aim_at_player` is the axis and the aim together — the combat pass
  goes through it off a firing solution, `gameplay_provoke_enemy` off having
  been hit — so the target arithmetic and the telegraph exist once. It asks
  nothing about line of sight, and that is the difference between the callers
  rather than an omission: being shot, knifed, crated or landed on has already
  answered that. The combat path is byte-equivalent, verified by the suite
  staying green over a refactor that moved every line of it.

Three things are worth keeping, and the third is the one this page has not
written down before.

**No gate in this tree could have seen it, `--shot` included.** The soak sweep
drew these floors every single run and `make coverage` counted every line of the
aim, the fire and the bullet update as executed — a round that misses executes
exactly as much code as one that lands. A photograph is no use either: the
telegraph, the muzzle flash and the round in the air are all *drawn*, so a still
of a guard defending himself and a still of a guard missing by two tiles are the
same still. What found it was **a player**, which is now the fourth report in
four releases that no gate here could have produced — the reveal that drew
nothing on a climb, the strobing skyline, the helicopter flying backwards, and
this. The first three were pictures with no simulation behind them; this one is a
simulation whose picture was right the whole time.

**A number borrowed from a rule whose justification does not come with it is
this repository's most reliable way of being wrong**, and it has now happened
often enough to be worth stating as the rule rather than as a story. The mine's
first draft took the fan's ±2 columns. The falling panel's first draft took the
ladder's ±2. This one took `enemy_shot_solution`'s 1.2 tiles. Every time, the
borrowed number was *correct in its own place* and measured nothing where it
landed, and every time the honest bound came from asking what makes the thing
fail — here, the clamp in the fire.

**And the balance is a measurement rather than a feel.** Three behaviours over
64 seeds on the fixture: stomping on the beat in his column costs **1.00 hearts
of three** and kills in 1.95s; one stomp and away costs 0.41 and does not kill;
breaking off out of the column between bounces costs **2.08** and wiped out 5 of
64 runs, because a player beside a guard is level with him and gets the
horizontal round this fix left alone. So the cheapest way to stomp a man to
death is still to commit to it, and the engagement is capped at one heart by
`PLAYER_HIT_INVULN` however many bounces it takes — the mechanic charges for the
kill instead of punishing hesitation, which is the shape worth having. The first
stomp stays free, because he is not provoked until it lands.

**Four sentences called the stomp the *free* kill and all four are corrected**,
which is this file's "a fix that lands on one copy of a sentence" rule applied
before it could bite rather than afterwards:
[game_config.h](src/game_config.h)'s note on the heavy,
[docs/gameplay.md](docs/gameplay.md) twice, and
[levels/LEGEND.md](levels/LEGEND.md)'s `Q` entry. The correction leaves the
heavy **sharper** rather than blunter, which is the reverse of what taking the
edge off a mechanic usually does to the thing built on top of it: an ordinary
guard now trades a heart for the kill, and a heavy charges the same heart and
gives nothing back.

**And the player is told, because a rule learned only by losing a heart to it is
a rule the game never taught** — which is [manual.c](src/manual.c)'s own
argument for drawing the heavy by silhouette, one sheet over. The FIGHTING sheet
says he fires straight up. Worth knowing what happened when it was added as a
fourth line: `manual_page_lines_fit` refused it, exactly as that sheet's own
comment warns it will — *"FIGHTING is the longest sheet in the book and its last
row once put ink three pixels under the column, silently"* — so the new fact had
to pay for itself inside the existing three lines, and the wording is what the
check chose rather than what read best. That is the fit check doing the only
thing it exists to do.

All of it was **checked by breaking the thing it guards and watching the new
test fail** — four mutations that fail and one that deliberately does not: the
provoke reverted to the shipped flat aim (which fails the axis *and* the heart
count), the level test reverted to the 1.2-tile band (the same two, which is how
the first draft was caught), the vertical lane widened to everything off the
strip (which fails the confinement clause), and the telegraph never started
(which fails in three *existing* tests, so the aim itself was well guarded all
along). The one that passes is the crawling aim factor moved from 0.45 to 0.15,
and it is written down rather than fixed: measured against the clamp, a standing
player's aim point is always below the band and always pulled up to it, while a
crawler's sits inside it — and both values land on the part of him that is
actually there, so there is nothing to hold. The wide monkey was then re-run at
24 seeds and 120 seconds over all seventeen sectors *and* the four washrooms —
**14.5 million steps**, with the invariant the suite does not keep and one it has
never had (a guard's `aim_vdir` inside its own range) — because this changes when
every provoked guard on every shipped map stops to aim, and that is not a
comment. Clean. `make win` cross-compiles at nought warnings and `make
sanitize` walks the whole sweep clean.

Re-measured afterwards: `make coverage` at `none` functions and **459**
unexecuted lines, down from 466 — `gameplay_ai.c` 71 to 66, which is the new
axis and the shared aim being reached rather than anything else moving. And the
figure this pass was actually asked for: a guard who could be killed by three
free jumps on the head charges a heart of three for it now.

**And then a player said the woman being taken has one arm, and she did — on the
one pose of hers the game shows least.** `draw_hostage` has two arm branches. The
bound one puts a bent sleeve forward and ends it in two small hands taped
together, so the pair is unmistakable; the untied one drew a single sleeve down
the leading flank. She is untied in exactly two places, the kerb where she is
taken and the reunion, and bound everywhere else — the arrival, the report
between sectors on all six boundaries that show one, and the outro walk. So a
player meets the two-handed drawing eight times and the one-handed drawing on the
third screen of the game, reads the second against the first, and is right.

Three things are worth keeping, and the first is the general one.

**A figure drawn two ways owes the two drawings the same anatomy.** Nothing else
about the untied pose was wrong: every other figure in
[cutscene.c](src/cutscene.c) shows one arm, `draw_walking_arm`'s own comment
explains why a profile only needs one, and the reunion had stood for as long as
the ending has. What made this one read as a missing limb is that *her other
pose shows both hands* — the tie is what puts them there — so the count became a
fact about the character rather than a convention of the view. The far arm is a
shoulder and a forearm past the back flank, dimmed because it is on the unlit
side and a pixel higher than the near hand so the two do not sit level and merge
into the hem. The bound branch is untouched: both wrists are in front of her
there, and a third arm behind them would be the fix applied where it is wrong.

**No gate in this tree could have seen it, `--shot` included, and this one is the
sharper version of that sentence.** A photograph *does* show it — unlike the
strobing skyline or the patrol that stopped being one, where the frame is
identical either way — and the sweep has drawn `--screen abduction` for its full
length on every run since the screen existed. What was missing is that nobody had
cropped to her. That is the same lesson as the helicopter that flew backwards,
one beat earlier in the same prologue: **a defect a photograph can show is only
found by somebody looking at that part of the photograph**, and it is now the
fifth player report in five releases that no gate here could have produced.

**And the fix has no mutation, which is the honest form rather than a
shortfall.** `make test` links no SDL, `--screen` names a state and this is a
drawing inside one, and no counter can tell a frame that was drawn from a frame
anybody could read. What verified it is a capture in each direction at each of
the three moments the pose is on screen — the walk up the pavement, the walk back
to the SUV with the crew either side of her, and the reunion — before and after.
`make test`, `make lint`, `make win` at nought warnings and `make sanitize` with
the full sweep are all clean, and `make coverage` is unmoved at `none` functions
and 459 lines, which is what a renderer fix on the far side of the boundary
looks like.
