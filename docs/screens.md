# Everything that is not a sector

## The options sheet

Everything the player is allowed to decide is one struct and one table, both in
[settings.c](../src/settings.c) / [settings.h](../src/settings.h): two audio levels,
fullscreen, the CRT filter, reduced motion, the three assist switches, and — on
a second page — which key does what. The sheet opens
from the title screen or from pause (`J`, or X on a pad) and returns to
whichever opened it. It replaced a three-switch assist sheet, an `M` key and a
fullscreen key that were the whole of the game's settings, and four decisions
carry it.

**The table is the sheet.** A `SettingRow` names a value, says one sentence
about it and says whether it is a level or a switch, and `draw_settings_sheet`
in [game_render.c](../src/game_render.c) draws whatever it finds there — including
sizing the plate from the rows, so a new section costs no layout. A setting in
the struct and not in the table is one nobody can reach; a row naming a value
the struct does not hold will not compile. The cursor steps over headings, and
`test_settings_cursor_only_lands_on_rows` walks two laps each way to prove it
and to prove every reachable row explains itself.

**The module links no SDL**, the way [crew.c](../src/crew.c) does not, so the test
suite holds it to the round trip through a file and to the rules the cursor
obeys. The shell owns the file itself, because `SDL_GetPrefPath` is the only
part of this that needs a platform. **A damaged or older file is not a reset**:
`settings_parse` applies what it recognises and leaves everything else at what
it already was, so a key from a build that knew more, a line with no value or a
level outside the bar costs the player nothing.

**A setting is not a setting until something has done what it says.**
`game_settings_adjust` is the one place a value changes, and each one reaches
its own system from there on the same frame — a level to `audio_set_volumes`,
fullscreen to the window, an assist switch to whatever is running. `F` goes
through the same `game_set_fullscreen` the sheet's row does, and if the window
refuses, the setting is put back to what the window actually is: a sheet
reading FULLSCREEN ON over a windowed game is worse than the failure it is
reporting.

**Assist reaches the simulation as it always did.** The three switches live in
`Settings.assist`, and only two of them ever reach the gameplay core: they are
handed over as plain flags applied at level load (`assist_more_hearts`,
`assist_slow_enemies` — read through `gameplay_player_max_hp` /
`gameplay_enemy_speed_scale`), so the gameplay core stays deterministic and
menu-free. The third, `infinite_lives`, deliberately never crosses that line at
all: a death is a death as far as the simulation is concerned, and it is
`finish_player_death` in the shell that declines to spend the life. Keeping it
out here is what stops "a death never costs a life" from becoming a rule the
core has to know about — the core has no idea the switch exists, and a sector
plays out identically with it on or off.

**Music and effects are separate buses**, in `AudioSystem.music_volume` and
`sfx_volume`, sitting on top of the mix rather than inside it: every effect and
every score is still built at the gain it was written with. Both default to
full, so a fresh install hears the mix everything was balanced at. `M` survives
as a kill switch on top of both and is deliberately **not** saved — a game that
starts silent with nothing on screen explaining why is a game the player thinks
is broken. The pad's old mute (Y from pause) is gone: a pad muting to silence
while the sheet beside it still read 100 was two answers to the same question,
and a pad now reaches both levels in two presses. **`M` is not answered while
the sheet itself is open**, and that is the same rule rather than an exception
to it: this is the one screen showing the mix, and a sheet reading MUSIC 100
over a silent game is the pad's old bug said to the player who is looking
straight at the two levels that actually decide it.

**And the sheet says when it is being overridden**, which is the half that rule
was missing. Keeping `M` off this screen stopped the sheet *creating* the
contradiction and did nothing whatever about the other order — mute first, open
the sheet second, which is one keystroke and the likelier of the two — so a
player could still stand in front of MUSIC 100 and SOUND EFFECTS 100 hearing
nothing at all. While the kill switch is down, the sheet's audio heading
carries a line in the palette's danger red saying so and saying that this
screen is not where it is undone: a warning that named `M` as a binding here
would be a prompt for a key the sheet deliberately swallows, which is the same
bug in the other direction. The heading is found by asking the table which
section contains the sliders (`heading_governs_the_levels`) rather than by
matching its label, and the warning stands in that heading's detail slot, so it
is part of the plate's own measured height rather than something drawn on top
of it.

**Reduced motion is the fourth thing on the display section, and it is there
because the CRT filter was answering the smaller half of its own question.**
Scanlines were the player's to switch off and the screen shake was not, nor was
the alarm switch blinking at 5Hz, the mine's fuse at 24, or the cordon strobing
red and blue for the whole minute the drive lasts. None of that is decoration
the game can be played without seeing — a lit call point is how a sector reports
that its alarm is up — so the switch holds each light at its own *mean*
brightness rather than removing it: the same red on the same wall, no longer
moving. The shake is the one thing taken out entirely, and it is spent rather
than skipped (`update_camera_shake` still runs its timer down), so a blast ends
its shake when it always did and nothing but the camera can tell.

## The controls page

**Which key does what is a second page of the options sheet rather than a
section of the first, and the reason is arithmetic.** The plate is sized from
its rows and the main page already stands 516px tall inside a 552px frame; nine
controls with two keys apiece do not go in the 36px that are left. So they are a
page reached from a row, the way the manual and the options sheet are siblings
off the title screen — and the sheet now squeezes its rows if a page would still
leave the frame, which keeps "a new section costs no layout" true in the
direction it had quietly stopped being true in.

Five decisions carry it.

**The pad is on the same row as the keyboard, not on a page of its own.** It
was on no page at all for a long time, and that was the last asymmetry left in
this feature: a sheet built because "the control scheme was a property of the
source rather than of the player" shipped with the pad layout welded into
[game_input.c](../src/game_input.c) — a `switch` over letters and bumpers for
the three edges a sector reads, and a hard-coded d-pad beside the stick for the
four it polls. A player who wanted jump somewhere else could have it on a
keyboard and not on a controller.

Each row now carries four caps — two keys, then two buttons — and one caret
walks all of them, which is what keeps "what does JUMP answer to" a single line
of the sheet however the game is being held. Two pad slots rather than one
because the shipped layout needs two: attack has always answered both the
button lettered X and the one lettered B, since those are the two a trigger
finger finds and leaving B inert mid-sector read as a dead button.
`keybind_defaults` reproduces exactly what the two hard-coded paths used to do,
because a rebindable pad that quietly moved a button on first launch would be a
regression wearing a feature's clothes.

**A face binding is stored as a letter and only becomes a position when it is
read**, and getting that backwards would undo the fix
[the letter on the button](#the-letter-on-the-button) exists for. A
Switch pad prints A where an Xbox pad prints B, so a file keeping the raw
position a Switch player pressed would move their jump button the day they
plugged in an Xbox pad. The four canonical positions stand for the four
letters; `pad_resolve` turns one into the button on the pad in hand, and a
capture goes back the other way through `pad_hints_face`. Everything that is
not a face — the d-pad, the bumpers, the stick clicks — sits in the same place
on every pad and is stored as itself. The settings file is positional for the
same reason and never spells a letter.

The stick is not on the table and cannot be: it is an axis, it is read every
frame as movement beside whatever the d-pad is bound to, and there is nothing
about "push the stick left" a player could usefully move somewhere else. The
triggers stay off it too — they are the drive's pedals, and the drive is not a
sector. START and BACK are off it for the reason ESC and BACKSPACE are, and are
what cancels an armed capture on a pad.

And the four remaining decisions, which were always here.

**The model links no SDL, and its numbers are SDL's.** [keybind.c](../src/keybind.c)
is a table of scancodes in a file that links none of the library they come from,
which is a copy of somebody else's constants and therefore has to be checked
rather than trusted: [game_input.c](../src/game_input.c) asserts every row
against `SDL_SCANCODE_*` at compile time, generated from the same list, so a
scancode that ever moves is a build failure and not a key that silently stops
working. It is the rule CI already keeps for the pinned SDL version.

**One key does one job.** `keybind_set` takes the key off whoever held it,
including the same action's other slot. A binding that leaves the old owner in
place is a key firing two actions, which on a keyboard is indistinguishable from
the game being broken. That can leave an action with no key at all, and that is
allowed — it is a thing the player did, the sheet draws it as an empty cap, and
the reset row is on the same page.

**ESC, ENTER and BACKSPACE cannot be bound, and neither can the sheet's own
cursor keys be rebound out from under it.** They are pause, confirm and back —
the whole of how somebody who has just bound something unreachable walks back to
the row and undoes it. A settings screen that can lock you out of the settings
screen is worse than no settings screen. ESC doubles as the way out of an armed
capture for exactly this reason: it is the one key a player already knows means
"not this", and it is unbindable precisely so it can always mean it.

**And the prompt drawn over the sector names the key that actually works.** The
four `PRESS E` lines — the door, the two restroom doors and the terminal hack —
are built from the bound key now, because they are the only lines in the game
that name a rebindable key while it is being played, and a prompt naming a
button the state does not accept is the thing this codebase refuses everywhere
else. The manual's `CONTROLS` sheet still names the defaults and points at this
page, and that is a limit rather than a preference: its movement row is four
actions in one eleven-cell column, and no fixed table prints two six-character
key names for each of them. The sheet is the tightest of the eight and has lost
a line to exactly this once already.

## What outlives the process

The campaign is fifteen sectors and a prologue, which is more than one sitting
for most people, and none of it used to survive the game being closed. Two
numbers now do, in [progress.c](../src/progress.c): the **best score** any run has
finished on, and the **furthest sector** any run has reached. The module links
no SDL for the same reason [settings.c](../src/settings.c) does not — the shell
owns the file, `SDL_GetPrefPath` being the only part that needs a platform —
and it is a second file rather than a section of the first, because the two
answer different questions: the settings are what the player decided, the
progress is what happened, and wiping a campaign must not cost somebody their
volume levels.

Three decisions carry it.

**It is a record, not a save state.** Nothing about a sector is written down,
so a resume is a fresh run of that sector rather than a restored one, and no
file on disk can put the simulation into a state the game could not reach by
playing. What it hands back is the walk up to it, which is the part that costs
an evening.

**Both numbers are ratchets, and both are written only when they move.** The
sector is banked on *arrival* rather than on finishing — a player who died on
sector nine got to sector nine — and the score on the **five** ways a run can
end: the game-over card, the outro, abandoning from the pause sheet, the retry
past the last continue (which starts the score again from nothing and so ends
the run's scoring without passing through any of the others), and closing the
game. A worse run can never take anything off a better one, and no frame the
player is actually playing carries a disk write.

That fifth one is the one the list spent a while denying it needed. It read
"four ways… every way out of a campaign", and quitting was not among them, so
abandoning from the pause sheet kept a record and Cmd-Q one keystroke later
threw the same run away — two answers to the same question, and which one a
player got depended on which way out they happened to reach for. It is banked
in `game_shutdown` now, beside the settings write that was already happening
there. Anything that becomes a sixth way out owes the same call.

**But being put in a sector is not arriving in it**, and that is the one
exception the rule needs. `load_level` takes a `bank_arrival` flag because
`game_start_at_level` is not a campaign path: `--level N`, the editor's
playtest button and the debug picker all hand it an arbitrary sector, and
banking those meant that opening sector 15 to check a map's lighting unlocked
the title screen's resume chip at a floor nobody had played to — the record
quietly recording something that never happened. Every route the campaign
actually walks passes true; the authoring entry point passes false. A resume
comes through the same door and needs nothing, because the sector it names is
by definition already in the record.

**And a file it cannot read costs nothing.** Both this module and
[settings.c](../src/settings.c) route every value through a `read_int` that
reports whether there was a number there at all, because `strtol` answers "no
digits" with a nought and a write cut off mid-line — a full disk, a pulled plug
— leaves `best_score ` behind. Taken as written that was the one kind of damage
that *did* change something: a campaign and a best score wiped, and the CRT
filter switched off, by a save that never finished. A key this build recognises
with a value it cannot read is now worth exactly what a key it does not
recognise is worth. `test_a_value_that_is_not_a_number_changes_nothing` pins
both files.

**The title screen gained a chip, not a band.** The resume joins the existing
quiet line beside the manual and the options, at the same hint weight, because
a plate, a second plate and a keycap row is the three-band composition that
line was built to replace. It only exists once a sector has been earned; the
lobby is what START already is. Taking it is `R`, or **SELECT** on a pad — the
one button still free on that screen, since A starts, X and Y open the two
sheets and B is deliberately inert — and both are named on the manual's control
sheet, because a screen answering a button nothing names is the same bug as a
prompt naming a button that does nothing. The best score is drawn on the
game-over card instead, which is the one screen a score is being looked at
rather than played for.

## The letter on the button

SDL names a face button by its **position** — `SDL_GAMEPAD_BUTTON_SOUTH` is the
bottom one on every pad ever made — and binding straight to those positions is
what the game used to do. It is right on an Xbox pad and quietly wrong on a
Nintendo one, which prints A where an Xbox pad prints B: the title screen asked
for A, and the button printed A was the one that quit the game.

**This is also the rule the rebindable pad had to be built around**, and the
place it would have been easiest to break: a binding taken from a Switch player
and stored as the position their thumb was over would move somebody else's jump
button the day they plugged in an Xbox pad — the very bug this section exists to
record the fix for. So the four faces are stored as letters, spelled as letters
and only turned into positions at the moment they are read, and the settings
file never writes one down. See
[the controls page](#the-controls-page).

So the game binds by **letter**, in [pad_hint.c](../src/pad_hint.c). A pad is asked
what it prints on each of its four faces the moment it is plugged in
(`pad_hints_read` → `PlatformState.pad`), and the four letters are filed by what
they say rather than by where they sit: **A confirms, jumps and skips; B backs
out, and attacks inside a sector; X attacks and opens the assist sheet; Y uses
a door and opens the manual.**
A PlayStation pad needs no swap, only a spelling — its cross, circle, square and
triangle already sit where an Xbox pad's letters do, and since every prompt goes
through `SDL_RenderDebugText`, whose 8x8 font is ASCII only, they are spelled
`X`, `O`, `[]` and `/\` rather than drawn. START and SELECT never move and only
change name (`+` and `-` on a Switch pad, OPTIONS on a PlayStation), so they are
read off `SDL_GetGamepadType` instead.

The rest of the pad is bound the way the platform holders' own guidance binds
it, because a player arrives already knowing what these buttons do and every
departure from that is a bug the player blames on themselves:

- **START pauses. Nothing else does.** It is the pause button on every pad ever
  made. B carrying it as well is what this used to do, and it meant a stray
  thumb froze a sector and — once the drive put the brake on B — that braking
  opened the pause sheet.
- **B backs out, and backing out never destroys anything.** It closes the
  manual, the assist sheet and the pause sheet — and that is the whole list.
  Inside a sector there is nothing open to close, so B attacks there instead,
  beside X: A and B are the two buttons every thumb finds first, and a dead
  button under the thumb while the game is being played is its own kind of
  wrong.
  **A press of it never closes the game**, because the player did not open the
  game the way they opened a sheet, and a first press of B on the first screen
  ending the session is the worst version of that mistake — B is also what
  closes the manual, and that sheet hands straight back to the title screen
  with the thumb still on the button. During a sector, a cutscene, the report
  between sectors, the continue prompt or the drive it does nothing at all:
  dropping a run on one press of the button players use to say "not that" is
  the same bug wearing a hat. The way out of a run is deliberate and from the
  pause screen only (SELECT, `abandons_run`).

  **Held on the title screen, it quits, and that is the exception the rule
  always needed.** Quitting used to be `ESC` or the window's close box, and a
  pad in fullscreen has neither — every other letter on that screen is spoken
  for (A starts, X and Y open the two sheets, SELECT takes the resume), so B
  is the only button left to carry it. `TITLE_QUIT_HOLD_TIME` is what keeps
  the rule above intact: a press is still inert, the chip fills in the
  palette's danger red while the button is down and empties again as fast when
  it comes up, and the hold is not armed at all until B has been seen released
  (`quit_armed`) — otherwise closing the manual and not letting go would walk
  straight on into closing the game. The mouse needs no hold, because clicking
  the chip is the same act as clicking the close box.
- **SELECT has exactly two jobs of its own, and they never overlap.** From the
  pause sheet it is the deliberate second step out of a run; on the title
  screen it takes the resume the third chip is offering, and does nothing when
  there is none. It is the only letter-free button left on that screen — A
  starts, X and Y open the two sheets, and B is inert until it is held — which
  is why the resume is on it rather than on a face.

  Everywhere a **sheet** is open it also means "done", alongside B, START and
  whichever letter opened the sheet (`handle_manual_gamepad`,
  `handle_settings_gamepad`). That is not a third job, it is the one rule every
  button on those two screens follows: an open sheet answers everything that
  could plausibly mean "put this away", because the cost of guessing wrong
  there is nothing at all. This paragraph used to end "nowhere else does it do
  anything at all", which was a sentence the code had never matched — and a
  rule stated more absolutely than it is kept is worse than one stated loosely,
  because the next reader trusts it.
- **X keeps its letter inside the manual.** Everywhere else X opens the options
  sheet, and the manual and the options are the two sheets the title screen's
  quiet line offers, so X crosses straight from one to the other rather than
  being folded into the manual's list of buttons that mean "done" (`J` on the
  keyboard does the same). `game_open_settings` therefore accepts
  `STATE_MANUAL` and returns to the title screen from it, because that is where
  both sheets hang. Left out, X was the one letter on the pad that did nothing
  at all on that screen.

  **`ESC` keeps the same promise, and the list of states it keeps it in is
  written out in [main.c](../src/main.c) rather than implied.** It pauses whatever
  is running, and it is deliberately inert at `STATE_LEVEL_TRANSITION`,
  `STATE_CONTINUE` and `STATE_LEVEL_CLEARED` — the three places a run is still
  on the table and nothing is open to close. The continue prompt is a live
  decision, and the countdown already reaches the title on its own if nobody
  answers. `STATE_LEVEL_CLEARED` is the second between finishing the last
  sector and the outro starting: it is the one moment in the game where the
  player has just won, and `ESC` landing in it used to replace the ending with
  the title screen. What is left — the prologue's two cutscenes, the manual,
  the outro and the game-over hold — has no run at stake, so there `ESC` is the
  way out rather than an accident.
- **The bumpers cycle.** RB takes the next weapon and LB the one before it
  (`player_select_prev_weapon`, one ring walked in both directions so the two
  can never disagree), and in the manual they turn the sheet, which is the one
  job Microsoft's own gamepad guidance gives a bumper. What they must not carry
  is a setting: mute used to sit on LB, where nothing would look for it and
  where a thumb reaching for a weapon found it. It is on the pause screen now,
  with the rest of the settings. The keyboard walks the same ring in both
  directions — `TAB`/`Q` forward, `Z` back — because a keyboard that could only
  ever go forward is three presses from the weapon a bumper reaches in one.
- **The triggers drive.** The drive answers RT and LT as throttle and brake as
  well as the letters it prompts for, because that is where a driver's fingers
  go.
- **The left stick moves a cursor wherever there is one.** Every menu in the
  game answered the d-pad and nothing else, while the same stick steered Chuck,
  steered the car and is the first thing most thumbs reach for — so the pause
  sheet, the options sheet and the manual were three screens where half the pad
  quietly stopped working. The footers were honest about it (`D-PAD: SELECT`),
  which made it a gap rather than a lie, but naming a limitation is not the
  same as having a reason for it. `menu_stick_step`
  ([game_input.c](../src/game_input.c)) reads the push as the d-pad button that
  means the same thing and hands it to the very same three handlers, so there
  is exactly one description anywhere of what up does on each sheet; the
  footers now say `LS/DPAD`. Two things about it are deliberate. A stick is an
  axis and a menu wants presses, so the **edge is made here** — a step is taken
  when the push changes, and holding a direction is one step rather than a row
  a frame, which is also why the handler runs on the release. And **nothing
  without a cursor is touched**: a synthesised d-pad press has no business
  reaching a sector, where the stick is already read every frame as movement,
  and none reaching the title screen, whose chips are not a list.

The drive is the one state that reassigns a letter, and it is worth knowing why
the exception is allowed: a car is not a platformer figure, so **A is the
accelerator and B the brake for as long as `STATE_CHASE` is up**, held rather
than pressed. A therefore stops confirming there (`confirm_with_gamepad`
returns early) and the skip moves to the one letter still free, Y, which
reaches the simulation as `use_door`. START still pauses, as it does
everywhere.

Two rules follow, and both are the reason this is one module rather than a
ternary at each prompt:

- **Nothing spells a letter itself.** A prompt is a template — `$A`, `$B`, `$X`,
  `$Y`, `$START`, `$SELECT`, `$LB`, `$RB` — handed to `pad_hint` along with the
  keyboard line, and it comes back spelled for whatever is in the player's
  hands, or as the keyboard line when that is the keyboard. The manual's control
  table is written in the same tokens, and its chip columns are measured
  **after** spelling, because `$START` is six characters and OPTIONS is seven.
  **A drawing of a pad owes the same rule, and owes it twice.** The manual's
  gamepad illustration (`illus_controls` in [manual.c](../src/manual.c)) is the one
  place that renders the four faces as objects rather than as text, so it takes
  both the letter *and the corner* off `PadHints` — `pad_hints_button` gives the
  position SDL filed each letter under, and `face_offset` turns that into where
  the cap is drawn. Nailing A to the bottom corner was the same bug the token
  system exists to prevent, only in paint: it told a Switch player the wrong
  thing about every button on the pad, and showed a PlayStation player four
  letters their pad does not carry, on the same sheet whose table beside it
  correctly said `X`, `O`, `[]` and `/\`. What stays fixed is the **tint**,
  which is filed by action rather than by letter, so the diagram keeps the
  colours it was drawn with whatever is plugged in.
  **And the shoulders are hardware too**, which is the part that survived the
  first fix: the faces were taught to read `PadHints` while the two bumpers
  above them went on saying `LB` and `RB` in paint, on the same sheet whose
  table three lines away spelled `$LB $RB` as `L`/`R` for a Switch pad and
  `L1`/`R1` for a PlayStation. A page that contradicts itself is worse than a
  page that is uniformly wrong, because the reader cannot tell which half to
  believe. They come off `shoulder_l`/`shoulder_r` now, centred on the bumper
  the way the face caps are centred on their own, because those names are one,
  two or three cells wide.
- **One answer per frame.** `game_pad_hints` returns the pad the frame is drawn
  for or NULL, and every renderer takes that pointer where it used to take a
  `bool gamepad_active`, so no two screens can disagree about which pad the
  player is holding.

The rule for a new prompt is therefore: name the letter, never the position, and
route it through `pad_hint`. A prompt that names a button the state does not
actually accept is the same bug in a smaller way — the drive advertised START as
its skip while START was pausing it.

**And the mirror of that is a bug as well: a state that accepts a button nothing
names.** Every edge input the sector owns is reported only from the sector,
which is why `E`, `SPACE` and the `UP`/`W` jump are gated on `STATE_PLAYING` in
[game_input.c](../src/game_input.c) alongside `LSHIFT`. Ungated, `E` reached the
drive — which reads `use_door` as the skip the pad puts on Y — and so handed
the keyboard a second way past the prologue that the drive's own prompt, which
asks for `ENTER`/`SPACE`, never mentions. The `UP` branch has a second reason:
it tests for a ladder by reading the live player out of `game->gameplay`, and
outside a sector that is whatever the last one left behind. `SPACE` was the
one that got away with it: every state it could have reached clears the edge
inputs it did not ask for, so the ungated press was harmless — and a rule kept
by accident somewhere else in the frame is not the rule being kept.

## The prologue: three beats, one shot

Pressing START plays the whole abduction before the platformer begins, in
three scenes that are staged to read as one continuous take. All three are
skippable with confirm.

**The kerb** (`STATE_ABDUCTION`, `abduction_cutscene_*` in
[cutscene.c](../src/cutscene.c)) is the beat the campaign hangs off: Chuck's car
parked outside a coffee window, Ellen walking the last block ahead of him, and
an SUV that comes up the kerb lane with its lights off. It is staged left to
right and ends with the SUV accelerating toward the tower on the skyline and
Chuck running back for his car — which is exactly the state
`CHASE_PHASE_DEPARTURE` opens in, so the cut between them is invisible. It
reuses the opening cutscene's cast, vehicles, street and rain wholesale and
adds no sound effect of its own; a thirteen-second beat does not earn an entry
in the synth table. Two things in it are load-bearing and easy to break: the
SUV's headlights are a parameter separate from `moving`, because a rolling
vehicle throwing no beam is the one detail anybody would remember, and Chuck
stops a clear seventy pixels short of the SUV's tail, because a figure
standing on the vehicle he cannot reach says the opposite of the scene.

**The drive** ([chase.c](../src/chase.c)) is a top-down, forward-only car chase:
Chuck tails the SUV through night traffic until it parks at the building the
first level opens in. It is a gameplay-core module — no SDL, seeded `Rng`, its own
`GameEventBuffer` — and [chase_render.c](../src/chase_render.c) is the only part
that touches SDL.

Road space is measured in pixels: `x` across the road, `y` along the driving
direction and growing forward, so screen-up is forward and the renderer needs no
world scale. Four phases run in order: `DEPARTURE` (the SUV pulls away, Chuck
runs to his car — skippable), `PURSUIT` (`CHASE_PURSUIT_DURATION` seconds of
driving), `ARRIVAL` (both cars brake onto their marks) and `DONE`, which is the
shell's cue to play the opening cutscene. Crashing out or letting the gap exceed
`CHASE_LOSE_GAP` only fails the attempt — and only costs a beat of it:
`CHASE_PHASE_FAILED` resumes the drive `CHASE_FAIL_REWIND` seconds back from
where it went wrong, not from zero, and after `CHASE_SKIP_AFTER_ATTEMPTS`
failures a skip press goes straight to the arrival.

**The clock and the road are handed back together**, which is the one part of
the rewind that is not obvious: `reset_pursuit_layout` takes the road position
the attempt picks up at as a parameter rather than starting at zero. It used to
keep the clock and reset the road, and the cordon below is a *spatial* ramp —
read off the block a junction is generated in — so the last of a drive that had
been crashed on was spent rebuilding the ring from its thinnest end. Measured,
a crash two thirds of the way in arrived at the tower past nought to three
junctions of twelve held, where a clean run arrives past a near-solid one: the
opposite of the thing the drive exists to show.
`test_chase_cordon_survives_a_crash` pins it, because the older cordon test
only ever drives clean.

**And at that same attempt the rewind stops, which is what makes the drive
finite.** Handing road back is only forgiveness while the player makes more of
it than they lose; someone who crashes more often than every
`CHASE_FAIL_REWIND` seconds never reaches `CHASE_PURSUIT_DURATION` at all.
Measured before this rule, a pad held on the throttle with no steering — the
most naive thing a first-time player can do with a car — never arrived in three
minutes of driving across five seeds, while a pad touching nothing at all
always did, which is a difficulty curve pointing the wrong way. So the rewind
and the skip prompt now appear together: from that attempt on, the pursuit
clock only ever grows and the prologue ends whether or not anybody takes the
skip it is offering. The prologue is a curtain-raiser and must never be the
wall someone quits the game on; all three rules are tested, the last of them by
driving the naive input end to end.

**The car has two pedals, and it says so.** `Input.gas` and `Input.brake` are
the only things `drive_player_car` reads, and the shell fills them from the
letter under each thumb as well as from the stick, the d-pad and the arrows
(`game_read_input`) — A and UP accelerate, B and DOWN brake. They are their own
inputs rather than aliases of `up`/`down` so that binding a face button to the
throttle cannot quietly bind it to climbing a ladder, and `skip_pressed` reads
`confirm || use_door` because the pad and the keyboard cannot agree on one
button once A is a pedal. None of it works if the player is never told: the
platformer never asks for a throttle, so an unaided player holds a direction
and watches the SUV pull away without ever learning the car had to be driven.
So the pedals are named twice — permanently, in the HUD line under `PURSUIT`,
which names the buttons rather than the hardware, and outright on the road at
the head of **every** attempt for `CHASE_CONTROL_HINT_TIME` seconds, because a
crash is exactly when someone needs to read them again.

Two rules keep it fair, and both are tested: traffic is never generated more
than `CHASE_MAX_CARS_ABREAST` cars wide, so at least two lanes are always open,
and the SUV holds a speed that keeps Chuck at arm's length once it is being
tailed, so holding the accelerator settles into a stable tail instead of ramming
the car his wife is in.

## The field manual

`H` on the title screen (`Y` on a pad, or a click on the line naming it) opens
`STATE_MANUAL`: eight sheets in [manual.c](../src/manual.c) that say who is in
the building and why, who the twelve of them are, what the building is, what
the buttons do, what the floor plan allows, what the guards do, how the wall is
climbed and how to read the console. The first two sheets are the only place
the plot is stated outright, and they are the same night from either side.
`THE NIGHT` is illustrated with the crew's own flight case — open,
stencilled *Meridian Facility Services*, signed in on 14 March and never
inspected. Three heads at twelve pixels across would have said nothing the
text does not; the case says the part the text cannot, which is where every
rifle and rocket in the campaign came from. `THE CREW` is the docket that case
came in on: the night access log, twelve ruled lines, twelve names drawn
straight out of `crew_callsign` rather than out of a list of its own, so the
man the strip quotes in a sector is line one of the log. Being *told* there
are twelve of them and *counting* twelve of them are different sentences, and
the second one is why the sheet is a document and not a paragraph. Arrow keys
or the
footer chips turn a sheet; anything that means "done" hands back to
`game_return_to_intro`, which is also where `ESC` already went. It is a branch
off the title screen and nothing else — there is no simulation to pause, so the
manual cannot be opened mid-sector.

**Every sheet draws an illustration of its own, and that is why the book has a
switch.** `PAGE_ILLUSTRATIONS` in [manual.c](../src/manual.c) is eight drawing
functions indexed the same way `MANUAL_PAGES` is, with a `_Static_assert`
keeping the two arrays the same length — and for a long time the assertion was
the only thing that reached seven of them. Nothing turns a sheet but a hand, so
`make smoke` drew the first and no other; some six hundred lines of drawing,
plus the figure helpers only the later sheets reach, were executed by nothing in
this tree at all. What made it easy to miss is the half that *was* covered:
`test_manual_sheets_fit_the_column` measures the words of all eight, so the
sheet everybody checks was checked and the picture beside it was not — the same
shape of gap, one layer in, that splitting the text out into
[manual_pages.c](../src/manual_pages.c) was meant to close. `--page N` opens the
book on a given sheet and the smoke run walks all eight; see
[AGENTS.md](../AGENTS.md).

It **replaced the title screen's row of control hints** rather than joining it,
and that is a composition decision as much as an editorial one: a plate, a
second plate under it and a keycap row under that left the bottom eighty pixels
of the shot carrying three bands of interface, which reads as a menu stacked on
a picture. The hints were only ever there because there was nowhere else to
learn the controls; the manual is that place, and it is named on the line they
used to occupy. That line now carries up to four chips — the resume, the
manual, the assist options and the way out — centred together as one line of
things to know about, at the same hint weight; it is still one band, which is
the whole reason each of them was allowed to join it rather than stand on its
own. At their widest — a PlayStation pad spelling CREATE for SELECT and `[]`
and `/\` for two of its faces — the four come to about 650 of the 800 the frame
is laid out in, so the line is measured but not yet full. Anything added to the
title screen from here owes the same question — the shot holds the wordmark,
START and one quiet line, and a fourth *band* has to earn itself.

The quit chip is the newest of them and the one that had to argue hardest for
its place, because the screen already had a way out: `ESC`. What it did not
have was a way out anybody could *find* — the only line naming `ESC` was the
last bullet of the manual's control sheet, which had quietly outgrown its text
column and was never drawn — and a pad in fullscreen has no `ESC` and no close
box to fall back on. See [The letter on the button](#the-letter-on-the-button)
for why it is B, and why B has to be held.

The chips are measured **after** spelling, the way the manual's control table
is: a keycap is as wide as the letter on it, because `$Y` is one cell on an
Xbox pad and two on a PlayStation, and `$SELECT` is six or seven. That is why
`intro_init` and `intro_update` take a `PadHints *` — a hit rect sized for one
pad and drawn for another is a chip the mouse misses.

Three decisions carry it, and they are the reason it is two files rather than
twenty.

**The text is a table, and the table has no SDL under it.** Every sheet is a
`ManualPageText` in [manual_pages.c](../src/manual_pages.c) — a title, a strap, a
caption and a list of typed lines — and the line kinds (`LINE_HEAD`,
`LINE_BODY`, `LINE_BULLET`, `LINE_KEY`, `LINE_GAP`) are the whole layout
language. The split is the same one [credits.c](../src/credits.c) and
[crew.c](../src/crew.c) already make, and it is made for the fit check below: the
suite links no SDL, so the words and the geometry they are measured against
have to sit on this side of the line. What stayed in
[manual.c](../src/manual.c) is the one thing that could not cross it — an
illustration is a function taking an `SDL_Renderer`, so `PAGE_ILLUSTRATIONS`
is a parallel array indexed the same way, with a `_Static_assert` holding the
two to the same length. A rule that changes in [game_config.h](../src/game_config.h) is a string
edited in one place rather than a paragraph hunted through a draw function, and
the control rows are `key|pad|action` so the two chip columns can be sized from
the widest label on the sheet instead of per row. **Prose that names a button
takes the same bar**, as `pad wording|keyboard wording` — the paragraphs used to
spell `E` at everybody, which is exactly what the `$` tokens exist to prevent,
said to the one reader with no E to press. A line with no bar in it is printed
as written, which is every line naming no button.

**And a table can outgrow its column without saying so.** `render_text_column`
stops at `MANUAL_BODY_BOTTOM` rather than drawing past it and never wraps a
line, which is right for a frame and silent for whoever is writing the sheet:
`CONTROLS` spent a long time nine pixels over, and what fell off the bottom was
its last bullet — the only line in the whole game that named the key which
closes it. A rule that is documented, written down and never drawn is worse
than one that was never written. `test_manual_sheets_fit_the_column` walks the
same kinds in the same order and off the same pitches the renderer does, so the
two cannot disagree about where a line lands, and it holds **every** sheet
rather than the open one — the answer must not depend on which page somebody
turned to. It checks all three ways a sheet goes over: past the bottom of the
column, past `MANUAL_TEXT_RIGHT`, and a caption past `MANUAL_CAPTION_MAX`,
which is clipped rather than reflowed. The width half is the one nothing used
to check at all; a line that runs off the side is exactly as invisible as one
below the bottom, and it is measured against the widest pad the game can
meet — a PlayStation's `[]`, `/\` and OPTIONS — because the chip columns are
sized from whatever is plugged in. This was a `CHUCK_DEBUG` assert in
`manual_init` until the sheets moved, which meant it only ever ran for somebody
who opened the book in a debug build.

**The vertical half asks a different question from the renderer, and has to.**
`render_text_column` clips on `y < MANUAL_BODY_BOTTOM`, which decides whether a
line *starts* inside the column — so a line beginning a pixel above the bottom
is drawn whole below it. The fit check mirrored that exactly, and inherited a
blind spot with it: a keycap is drawn from `MANUAL_KEY_CHIP_RISE` above its own
row and is `MANUAL_CHIP_H` tall, so a control row could pass the check with its
cap sitting in the footer chips ten pixels further down. Measured by their tops,
`FIGHTING` and `CONTROLS` were both certified with ink under the column. Each
kind is now measured by what it actually puts on the sheet
(`MANUAL_INK_TEXT`/`_HEAD`/`_KEY`, beside the pitches in
[manual_pages.h](../src/manual_pages.h), and named from there by
[manual.c](../src/manual.c) so the draw calls and the check cannot drift). That
cost `FIGHTING` a line, which is where `DOGS are faster and lower. No stomping.`
came from. The renderer's clip goes back to being what it always was: a backstop
that drops a line rather than drawing it off the plate.

**An illustration of the game's own interface is a cutting of it, not a
sketch.** `illus_console` draws the strip, so it draws what the strip draws:
three hearts and the lives counter beside them, and an interior sector number —
it spent a while showing the five-heart assist row and labelling itself
`SECTOR 07`, which is a climb, and a climb has an entirely different strip with
no ACCESS chip on it at all. A diagram of a console the player will never find
is worse than no diagram.

**The sheet is a thing in the frame.** A wall of type on a flat fill would be
the one screen in the game that is not lit, so it is a steel-clipped sheet on a
dark desk: a lit top edge, a dark base, rust corner brackets, and the same
vignette and scanlines every other frame is finished with.

**The illustrations use the game's own vocabulary.** The figures go through
`fx_form_mass`/`fx_form_block` with the legs dropped away from the garment, the
slabs get an arris and ambient occlusion, the cornices are drawn the way
[level_art.c](../src/level_art.c) draws them. A manual illustrated in a second
style would be a manual for a different game. Two things that cost a revision
each and are worth keeping: a caption is set from the panel's left edge, so it
is clipped to `CAPTION_MAX` rather than trusted to be short — a line that
outgrows the plate runs off the sheet — and `dash_arc` takes the height the
curve actually reaches, because a quadratic only rises halfway to its handle
and an arc drawn through the handle leaves the figure hanging above its own
jump.
