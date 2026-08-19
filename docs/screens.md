# Everything that is not a sector

## The options sheet

Everything the player is allowed to decide is one struct and one table, both in
[settings.c](../src/settings.c) / [settings.h](../src/settings.h): two audio levels,
fullscreen, the CRT filter, reduced motion, the three assist switches and the
veteran run, and — on two further pages — which key does what, and the row that
throws the records away. The sheet opens
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

**And the sheet's last section runs the other way.** `VETERAN` is one switch
under a `CHALLENGE` heading, and it is the answer to a campaign somebody has
already finished: the crew at `VETERAN_ENEMY_SPEED`, `VETERAN_LIVES` in hand and
`VETERAN_CONTINUES` behind them. Three numbers, all read at the places the
assist switches are already read, and deliberately not a second *tuning* —
every map, hazard budget and jump in the tree is drawn against the pace in
[game_config.h](../src/game_config.h), and a mode that moved several of those at
once would be a set of sectors nobody had played. The pace is still under
`PLAYER_WALK_SPEED`, because a crew that outran Chuck on open floor would mean
there is no such thing as breaking off — which is a different game rather than a
harder one.

**And `VETERAN_LIVES` is a number the run has to keep**, which took a while to be
true. `campaign_reset` was handed the flag, spent it on the opening lives and
continues, and forgot it — and `campaign_accept_continue`, which is the *other*
place lives are handed out, had nothing left to ask and handed out `PLAYER_LIVES`.
A veteran run opens with no continues at all, so its very first death takes the
branch that resets the score: one mistake, which on a one-life run is the whole of
the run, and it came back with three lives. Two of the three numbers the mode is
survived a continue and the defining one did not. The flag is on `CampaignState`
now, kept in step by `apply_assist_to_state` — the one function every change to a
run's difficulty already passes through, because the switch can be reached from the
pause sheet mid-run — and
`test_the_veteran_run_is_three_numbers_and_no_more` walks the continue as well as
the reset. Unlike `assisted` it is not sticky: this one is live difficulty and
follows the sheet in both directions.

Two things about it are decisions.

**It is not locked behind finishing.** Every row on this sheet is a thing the
player may choose, and a row drawn but refusing to be chosen would be the first
one in the table that is not: the cursor, the renderer and
`test_settings_cursor_only_lands_on_rows` would all need teaching about a state
no other row has. Somebody who wants the hard run on their first night can have
it, and the heading says who it is for.

**The pace reaches a sector already running, and so do the lives.** A switch
that only took effect at the next doorway would be a setting the player cannot
see having changed anything, so `apply_assist_to_state` puts both
`state->veteran` and `campaign->veteran` on the sheet in both directions. Only
`continues_remaining` waits for a new run, because that one is
`campaign_reset`'s alone.

**This page said the opposite of that two paragraphs earlier, and the sheet
believed it.** The passage here read "the pace reaches a sector already running
and the lives do not", argued that reaching into a run to take two lives off
somebody would be the only thing on this sheet that costs a player something
they already had, and cited the row's own `NEXT RUN` as proof — while the
paragraph above it, on the same page, explained that the flag lives on
`CampaignState` precisely so a *continue* can ask it. Both cannot be true.
`campaign_accept_continue` reads the flag, so flipping VETERAN on mid-run cuts
the next continue from `PLAYER_LIVES` to `VETERAN_LIVES`: one flip, one death,
two lives gone, on the promise that nothing would happen until the next run.
`test_the_veteran_run_is_three_numbers_and_no_more` had been *asserting* that
behaviour the whole time, so the suite and this page disagreed in the same tree
and the row sided with the page. It says `THIS RUN TOO` now.

Two things are worth keeping from it. The first is that **the sheet is the only
one of the three that a player reads**, which makes a wrong word there worse
than a wrong comment — the same reason the `$A` pad cap further down this page
mattered more than any of the code around it. The second is that a comment which
cites a user-facing string as its justification has quietly made that string
part of the invariant; there was nothing holding the two together, and the
argument read as a check for as long as anybody skimmed it.

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

**Which key does what is a page of the options sheet rather than a section of
the main one, and the reason is arithmetic.** The plate is sized from its rows
and the frame is 552px tall, of which the title, the strap, the footer and the
margin take about 120 — so a page is roughly eight rows and four headings before
it has to start squeezing them. So nine controls with two keys apiece, and two pad
buttons beside them, do not go on the end of the main page: they are a page
reached from a row, the way the manual and the options sheet are siblings off the
title screen.

**And the records are the third page, which arrived the other way round and is
the reason the arithmetic above is now measured rather than asserted.** That
section was *added* to the main page, took it to 716px against a 536px budget,
and the squeeze meant to absorb that scaled the value rows and then took the
scale off the headings as well — which cannot be done, because a heading's height
is its rule plus its sentence. So the plate came out about 50px shorter than the
rows laid on it: the RECORDS strap was printed through the footer prompt, and
`RESET RECORDS` and its detail line were drawn below the plate and off the
bottom of the screen. On every machine, because the renderer's presentation is
logical and fixed at `VIEW_W`×`VIEW_H`, and in the still the store page is cut
from.

Two things came out of it. The layout is `settings_page_layout` in
[settings.c](../src/settings.c) now rather than arithmetic inside the renderer,
which is the same rule every table of words in this game keeps — the geometry
lives where `make test` can reach it — and
`test_every_page_of_the_options_sheet_fits_the_frame_it_is_drawn_in` adds the
axis nothing had ever measured. The width check beside it is called "every word
… fits the plate" and had always asked about one axis of two.

**And then somebody asked how much room was left, which `fits` cannot answer.**
It is a cliff: true for every page since the split, and silent about how close
the edge is. Measured, the main page was *at* it — one spare value row, and
**none** with the mute warning up, drawn at a squeeze of 0.68 against a floor of
0.6, which is about 4px of air between a detail line and the label under it where
the design allows 17. The gate does fire on the row that would break it, in the
muted state, one release before a player could see anything; what it cannot do is
tell the next person adding a row that they are adding it to a full page. So the
layout answers `spare_rows` as well, the header states the figures, and the fit
test checks the number against the function rather than restating it — a page with
`spare_rows` in hand must still fit a frame that much shorter and must not fit one
shorter still, since a row costs a *squeezed* row rather than a whole one.

**DIFFICULTY is the fourth page, and it is the split that paragraph asked for.**
What it said the page needed was another split and that the choice of *which*
rows was "a decision about what a player should see rather than something a check
can make" — so the check kept reporting one spare row for a release while the
decision waited. ASSIST and CHALLENGE came off together rather than one at a time,
because they are one question asked in both directions: `gameplay_enemy_speed_scale`
reads both switches and resolves them against each other, the comment on
`ChallengeOptions` calls veteran "the other direction", and a sheet holding one of
the two would answer half of "how hard do you want this". The main page keeps what
a player changes on the way past — the two levels and the three display switches —
and carries a row down to each of the other three pages, under a heading of its
own. `CONTROLS` used to hang off the end of DISPLAY and `RECORDS` off the end of
CHALLENGE, which put two "this opens another sheet" rows inside two sections about
something else: a grouping nobody chose, arrived at because there was no room for
a fourth heading. Measured after the split, the main page has **six** spare value
rows and no squeeze at all in either state — 502px of plate against a 552px
frame, or 516 with the mute warning up.

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

**And "two keys, then two buttons" is the order they are drawn in, which for a
release they were not.** The caret is one number — `settings_bind_slot`, stepped
by left and right — and slots 0 and 1 are the keys. The renderer pinned the
*keyboard's* pair to the right margin and put the pad's to its left, under a
comment saying that made a row read keys-then-buttons "in the order the caret
walks them". So the page opened with the caret on the third cap from the left,
LEFT was refused at slot 0 while two caps sat beside the label, and the two the
caret reached last were the two drawn first. The heading over the page said one
thing, the manual's own `CONTROLS` sheet drew the other, and the two screens
disagreed for as long as nobody counted — which is this tree's most reliable
smell and the one it keeps finding on sheets rather than in simulations.

Nothing could have caught it. The fit check measures the run's *width* and the
order does not change it; the soak sweep drew the frame every run, and a counter
cannot tell a frame that was drawn from a frame anybody could read. What makes it
checkable is that the geometry moved to
[settings.c](../src/settings.c) — `settings_bind_caps`, beside
`settings_page_layout`, which had moved there for the same reason one release
earlier — and `test_the_binding_caps_run_left_to_right` asks the property rather
than the numbers: cap `i + 1` starts right of where cap `i` ends, the keys come
first, the two groups are further apart than two caps of one group, and the run
finishes on the margin it was handed. The check that used to measure the run laid
it out a second time, "exactly as `draw_setting_keys` lays it out"; it asks the
one function now, because two copies of a layout is the arrangement every drifted
pair in this repository started as.

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

**One key does one job, and it is swapped rather than taken.** `keybind_set`
moves the key off whoever held it — including the same action's other slot,
since a binding that leaves the old owner in place is a key firing two actions,
which on a keyboard is indistinguishable from the game being broken — and hands
that action whatever the slot it went into was holding.

**The swap is the half this got wrong for as long as it existed, and it cost a
run.** Clearing the old owner could leave an action answering nothing, and the
page said so approvingly: "a thing the player did, the sheet draws it as an
empty cap, and the reset row is on the same page." Read against the single
likeliest edit anybody makes on this sheet — jump onto SPACE, which is ATTACK's
only key — that is a player walking out of the options screen unable to fire,
told by a `-` on a prompt they may never read. Worse with `USE`, whose only key
is `E`: sector 14's window is on the far side of a door pair, `gameplay_use_door`
is the only thing that opens one, and an emptied `USE` is a sector that cannot be
finished. Swapped, ATTACK inherits the LSHIFT the jump gave up and both actions
still work, which is what the player asked for and all they asked for.

**And a bind with nothing to hand back is refused.** Putting SPACE into the
jump's *second* slot offers ATTACK nothing in exchange, so it would simply lose
its only key; the sheet answers that the way it answers an unbindable key and
nothing moves. The rule is the one this page already keeps one level in — ESC,
ENTER and BACKSPACE are unbindable because a sheet that let them go could lock
the player out of the sheet — applied to the game rather than to the screen.
`test_no_sector_is_locked_behind_an_unbindable_action` sweeps every action, slot
and key from the defaults and requires that no accepted bind ever leaves an
action empty, and it holds the door-pair claim to the shipped maps so the reason
cannot quietly stop being true. A file edited by hand can still arrive with an
action empty, and the empty cap and the prompt's `-` still say so: emptiness is
legal to hold and no longer possible to cause.

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
key names for each of them. The sheet is the tightest of them and has lost
a line to exactly this once already.

## What outlives the process

The campaign is seventeen sectors and a prologue, which is more than one sitting
for most people, and none of it used to survive the game being closed. Three
things now do, in [progress.c](../src/progress.c): the **best score** any run has
finished on, the **furthest sector** any run has reached, and the **quickest any
sector has been cleared**. The module links
no SDL for the same reason [settings.c](../src/settings.c) does not — the shell
owns the file, `SDL_GetPrefPath` being the only part that needs a platform —
and it is a second file rather than a section of the first, because the two
answer different questions: the settings are what the player decided, the
progress is what happened, and wiping a campaign must not cost somebody their
volume levels.

Three decisions carry it.

**The times are the newest of the three, and they exist because the game had
been asking for them all along.** The night clock gives every floor 134
seconds and `campaign_award_sector_bonus` pays for each second handed back,
so the report between sectors has printed a stopwatch and a par bonus since it
existed — against a par that belongs to the *night* rather than to this player.
There was nothing on screen a player could measure themselves against, which
makes a stopwatch a number to glance at rather than one to play for. The record
is drawn in the same TIME field on the report, and a run that has just set it
shows **its own** time in the credit colour rather than the one it beat: the old
number printed under the word BEST on the very screen that replaced it is a
field disagreeing with itself. That also covers a first clear, which has nothing
to compare against and is a record by definition.

**Banking it and showing it are two arguments, and only the first was ever
made.** The record is written for all seventeen sectors and the report that
prints one is shown after six: the ten that leave by a window cut straight to
the next sector, and the last one cuts to the outro. So eleven per-sector
records were written to the player's disk, kept across sessions, and reachable
by no screen in the game — while those same eleven clears went on paying a time
bonus and a clean bonus with nothing on screen connecting the score to either,
which is precisely what the paragraph above says a bonus must never do. The
fiction's objection is to the *cut*: a window is a continuous physical route
onto the facade and an ops-room report over it would contradict the display.
None of that is an argument against a line. [sector_tally.c](../src/sector_tally.c)
is that line — the sector, the clock, the record, and the two bonuses — drawn
low over the reveal of the sector above, and under the card that ends the
campaign. `test_every_sector_reports_or_tallies_and_none_does_neither` holds
the two sets against the maps so that a `Y` added to a sector moves a clear
from one to the other instead of off the end of both, and
`test_the_sector_tally_fits_the_frame_it_is_drawn_in` measures the widest line
any run can make against the frame in [sector_tally.h](../src/sector_tally.h).

**And the same argument had one more thing on the suppressed screen that this
fix walked straight past.** The report carries a *line of the plot* as well as
the numbers, and `TRANSITION_INTEL` is sixteen rows against six reports — so ten
story beats were in exactly the state the eleven records above were described as
being in, and the paragraph rescuing the records did not look up. Fixing one half
of a symmetric defect is the most reliable way to stop anybody looking at the
other half, and this is that, one file over from where the page already says so.
The tally carries the sentence too now, in the report's own grey above the amber
numbers, and the assertion is about *reach* rather than width: every row of the
table either shows a report or rides the tally. The band grows away from
whichever neighbour it has — upward off the bottom edge on a reveal, downward
under the verdict on the cleared card — and getting that wrong the first time
printed the sentence straight through `SHE IS TWENTY FEET AWAY` on a frame no
test could see and no run reaches. `--screen reveal` is in the soak sweep so the
other placement is drawn by something too: a placement is not covered because
its twin is.

Two properties of it are the rest of this section's rules, restated for a value
that is not an integer. It is **banked on every way out of a sector** — the
stair door, the window onto a climb, and the last floor of all — because a
record only the stair door could set would quietly exclude ten of the seventeen,
which is the same argument the bonus above it already won. And
`PROGRESS_NO_TIME` is nought meaning *nobody has cleared this*, never a perfect
run: `progress_note_sector_time` refuses a figure below `PROGRESS_MIN_TIME`, so
a damaged line cannot file an unbeatable record against a floor nobody played.
The file writes one `sector_time N SECONDS` line per sector that has one, so it
stays as short as the player's progress actually is and a build tracking more
sectors reads an older file unchanged.

**An assisted run banks none of it, and the sheet says so.** The three assist
switches take effect on the frame they are flipped, which makes "was this run
assisted" a question the finish cannot answer — so `CampaignState.assisted` is
sticky (`campaign_note_assist`, set wherever the assists reach a simulation) and
`campaign_records_count` is the one gate the shell's `progress_note_*` calls read.
A run that spent one sector on infinite lives banks no score, no sector time and
no docket. That is not tidiness: a par set with infinite lives is a par nobody can
beat honestly, and the player who set it was never told they had stopped competing
— which is the same complaint this section makes about a stopwatch nobody can
measure themselves against, one turn further on. The RECORDS page of the
options sheet states the rule in its strap, the game-over card and the outro print `SCORE n -
ASSIST, NOT RECORDED` instead of a best, and `THE RECORD` sheet in the manual says
it a third time where all seventeen numbers are read.

**And the RECORDS page shows the figures, which for a release it did not.** Its
strap listed the three things the game keeps and the only other row on it offered
to delete all of them, so the one screen in the game whose whole subject is the
records was the one screen that would not print one — they were readable on the
manual's `THE RECORD` sheet and nowhere else, and the destructive row was asking
"are you sure" about numbers the player could not see. It carries four readouts
now: the best score, the docket out of `campaign_docket_sheets`, the furthest
sector, and how many of the seventeen sectors have a time on them at all — which
is the line that says there is a grid of them to go and look at. The seventeen
themselves stay on the manual sheet, because a grid is not a row.

**And the third of those said *floor* for a release, in five places at once.**
The row's label read `FURTHEST FLOOR` over a value that formats as `SECTOR 09`,
under a detail line saying "THE HIGHEST SECTOR ANY RUN HAS REACHED" — one word
out of three disagreeing, on the screen whose whole subject is what the game
remembers. It is not a synonym in this game: `BUILDING_FLOORS` is forty and
`CAMPAIGN_SECTORS` is seventeen, and AGENTS.md keeps the two constants
deliberately underived from each other because the tower is a height and the
campaign is a route up it. What let it happen is that the words existed twice —
`RECORD_LABELS` in [run_tally.c](../src/run_tally.c), which had a
`_Static_assert`, a test and **no caller in the game at all**, and a second copy
spelled out in `RECORD_ROWS` in [settings.c](../src/settings.c), which was what
the player actually read and was held by nothing. They have one home now and
reach the renderer from it through `settings_row_label`, and
[check_docs.py](../tools/check_docs.py) derives the label so this paragraph
cannot be the copy that goes stale next — which is what the sentence above it
was, for a release, having been written while the row still said floor.

Two things about the readout are worth keeping. It is a **row kind**
(`SETTING_ROW_READOUT`) rather than a block the renderer draws under the table,
because everything that makes this sheet safe is per-row: the plate's height is
added up from the rows, the fit check walks the rows, and the caret walks the
rows. A line drawn outside the table would be a line on this plate that none of
the three had an opinion about, which is exactly how the sheet came to be drawn
50px off the bottom of the frame the last time something was added to it. And a
record nothing has been written to prints as `--` rather than as nought, for the
reason `PROGRESS_NO_TIME` is not nought: all four are high-water marks that only
rise, so nought and never are the same state, and a page offering to clear a
record must not show one where there is none. **`furthest_sector` is
deliberately outside the gate**: the resume chip is navigation rather than a
record, and a player who took the assist to reach sector nine still has to start
at sector nine.

**And that rule had to be applied twice more in the file that exists to hold
it.** [run_tally.c](../src/run_tally.c) is one file so that a record reads the
same wherever it is read, and it was keeping the rule on two of its three lines.
The end-of-run card's score line fell through to its comparison clause and told
the first player to die before scoring `SCORE 0 - BEST 0` — quoting a record
that does not exist at them, on their first sight of the scoreboard, while the
RECORDS page described the identical state as `--` two screens away. The docket
line beside it had suppressed itself in exactly that state, for exactly that
reason, since it was written. It is `SCORE 0` now, and
`test_no_end_card_quotes_a_record_before_there_is_one` asks the property rather
than the strings: no line quotes a record until one exists, and every line
quotes it once there is one, with the card and the page driven from the same
`Progress` in the same state so they cannot answer differently again.

**And there was a third line, which that fix could not see because it was not in
that file at all.** The check above drives the card and the page, and its own
closing comment says so — "which is the check that would have caught this". There
are three screens that show a record, and the third is `THE RECORD` sheet in the
field manual, which spelled its two run figures with an `SDL_snprintf` of its own
because the manual is handed plain integers rather than a `Progress` (see
`ManualRecords`, and the split it exists to keep). So a fresh install opened that
sheet and read `BEST SCORE 0` and `DOCKET 0`, while the options page reading the
same file said `--`, and while the seventeen sector cells directly above on that
same card — which do come through `run_tally.c` — said `--:--`. The card
disagreed with the page beside it, with its own grid, and with the rule the file
it bypassed exists to hold. `DOCKET 0` is the one that costs something: it reads
as "your best night carried no sheets" rather than "no night has finished", which
is the misreading the score line above was fixed to stop.

What was missing was not the rule but a way in for a caller holding the number
instead of the file. `run_tally_format_record_value` is that, and
`run_tally_format_record_line` puts the label in front of it so the card's two
lines are a label and a figure from the one place that owns both;
`RUN_TALLY_RECORD_LINE_W` is the column those lines have to fit, because
otherwise they are a renderer's own literals in a renderer's own layout — the
state this sheet's neighbour was found in with a line already off the plate.
`test_every_screen_spells_a_record_the_same_way` now asks the property of every
figure at once, with no list anywhere of which string is right.
**A fix that names the screens it covers has counted them**, and this one counted
two of three.

And `SECTORS TIMED` counted the wrong set. `Progress` keeps
`PROGRESS_MAX_TRACKED_SECTORS` times against `CAMPAIGN_SECTORS` floors on
purpose — the slack is what lets a longer campaign ship without a new file
format, and `progress_parse` accepts every index inside it — so the count walked
the array while the denominator named the campaign, and a file from such a build
or from anybody with a text editor printed `20 / 17`, a fraction over its own
whole. **A numerator and a denominator have to be asked the same question**, and
the question is the campaign that is running.

**And the sheet can throw the records away**, which is the answer for somebody who
found that rule out too late. `RESET RECORDS` clears the best score, every sector
time and the docket (`progress_clear_records`) and leaves `furthest_sector`
standing, so it does not answer "my times are polluted" by also discarding the
campaign the player is in the middle of. It is the only row on any of the four pages whose
action cannot be undone, so it is the only one that is armed rather than taken:
the first press swaps the row's own detail line for
`SETTINGS_RECORDS_ARMED_DETAIL` in the danger red, the second press spends it, and
anything else at all — a cursor move, a value change, closing the sheet — disarms
it through `settings_disarm_records`. That is the pause menu's rule about
ABANDON RUN, restated for the row that reaches the disk.

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
exception the rule needs. `load_level` takes a `LevelEntry` saying *how* the
sector is being entered, because `game_start_at_level` is not a campaign path:
`--level N`, the editor's playtest button and the debug picker all hand it an
arbitrary sector, and banking those meant that opening sector 15 to check a map's
lighting unlocked the title screen's resume chip at a floor nobody had played to —
the record quietly recording something that never happened.
`LEVEL_ENTRY_AUTHORED` banks nothing; the two campaign entries bank. A resume
comes through the authoring door and needs nothing banked, because the sector it
names is by definition already in the record.

It is an enum of three rather than the flag it started as, and the third state is
why: the same question decides whether the explosive in Chuck's hands survives the
doorway, and the answer is not the same as the banking answer.
`LEVEL_ENTRY_RUN_START` — the first sector of a run, and the retry after a
continue — is an arrival worth writing down and *not* a step out of the sector
below, so there is nothing to bring through. Only `LEVEL_ENTRY_CAMPAIGN_STEP`
hands anything over. A bool could answer one of those two questions and was
answering the other one by accident.

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
prompt naming a button that does nothing.
**Whether the sweep drew it used to depend on whose machine the sweep ran on.**
Five things in [intro.c](../src/intro.c) hang off `resume_offered` — the chip's
width, the row's centring, the hit plate and the drawing — and all of them come
off `progress.furthest_sector`, which was read from the runner's disk. A
developer who had played to sector 2 rasterized the chip on every soak; a clean
checkout never did, and neither could tell which. `--shot` and `--soak` take the
shipped defaults now, so `--screen resume` is what reaches it: the last sector's
number, which is also the widest one the chip has to fit beside START. The best score is drawn on the two
screens a run *ends* on instead, which is where a score is being looked at
rather than played for: the game-over card, and — since
[run_tally.c](../src/run_tally.c) — the outro's own thank-you card. The second of
those printed nothing for as long as it existed, so the ending the campaign is
played for reported less than the ending that comes of dying on sector three, and
the twelve-sheet docket was acknowledged only to the player who lost. Both draw
the same two lines out of the same file now.

**And the card that ends the campaign no longer points upward.** `STATE_LEVEL_CLEARED`
is reached from exactly one branch of `try_finish_current_level` — the one where
there is no sector above — because a window hands straight over and a stair door
draws the report. Its panel read `THE TRAIL LEADS UP`, which was right when every
clear drew this card and became wrong the day the other two routes were added:
the one time a player ever saw it was on the roof, at the top of the building,
with Ellen twenty feet away. It reads `THE ROOF IS HIS / SHE IS TWENTY FEET AWAY`
now. No fit check could have found it — the words fitted perfectly — which is why
a card reached by one branch is worth reading in that branch's own situation.

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
(`read_pad_hints` in [game_input.c](../src/game_input.c) → `pad_hints_apply` →
`PlatformState.pad`), and the four letters are filed by what
they say rather than by where they sit: **A confirms, jumps and skips; B backs
out, and attacks inside a sector; X attacks and opens the assist sheet; Y uses
a door and opens the manual.**
A PlayStation pad needs no swap, only a spelling — its cross, circle, square and
triangle already sit where an Xbox pad's letters do, and since every prompt goes
through `SDL_RenderDebugText`, whose 8x8 font is ASCII only, they are spelled
`X`, `O`, `[]` and `/\` rather than drawn. START and SELECT never move and only
change name (`+` and `-` on a Switch pad, OPTIONS on a PlayStation), so they are
read off `SDL_GetGamepadType` instead.

That call, and the four `SDL_GetGamepadButtonLabel` calls beside it, are the
whole of what needs a gamepad — so they are the whole of what stayed on the SDL
side. `pad_hint.c` itself links none of it: the labels and pad types are plain
numbers in [pad_hint.h](../src/pad_hint.h), held to `SDL_GAMEPAD_BUTTON_LABEL_*`
and `SDL_GAMEPAD_TYPE_*` by one compile-time assertion per row in
`game_input.c`, exactly as [keybind.c](../src/keybind.c)'s scancodes are. It read
as a file that needed hardware and did not: it needed what the hardware *said*.
Which is why the decision above — the one whose failure put quit under the thumb
the title screen asked for — is now driven by the suite against an Xbox, a Switch
Pro, a PS3, a PS4, a PS5, an unlisted pad type, and the two half-lettered pads
that have to fall back rather than commit a mixture.

The rest of the pad is bound the way the platform holders' own guidance binds
it, because a player arrives already knowing what these buttons do and every
departure from that is a bug the player blames on themselves:

**The pause sheet is a table of words like every other one**, and it was the
last one that was not. Its rows — a label, a sentence under it — plus a title,
a strap and a footer prompt were all string literals inside
`draw_pause_menu` with the plate's width a local `const float` beside them:
exactly the shape [settings.c](../src/settings.c) was found in, on the sheet a
player opens more often than any other in the game. It lives in
[pause_sheet.h](../src/pause_sheet.h) now with the geometry the renderer lays it
out from, and `test_every_word_on_the_pause_sheet_fits_the_plate` measures every
label, detail, title, strap and both forms of the footer against it. Measured on
the way out, all of it already fitted — the widest line is `GIVE UP THIS RUN AND
RETURN TO THE TITLE` at 320px inside a 420px plate, with 60px to spare — which
makes this the one sheet in the game whose fit check was written *before* it had
lost a line rather than after. The row enum comes off the same list as the words,
so the count cannot drift from the table.

**And then the fourth row arrived, which is what that check was for.** `FIELD
MANUAL` is on the sheet because the book could not be opened from inside a run at
all — see [The field manual](#the-field-manual) — and the row it needed was
measured against the plate, and the plate against the rows it now has to be tall
enough for, before either was drawn once. `PAUSE_ABANDON_ARMED` came with it:
`ABANDON RUN` is **armed rather than taken**, so its first press replaces the
sentence under it with `PRESS AGAIN TO GIVE UP THIS RUN` in the row's own red and
the second press is what ends the night. That is the pattern the options sheet's
RESET RECORDS row has always used, and this row is the more expensive of the two —
a record can be set again and the run being stood in cannot be resumed. It exists
because two shortcuts reached the row without touching it: `Q` on the keyboard and
`SELECT` on the pad both abandoned outright, and `Q` is the default
`BIND_WEAPON_NEXT`, the key a hand cycling weapons already knows. Both now land on
the row and arm it, so the shortcut is a way of *reaching* the decision rather
than a second way of making it, and moving the cursor off disarms.

The plate itself is shared with the options sheet — one `draw_sheet_plate`, lit
the same way, because they are the same object seen twice — but the two widths
are separate constants on purpose: same object, not the same size, and the
options sheet is wider because it carries key caps.

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
  pause screen only (SELECT, `abandons_run`) — and **that press arms the row
  rather than spending it**, because the sentence above was written about B and
  was every bit as true of the button beside START.

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
  pause sheet it lands on ABANDON RUN and arms it — the deliberate second step
  out of a run, and a third press is what actually takes it; on the title
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
`STATE_MANUAL`: ten sheets in [manual.c](../src/manual.c) that say who is in
the building and why, who the twelve of them are, what the building is, what
the buttons do, what the floor plan allows, what the guards do, **how to get
through a floor without any of them noticing**, how the wall is climbed, how
to read the console, and what the game keeps once the window is closed.
`THE RECORD` is the last of them and is the only sheet about the player rather
than the building: the best score, the most of the docket any one night has
carried out, and the quickest each of the seventeen sectors has ever been
cleared — the numbers `Progress` has always written to disk and, until that
sheet existed, showed one at a time on the screen after the sector that set it.
It also states the rule that decides whether a run joins them at all: any
`ASSIST` switch on and the night banks nothing, the `VETERAN` run counts, and
the options sheet can clear the lot without touching the sector being resumed
from. The first two sheets are the only place
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
footer chips turn a sheet; anything that means "done" hands back through
`game_close_manual`, which is also where `ESC` already went.

**It used to be a branch off the title screen and nothing else**, on the stated
grounds that there is no simulation to pause. That reason did not survive the
sheet beside it: `game_open_settings` has opened from `STATE_PAUSED` and handed
back to it for as long as the pause sheet has had an OPTIONS row, so the
machinery for a sheet over a paused run existed and was in use one row away. What
the restriction cost was the one moment the book is for — a player stuck on a
floor, wondering what a flash charge does or how a body is hauled, had to abandon
the run to read the sheet that explains it. Ten sheets that exist to be read,
unreachable from inside the thing they describe. `H`, `F1` and the pad's `Y` open
it from a paused run now, `FIELD MANUAL` is a row on the pause sheet, and
`game_close_manual` puts it back where it came from — which is the half that had
to be written, because every "done" key went through `game_return_to_intro`, and
that banks the run's score and ends it.

**Every sheet draws an illustration of its own, and that is why the book has a
switch.** `PAGE_ILLUSTRATIONS` in [manual.c](../src/manual.c) is one drawing
function per sheet, indexed the same way `MANUAL_PAGES` is, with a
`_Static_assert` keeping the two arrays the same length — and for a long time
that assertion was not keeping anything, because the array it measures was
declared `[MANUAL_PAGE_COUNT]`. A `sizeof` over an explicitly sized array is the
declared size, so the check read "the count equals the count" and no missing row
could fail it; a short initializer zero-fills, and a sheet listed in
`MANUAL_PAGES` with no drawing beside it was a null function pointer called on
the frame that sheet was opened. Deleting one entry built clean, passed
`make lint` and every one of `make test`'s checks, and segfaulted on
`--screen manual --page 10`. The array is written `[]` now, the way
`WEAPON_CYCLE` in [player.c](../src/player.c) always was, so the initializer is
measured against the count instead of against itself. Beside that, for a long
time the assertion was the only thing that reached any but the first of them.
Nothing turns a sheet but a hand, so a run that presses no key draws the first
and no other; some six hundred lines of drawing, plus the figure helpers only
the later sheets reach, are reached by no test at all. What made it easy to miss
is the half that *was* covered:
`test_manual_sheets_fit_the_column` measures the words of every one of them, so
the sheet everybody checks was checked and the picture beside it was not — the
same shape of gap, one layer in, that splitting the text out into
[manual_pages.c](../src/manual_pages.c) was meant to close. Nothing but turning
the pages reaches them, so an illustration is checked by looking at it.

**`GOING QUIET` is the ninth, and it is the sheet the book was missing rather
than the one it grew.** The blade behind an unaware man, a bolt thrown to be
heard somewhere Chuck is not, and a body hauled out of the room it fell in are
three answers to one rule — that this building's guards read what they see and
hear — and none of the three is announced anywhere else on screen except the
drag's own prompt. A mechanic nobody is told about is a mechanic that does not
exist for most of the people playing. They are a sheet of their own rather than
three more bullets on `FIGHTING` for an arithmetic reason as well: that sheet is
the longest of them and has already lost a line to the column once. What the
three actually do is [Going quiet](gameplay.md#going-quiet).

**Its last heading is the ceiling camera, and it is on this sheet rather than a
combat one deliberately.** A camera is not a fourth thing that fights; it is the
thing none of the three above works on — no back to get behind, no ears for a
bolt, and a crawl does nothing under a lens that is looking down at the floor.
Reading that at the foot of the page that just taught all three is the point of
where it sits: the sheet ends by naming its own exception. What it costs in
column space came out of the three sections over it, which were written long and
are now written short.

The sheet's illustration stays at two vignettes — the bolt in the air and the
body being hauled — because an illustration is a composition rather than a
contents list, and a third panel would leave three small ones where two clear
ones say the same thing.

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
