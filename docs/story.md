# The story, the words, and the people in the building

## The story, and where each piece of it is written down

The fiction is not decoration here: it decides what the HUD says, what the
manual teaches, what the captions between sectors are allowed to reveal and
what the ending can pay off. It is one page, and everything that speaks to the
player has to agree with it.

- **Chuck Ross** — twelve years an Army sapper, now a two-man strip-out crew
  that guts old buildings. That is the licence for the sidearm, for knowing
  charges, and for the `%` weak wall being a thing he can read as a bricked-up
  opening rather than a wall.
- **Ellen Ross** — his wife, and the night duty controller of Kessler Tower.
  She wrote the building's access system, which is why the sector rules are
  hers: one real key card among decoys, and an engineer's way in at every
  terminal.
- **Anton Voss** — twelve men badged into the tower since March as *Meridian
  Facility Services*, its night maintenance contractor. The flight cases they
  wheeled through the goods entrance were never inspected, which is where every
  rifle, grenade and rocket the player picks up came from. **The twelve have
  names**, and they are written down once, in [crew.c](../src/crew.c) — see
  [The net](#the-net) below. Nothing in the simulation depends on which name a
  guard wears, but the manual's `THE CREW` sheet and every line the player
  overhears in a sector come off that one list, so a thirteenth **callsign**
  anywhere is a thirteenth man the docket does not have.
  **Voss himself is not one of the twelve**, and that clause used to read
  "a thirteenth name", which his own name in a crew line contradicts on the
  first floor of the game: `TELL VOSS THE CIRCUITS CUT THEMSELVES AT MIDNIGHT`
  is a man on a handset talking about somebody who is not on the roster,
  because the roster is the twelve *badged* contractors and Voss is the one who
  badged them. So the count that reaches the player is twelve men plus the
  one who hired them, which is what makes `SIX FORTY, THIRTEEN WAYS. DO NOT
  MAKE IT TWELVE.` arithmetic rather than a slip, and `TWELVE OF US, ONE OF
  HIM` a man counting his own shift. It is the distinction between a name and a
  callsign that carries all of it, and the sentence above did not draw it — the
  usual failure of a rule that says "these must agree" without saying which
  copies it counted.
- **The cover** — a political demand broadcast at **00:04**. It puts every unit
  in the city on a cordon around this tower and nobody at all inside it. It is
  theatre, and it is also what buys the abduction its impunity: the pavement
  three blocks out is clean at 00:12 because the whole city is looking here.
  **The broadcast has to precede the drive**, because the cordon the player
  drives through is drawn thickening toward the building — a spatial ramp, not
  a temporal one, so all of it is already standing when the drive begins.
- **The job** — the sub-vault opens on the overnight settlement, six hundred
  and forty million in bearer bonds, and it is shut behind seven locks. The
  seventh runs on a two-key rule: the bank's
  key, and the duty controller alive and present. That is why they needed Ellen
  at all, and it is the only reason she is still breathing. **01:00 is when the
  bonds leave the roof**, not when the vault opens — the vault is emptied
  during the climb, which is why Voss is on the roof with them at 00:57.
- **Why Chuck is inside** — he was twenty metres behind her on the pavement
  when they took her, and he was through the front door forty seconds behind
  the men who carried her through it.

**The clock, in one place.** Every time the game states is on this line, and
none of them may be moved alone: **00:04** the demand goes out and the cordon
forms; **00:12** Ellen is taken three blocks out; **00:12-00:22** the drive, in
through the cordon; **00:22** the SUV reaches the tower, Ellen is walked in and
Chuck follows — which is also what sector one's wall clock reads, so the
cutscene and the first dial agree to the minute; **00:22-00:57** the seventeen
sectors, `NIGHT_CLOCK_*` in [game_config.h](../src/game_config.h) climbing the
dial at two minutes and fourteen seconds a sector; **01:00** the helicopter, the outro's
own caption. A change to any one of them is a change to the two cutscene
captions, the `TRANSITION_INTEL` table in [intel.c](../src/intel.c), the manual's
`THE NIGHT` sheet, the drive's cordon caption, `NIGHT_CLOCK_FIRST_MINUTE` and
both prose pages.
- **The ending** — the helicopter on the roof is the getaway, not a rescue.

Where the player actually reads it, in the order they meet it: the title
screen's one line; the abduction cutscene's captions; the drive's captions; the
opening cutscene outside the tower; **the `TRANSITION_INTEL` table in
[intel.c](../src/intel.c)**, one line on the report between sectors;
**the crew's own net** ([crew.c](../src/crew.c)), overheard while a sector is
being played; the manual's `THE NIGHT` and `THE CREW` sheets; and the outro. A
change to any of those is a change to all of them.

The last two of those are the ones that reach the player *while they are
playing* rather than between beats, and they carry the plot differently. The
intel table is Chuck working it out — one considered line, after the fact. The
net is the other side saying it themselves, in the room, with no idea he is in
it. Neither is a substitute for the other and neither may contradict the other.

**The building's height is `BUILDING_FLOORS`**, forty, and it is stated in six
places that have to agree: the title screen's tagline ([intro.c](../src/intro.c)),
the men shouting down off the facade (`CHATTER_WALL` in
[crew.c](../src/crew.c)), the credits roll twice — it opens on the height and
closes on it — the prose in [README.md](../README.md), and the store page in
both of its copies. Seventeen sectors is the *route*, not the storey count: a
sector is a stretch of the climb, not a floor, which is exactly why the two
numbers drift in prose and why neither is derived from the other.

**This paragraph has now been wrong about its own subject twice, which is the
thing worth writing down.** It first said *three* places and left the credits
roll out — which is how the roll came to be the one place stating the campaign's
length from memory as well, `SEVENTEEN SECTORS` beside the forty floors,
unmeasured, on the screen a finished campaign always ends on.
`test_the_credits_say_the_campaign_they_roll_over` closed that half. Then it said
*four*, and went on saying it while the store page shipped the number twice more:
a page written for people who are **not** reading this repository, carrying the
first line anybody reads of this game. A sentence whose whole content is "these
copies must agree" is a sentence that will miscount the copies, because nothing
counts them.

So the number has an owner now rather than a paragraph.
`BUILDING_FLOORS` is in [game_config.h](../src/game_config.h) beside
`CAMPAIGN_SECTORS`; the two tables on the SDL-free side are held to it by
`test_the_tower_is_one_height_everywhere_it_is_said`, which searches both for
*any* line counting floors rather than working from a list, and the four that are
prose or a string literal by [tools/check_docs.py](../tools/check_docs.py). The
constant is computed with nowhere and exists to be checked, which is the same
reason `INTEL_ARC_SECTORS` exists.

The table is indexed by finished sector, but only sectors that leave by a
**stair door** show a report at all — a window is a continuous physical route
onto the facade and cuts straight to the next sector. In the campaign as
shipped that is six reports, after sectors 1, 4, 5, 8, 9 and 16, and those six
carry the arc between them. The last of them is the vault, which is the one beat
in the story that used to be asserted by a caption and is now a floor.

**The other ten lines are read too, and for a release they were not.** The
sentence here said the six carried the arc "on their own", which reads as a
decision about emphasis and was really a description of ten rows nothing drew:
written, measured against the report's own divider, held against the maps by
`test_the_arc_lands_on_the_sectors_that_show_a_report`, and put on no screen in
the game. `MONITOR WALL: VOSS. HER HAND ON THE SEVENTH LOCK.` was one of them,
and `TWELVE PLACES LAID IN THE GALLEY. TWELVE MEN.` Every gate was green,
because every check on that table asks whether a line fits and none asked
whether anybody sees it.
This is the same defect as the bonuses one section down, in the same place, left
standing when the bonuses were fixed: the fiction objects to the *cut*, not to
the words, and the fix for the numbers was a line over the next sector's reveal,
which cuts away from nothing. The line rides with them now. What still separates
a marked row from an unmarked one is how much room the beat gets — a report is a
screen and the tally is a sentence — so a beat the player has to stop and take
in still belongs on the six.

**And for a release the sentence got a fifth of a second, which is not a
sentence.** The band is drawn while `STATE_LEVEL_START` is on screen and nowhere
else, and that state lasts exactly as long as the tile reveal — twelve tiles
every 0.004s, so `width * height / 3000` seconds: **0.18s on sector 1 and 0.43s
on the tallest climb**, for two lines and about 120 characters. Which is to say
the line above and the `DOCKET n/12` two sections down were on the glass for less
time than it takes to notice that text is there, and *how much* time depended on
how big the next floor happened to be. The paragraph above is a decision about
how much room a beat gets; nothing about a fifth of a second was decided.
Every gate was green, and the shape of that is the useful part: the fit checks
ask whether a line is too **wide**, the soak sweep draws the frame so
`make coverage` counts it, and `--shot` proves the pixels are there. None of the
three can see how long anybody had to read them. A line that cannot be read is
the same defect as a line that does not fit, one axis over.
`level_reveal_hold_for` holds the reveal open for `SECTOR_TALLY_HOLD_TIME` — the
crew strip's own `CHATTER_HOLD_TIME`, because two answers to "how long does a
sentence stay readable" would be two numbers nothing holds. It is the reveal
rather than the play that is held: after the reveal the player is standing at his
spawn, which on all five climbs and four of the five interiors carrying this line
is the bottom row of the map, directly behind a band pinned to the bottom edge —
letting it ride into play hid the climber for the whole of it. Against the report's
own `LEVEL_TRANSITION_DURATION` of 9.4s on the six boundaries that show a screen,
3.8s on the ten that show a line is still the cheaper beat.

Adding or moving a facade sector therefore changes which beats of the story are
told, which is the one thing about the level layout that reaches all the way into
the script — and it has already done it once without anybody noticing. Sector 14
had to gain a `Y` when sector 15 became a climb, which silently deleted the report
after it: `TWO-KEY DOOR. SHE IS THE SECOND.`, the answer to sector 8's own turn
and the only place the game says why the hostage is still alive. It sat in an
unread row through a green suite, because every check measured the line and
nothing compared the table to the maps. That beat is at sector 16 now — the vault
is the room the two-key door belongs to anyway — and
`test_the_arc_lands_on_the_sectors_that_show_a_report` holds `INTEL_ARC_SECTORS`
against the embedded maps so the next `Y` fails the build instead of the story.

**And the same alternation that deleted that beat had quietly emptied the middle
of the campaign, which is a different failure and needed a different answer.**
The reports land after 1, 4, 5, 8, 9 and 16: five of the six inside the first nine
sectors, then nothing until the vault. From sector 10 on the campaign alternates
interior and climb, so 10, 12 and 14 all need a `Y` to put Chuck on the wall, and
a `Y` suppresses the report — six sectors with no beat on the one channel a player
cannot miss, which at par is a third of the night. Nothing was wrong with any
sector. It is what the ordering does, and every check passed.

The answer is not to show a report after a window, which would contradict what is
on the display, but the crew's own net, which was already carrying beats for
exactly those sectors: `VAULT IS DRY. LOAD IT AND GET IT ON THE ROOF.` at 10, `HE
CAME UP THROUGH THE DUCTS. THE DUCTS, VOSS.` at 12, `TWO MINUTES AND THIS ROOF IS
SOMEBODY ELSE'S.` at 14. **They were written and effectively unreachable.** A
gated line competed on equal terms with every ungated one, and by the tenth sector
fifteen-odd lines are sayable — so the chance of a given call being the one that
moved the plot was about one in fifteen, on a call that also needs a live guard who
is calm, on the ground and inside `CHATTER_EARSHOT`. Half the point of writing
them was in a table nobody was going to reach.

`crew_line_in` leads with the newest thing the crew has to say, one call in
`CREW_FRESH_LINE_EVERY` — the highest sector gate the night has passed, which puts
those three beats at about a third of calls instead of a fifteenth. It reads a
slice of the roll already drawn rather than asking for a second one, so the
freshest line cannot shift a single seeded choice; and it applies at every tier,
because the newest beat is the right thing to lead with in the lobby too.

**And the sector that fell through that answer was the last one.** The three
gated lines cover 10, 12 and 14; the sixth and last report lands on the vault at
16; and the net's highest gate was 14 — so **sector 17, the roof, arrived with
nothing new to say on either channel.** It is the floor the whole night is played
for, with eight men, three dogs and four heavies on it and Voss standing on the
pad with the cases, and the freshest thing the crew could say on it was a line
written for the penthouse three sectors down. Every check passed, and the way it
passed is the point: the guard-rail below allows one quiet sector on the argument
that a quiet sector is a *climb*, and the roof came in on an allowance meant for
a wordless wall. Two lines are gated at 17 now — `CASES ARE ON THE PAD. WE ARE
WAITING ON THE BIRD.` and `THERE IS NO UPSTAIRS LEFT. THIS IS UPSTAIRS.` — and
the rule the guard-rail was standing in for is written down rather than implied.

The guard-rail is `test_no_two_sectors_in_a_row_go_quiet`, and it counts both
channels together because between them they are what the player meets: a sector
carries a beat if it shows a report **or** the net gains a line on arriving at it.
It asks two things. **A quiet sector has to be a climb** — that is the rule, and
it is the half that was previously only a sentence in a comment. And no two
sectors in a row may be quiet, which is a different complaint and still worth
keeping. One quiet sector is allowed and is the climbs — no guards to talk, no report either
side, and a wordless climb is the point of a climb. Two in a row is the stretch
where a player starts to wonder whether the game has forgotten what it was about.
It asks the net through `crew_line_allowed` rather than reading the gates, because
the schedule is presentation and a test that reached into it would be a second copy
of it rather than a question about it.

## The docket, and the only thing Chuck takes out that is not a weapon

**The cover is the plot's biggest loose end, and the collectable is what ties
it off.** The demand broadcast at 00:04 has the whole city believing this is a
political siege; the cordon is facing outward on the strength of it, and nobody
outside the building has a single piece of paper saying otherwise. Everything
Chuck learns tonight he learns by being in the room — the flight cases, the
twelve names, the settlement clock — and none of it is evidence.

`*` is ([levels/LEGEND.md](../levels/LEGEND.md)) a sheet off Meridian Facility
Services' own docket, and there is exactly one in every interior sector: twelve
across the campaign. Picking one up pays `EVIDENCE_SCORE` and changes nothing
else at all — no checkpoint, no counter the simulation reads, nothing in the
hand. It is the only pickup in the game that is worth nothing to the man
carrying it, and that is the point of it: it is worth something to everybody
else, afterwards.

Three decisions carry it.

**It is counted on the run, never on the floor.** `CampaignState.evidence_collected`
sits beside the score and the crew tally for the reason those two do — a
sector's own copy would be wiped at every doorway, and what this number is is
the state of the case at the end of the night.

**And `Progress` keeps a high-water mark rather than a set of flags**, which is
a decision about what the collection *is*. Flags would let a player assemble the
case across a dozen abandoned runs, which is a checklist; a high-water mark asks
for them in one night, which is what the fiction is about. Chuck is not coming
back tomorrow. It is read on the game-over card beside the best score — the one
screen where a finished run is being looked at rather than played — and
deliberately not on the title screen, whose quiet line of four chips is already
about 650 of the 800 the frame is laid out in. See
[The field manual](screens.md#the-field-manual) for why a fifth chip would have
to earn itself.

**And the count is on both screens between sectors, which it was not.** Everything
above is about what the sheet does to the *simulation*, and the answer is nothing
— no checkpoint, no counter a floor reads, nothing in the hand — which is right
and was quietly taken to settle a different question. `evidence_collected` was
raised by the pickup and then read by two screens, both of which are the end of
the run: a player spent a whole night on a collection laid out one to an interior
precisely so that a missed one is a floor they can *name*, and could not find out
how many they held until there was nothing left to do about it. So the report
prints `DOCKET n/12` under the score it paid for, in the credit colour the time
and clean bonuses already use, and the one-line tally over the next sector's
reveal carries the same cell — both of them, because six of the sixteen sector
boundaries show the report and the other ten show the line, and a counter on ten
of sixteen would be worse than one on none. It is on from the first clear rather
than from the first sheet: `00/12` after sector one is the game saying there is
something on these floors, to the player it is actually for.

None of that contradicts the paragraph above it. The objection to telling the
player is an objection about *Chuck*, who is carrying paper that is worth nothing
to him; neither of those screens is Chuck. They are the same voice that already
prints what the seconds were worth.

**And a sheet has to cost something to reach, or none of the above is a
decision.** `DOCKET n/12` is a number about what the player chose to go and get;
measured through the route model, seven of the twelve were sitting on a shortest
path to the way out, which makes the collection a thing that happens *to* a
player walking to the door rather than a thing they did. The placement rule is in
[levels/LEGEND.md](../levels/LEGEND.md) and it is checked now — see
`test_the_docket_sheet_costs_a_detour`. A rule about where a thing goes is as
much a rule as a rule about what it does, and this one had been prose for as long
as the sheet had existed.

## The report between sectors

One line of what the sector just told him, on the screen the run passes through
between floors. It is the sixth of the places the plot reaches the player and
the one that reaches them most often, because a thriller told a sentence at a
time between sectors is the version of it the game can be reasonably sure was
read.

**The table is [intel.c](../src/intel.c), and it links no SDL** for exactly the
reason [crew.c](../src/crew.c), [credits.c](../src/credits.c) and
[manual_pages.c](../src/manual_pages.c) do not. It lived inside
[cutscene.c](../src/cutscene.c) for a long time with its ceiling written down as a
sentence — "sixty characters, the report's first divider stands at x=526" — and
nothing at all holding it there. That is the same state the manual's `CONTROLS`
sheet was in when its last bullet quietly fell off the bottom of the column,
and it is worse here: six of these rows are the whole of the story a player
meets while actually playing, so a line that runs under the divider takes a
beat of the plot with it. `INTEL_TEXT_LEFT` and `INTEL_TEXT_RIGHT` are now the
constants the renderer lays the line out from *and* the ones
`test_the_report_between_sectors_fits_its_column` measures the table against,
so the words and the column they have to live in cannot drift apart.

The test walks **every** row rather than the six the shipped layout happens to
show, because a sector that later gains a stair door gains its line with it and
must not gain it already too long. Which six are shown is decided by the level
layout rather than by this table — see the note on stair doors and windows
above, and in [intel.c](../src/intel.c) itself.

**And the four fields under the line all mean something now, which two of them
did not.** The sheet has printed TIME, SCORE, HOSTILES and DEATHS since it
existed, and only SCORE belonged to the run: the game showed a player a
stopwatch that could not matter beside a death count that cost nothing past the
walk back. That is a worse arrangement than printing neither, because this page
is downstream of a fiction that will not stop talking about the clock — 01:00
is when the bonds leave the roof, the wall dials climb toward it, the line
after sector eleven says FOURTEEN MINUTES — so the game spent the whole campaign
insisting the night was against a deadline and then put a stopwatch on screen
that the player could safely ignore.

`campaign_award_sector_bonus` ([gameplay_state.c](../src/gameplay_state.c)) is
what pays them, and **the par it measures against is the night clock's own**:
`SECTOR_PAR_SECONDS` is derived from `NIGHT_CLOCK_MINUTES_PER_SECTOR`, so the
134 seconds the dial upstairs gives a floor are the 134 seconds the score
gives it, and moving one moves the other.
`test_the_sector_par_is_the_night_clock_s_own` holds the derivation in place.
Seconds left over pay `SECTOR_TIME_BONUS_PER_SECOND`, a floor cleared without a
death pays `SECTOR_CLEAN_BONUS`, and both are printed under the field that
earned them rather than folded silently into SCORE — a bonus the player cannot
see is a bonus they cannot learn to play for. A sector run well over its slot
is told `OVER PAR` rather than left with a blank line, because a field that
sometimes pays for no visible reason teaches nothing.

**And the field now says what there is to beat.** TIME prints the run beside
`BEST`, which is the quickest any run has ever cleared that sector — see
[What outlives the process](screens.md#what-outlives-the-process). Without it
the par is the night's allowance rather than this player's, and a stopwatch
measured against nothing is a number to glance at rather than one to play for.

The rates are set so that speed is a genuine alternative to clearing a floor
rather than a rounding error on top of it: a full par under is 2680, and a
sector holds eight or so men at 150 apiece. It is also the largest single jump
the score ever makes, which is why `try_finish_current_level` looks for an
extra life immediately after awarding rather than leaving it to the next
sector's first frame — a 1UP that arrives a screen later has arrived for no
reason the player can connect it to.

**All three ways out of a sector pay**, and that is why the award sits above
the branch in `try_finish_current_level` rather than inside the reporting arm
of it: only the stair door draws this sheet, while the window hands straight
over to a climb and the last floor goes to the outro. Banked where it was
easiest to put it, the five climbs and the roof would have been the six floors
in the campaign that paid nothing for being cleared well.

## The net

The crew talk, and now the player can read it. A guard alone calls in, a pair
standing together chat, whoever reaches a wall switch shouts, a thrower four
hundred feet up leans out of a window and shouts down, and the first person out
of the lobby shouts on the way — five beats the game already had as poses and
sounds, and none of which had ever said a word.
The words are in [crew.c](../src/crew.c), one file, and the reason it is one file
is that a strip printing `LENZ` while a manual sheet lists a different twelve
would be two answers to the same question.

**Twelve is the docket, not the body count, and that is a decision rather than
an oversight.** Counted off the maps, the campaign lays out **97 guard spawns**
across the twelve interiors — 84 of them ordinary and 13 heavies — plus
seventeen dogs, plus what the `SPAWNS` lines and a raised alarm send out of the
doors, plus six more in the restrooms — so a finished run has put down comfortably
more than a hundred men, and the fiction says twelve. Most action games get away with never being asked; this one asks
itself, out loud, because the manual's `THE CREW` sheet prints twelve ruled
lines, the report after sector 6 says `TWELVE PLACES LAID IN THE GALLEY. TWELVE
MEN.` and the net counts them. So the stance has to be written down somewhere,
and it is this:

- **The twelve are the men on the docket**, which is Meridian's own paperwork —
  the same document that walked twelve flight cases past a goods entrance
  nobody was inspecting. It is what the building was told, not a census, and a
  contractor who lied about the cases is not a reliable narrator about the
  heads. What the twelve names are actually *for* is the net and the manual
  sheet: a man being shot at is anonymous, a man on a handset has a name.
- **And the roll is deliberately not a citation.** The rule this page sets for
  the homage is that it is paid by describing the films rather than naming them,
  and a crew reproduced name for name is the one place that rule was broken — so
  the five names that identified somebody else's roster were changed, and the
  twelve read as twelve contractors on a docket instead of as a cast list. The
  work they do is unchanged, because it never depended on which twelve they were:
  a man on a handset has a name, and any name does that. A reader who recognises
  the shape of the night is meant to. A reader who recognises the cast was
  reading a copy.
- **Nothing on screen ever adds the two up.** The report between sectors prints
  the *sector's* tally and never a running total (`hostiles_neutralized`, wiped
  with the floor), which is the one thing that would put the two numbers side
  by side and make the player do the subtraction. The run's tally exists —
  `CampaignState.hostiles_down` — but it is read only by the gate on this net
  and is never drawn.
- **And the net no longer contradicts itself about it**, which is the half that
  used to be broken: the lines that name a surviving count now expire, see
  `until_down` below. A crew whose arithmetic is vague is a crew; a crew that
  insists on eleven while the player is standing on thirty is a table of
  strings.

Anything that puts a cumulative kill count on screen breaks this, and it is the
kind of feature that looks harmless — a run summary, a stat on the game-over
card — right up until it prints `97` under a sheet that lists twelve names.

`CHATTER_PANIC` is the odd one out and deliberately so: the people leaving the
lobby are not on anybody's docket, so it is the one kind that prints **without
a callsign** — a name on them would file them as staff the player is meant to
keep track of. Only the *first* of them gets a line
(`civilian_begin_run` in [gameplay_ai.c](../src/gameplay_ai.c)). How many there
are is the map's business — four `f` in [levels/level1.txt](../levels/level1.txt)
as shipped — and deliberately not written down in the code, which is a rule
this page spent a while breaking in both directions at once: the paragraph
above says four and this one used to say five. They break over
`CIVILIAN_STARTLE_SPREAD`, which is shorter than `CHATTER_HOLD_TIME`, so a
room's worth of them speaking would replace the caption before anybody could
read the first — a run of half-second flashes reads as a bug, not as a room
emptying.

**The boundary is the whole design, and it is the same one a sound effect
crosses.** The simulation reports that somebody spoke — `GAME_EVENT_CHATTER`,
carrying a `ChatterKind`, an enemy slot and one opaque number drawn off the
level RNG in `gameplay_crew_chatter` ([gameplay_world.c](../src/gameplay_world.c))
— and holds no string at all. The shell folds the number into a table and
spells it. That is what keeps a hundred lines of flavour text from being a
hundred edits to deterministic gameplay modules: writing a new one changes no
gameplay file, costs the RNG stream nothing, and cannot alter a single seeded
choice. `test_the_net_carries_words` pins it from the simulation's side.

Five rules follow, each of which cost something to learn:

- **A callsign is filed, never drawn.** `crew_callsign` maps an enemy slot to
  one of `CREW_SIZE` names and wraps. Drawing one would both spend a number out
  of the seeded stream — shifting every choice downstream of it — and make the
  same guard answer to a different name on a retry of the same sector, which
  is the one thing a name is for. On the facade there is no enemy array at all
  and the *window index* stands in: the men out there are the same crew.
- **Nobody talks about himself in the third person.** Several lines name a man
  — BRUNO is on the sixth lock, LENZ wants the lights back on, nothing has come
  back from MARCO — and the roll that picks a line and the slot that picks a
  name come from different places, so the two were free to land on the same
  one: the strip printed `MARCO: STILL NOTHING FROM MARCO.` A one-in-a-hundred
  line is still a line somebody reads, and it turns twelve men with a schedule
  into a table of strings being shuffled. `crew_line_said_by` steps forward
  from the rolled index until it finds a line that does not name the speaker,
  which keeps the whole fix out of the seeded stream: the simulation still
  spends exactly one number on a line however this resolves.
  `test_nobody_on_the_net_names_himself` walks every speaker against every
  roll.
- **Nobody says a thing that has not happened yet.** A good half of this
  writing *asserts* something about where the player is — the vault is empty,
  the roof goes in two minutes, nothing has come back from MARCO, he came up
  through the ducts — and the tables are rolled from with no idea of any of it,
  so a man standing in the lobby at 00:22 was free to report all four before
  Chuck had climbed a floor or touched anybody. These lines are the version of
  the plot a player who never opens the manual actually gets, so arriving out
  of order is the same failure as a report between sectors that spoils its own
  ending, only shuffled. Each row carries the earliest sector it may be heard
  in and how many of the crew have to be down first (`CrewLine` in
  [crew.c](../src/crew.c)), and the shell hands over a `CrewSituation` — the
  campaign's sector, never the restroom's, and the **run's** tally of the
  crew rather than the floor's, because a crew does not get its men back
  because the player opened a stair door. Gate for what a line asserts and
  never for flavour: when in doubt a line goes ungated, because a thin table in
  the early sectors is a worse failure than an eager line in a late one. It
  resolves by the same walk forward from the rolled index the name rule uses,
  so the simulation still spends exactly one number however both filters land.
- **And nobody says a thing that stopped being true twenty men ago**, which is
  the same rule read from the other end and was missing for a long time. A
  handful of these lines name an *exact* number of the crew still standing — `I
  COUNTED ELEVEN OF US TONIGHT`, `WHERE ARE THE OTHER ELEVEN?`, two named men
  not answering — and a gate with a floor and no ceiling let every one of them
  survive into a run with thirty guards down, where they are exactly as false
  as they were in the lobby and a good deal more conspicuous, because by then
  the player is the one who has been counting. A line that names a number is
  true over a **window**, and `until_down` is that window's far edge. It is
  only ever set on a line that counts: the rest of this net is a crew's opinion
  of the man coming up the stairs, and an opinion does not go stale — `IT IS
  ONE MAN. HOW MUCH TROUBLE IS ONE MAN?` gets funnier with the tally, not
  falser. `test_the_net_always_has_something_to_say` pins every kind having
  something sayable in every sector **with nobody down and again with far more
  down than the crew ever had** — a ceiling that empties a table is the same
  failure as a floor that empties one, and it arrives in the half of the
  campaign the player is fighting hardest through — and it pins that every
  single line is sayable at *some* moment of *some* run, which is what catches
  both a `from_sector` past the end of the campaign and a window whose ceiling
  sits at or below its own floor.
- **Earshot is shorter than hearing.** `CHATTER_EARSHOT` is eleven tiles
  against the sixteen a routine sound carries (`audio_play_at`), and it is
  measured off the same listener, so the words and the voice can never
  disagree. A sound from off screen is a cue about somewhere else; a *sentence*
  from a man the player cannot see is the game subtitling thin air. Eleven
  tiles is 352px, inside the 400px half of the viewport, so the speaker is on
  screen whenever the camera has caught up.
- **It is a strip, not a speech bubble.** A guard is twenty-six pixels across
  and about to walk out of frame; the plate stays under the HUD and names him
  instead. Its accent is the palette's own semantics rather than four decorative
  colours — cyan is the handset, red is the alarm, amber is a voice on the wall,
  and two men talking in a room get no accent at all, because nothing is
  happening. It stands down for the unlocked-exit banner, which lands in the
  same band and owns the frame.

`CREW_LINE_MAX` is the one hard constraint on the writing: a callsign, a colon
and the line have to fit inside 800px of 8x8 cells, and
`test_crew_traffic_fits_the_plate` fails the build rather than letting a line
run off the edge of a frame where nobody would ever see it.

**What the lines are for.** The building is a homage — one man, one tower, one
night, a crew of twelve on a contractor's docket — and the net is where that is
allowed to be funny. Every line is written to survive twice: as something a
tired armed man would actually say at half past midnight, and as a nod for
whoever has seen the films this building came out of. A line that only works as
the second is a joke told over the game rather than in it, and does not belong
in the table. The plot lines and the jokes share the same tables on purpose —
`RADIO_LINES` is where the locks, the cordon and the tally get said out loud,
so a player who never opens the manual still hears the story.

**And "survives twice" means the first reading has to be about the man actually
on screen.** Two lines used to have the crew joking that Chuck was not wearing
shoes, which is a joke about a different man in a different building: Chuck is
drawn in boots, deliberately and by name — `boot_rear` / `boot_front` through
`sprite_shoe` in [render_figures.c](../src/render_figures.c), under a comment
reading *"Boots, not holes."* A nod that contradicts the sprite is a nod told
over the game, and it is the exact failure the rule above names, so it went the
way the rule says. What replaced it keeps the pair and the running gag — a man
on the handset explaining that a cop would have waited for backup, and a
thrower four hundred feet up shouting `STILL NO BACKUP!` — and both are now
*true*: the cordon is facing out and nobody is coming, which is the whole point
of the 00:04 broadcast.

## The credits

The outro holds its thank-you frame for the rest of `OUTRO_CUTSCENE_DURATION`
and then hands the frame to `STATE_CREDITS`: the names rise through the same
letterboxed, grainy frame the rescue ended in, and when the roll runs out the
game is back on the title screen. That last part is why it is a state and not a
card bolted onto the outro — a finished campaign used to park on a frame the
player had to dismiss. Confirm means "get on with it" while the names are
moving and "done" once they have stopped; `ESC` leaves at any point, and the
pad's B is inert here exactly as it is during a cutscene.

**The table is the roll.** [credits.c](../src/credits.c) links no SDL, for the
same reason [crew.c](../src/crew.c) and [settings.c](../src/settings.c) do not: it is
a table of typed lines — `CREDIT_TITLE`, `CREDIT_ROLE`, `CREDIT_NAME`,
`CREDIT_NOTE`, `CREDIT_RULE`, `CREDIT_GAP` — plus the height and the scale each
kind stands at, and `draw_credits_roll` in [game_render.c](../src/game_render.c)
walks both, exactly the way `draw_settings_sheet` walks the options table. A
line added here costs no layout, and `test_credits_fit_the_frame` measures
every one of them at its own scale, so a line too wide for the frame fails the
build rather than running off the edge of the one screen nobody plays through
twice. It pins the clock as well: a roll that never comes to rest never reaches
the title screen.

**One name, said six times.** Chuck is written by one person, and the roll says
so by naming six genuinely different disciplines and answering all six the same
way — the repetition *is* the credit, not padding. The crew's docket has twelve
names on it and the manual's `THE CREW` sheet prints all twelve; this one has
one, and the roll says that out loud (`TWELVE NAMES ON THEIR DOCKET. / ONE ON
THIS ONE.`) rather than leaving it to be noticed. The homage is thanked the way
the net does it, by describing the films rather than naming them.

**It is still the same film.** The roll keeps the outro's two letterbox bars
and its film grain, and stands on a skyline of its own: silhouette blocks, dim
windows, Kessler Tower still the tallest thing out there with its roof beacon
still turning. A roll on flat black would be the one screen in the game that is
not lit — the same objection the manual's sheet answers — but the type is what
this screen is for, so every value out there is kept under it.

## Things that are only true tonight

The building's dressing — desks, counters, planting, restroom fittings — was
here yesterday and will be here tomorrow. A second, much smaller set says what
is happening *this* night, and it exists because the plot is otherwise told
only in places where the player is not playing: a cutscene, a report between
sectors, a sheet of the manual. Everything in this set is presentation, none of
it is collectable, solid or simulated, and none of it tells the player anything
they need in order to finish a sector. The rule for adding to it is that it has
to say something the story page says, in the place the player is standing.

- **The clock, `w`.** At 01:00 the bonds leave the roof; that is the only
  reason any of this is happening tonight. The dial reads the campaign sector it hangs in
  (`NIGHT_CLOCK_*` in [game_config.h](../src/game_config.h)), so the minute hand
  climbs the face across the seventeen sectors and is nearly back at the top on
  the roof. It is the **one prop that hangs**, so it asks the tile above it for
  support where every other prop asks the tile below (`decoration_hangs` in
  [level.h](../src/level.h), and `editor_symbol_hangs` on the editor's side of the
  same rule); `test_night_props_ask_for_the_right_wall` pins both directions.
  The second hand is not decoration on the decoration — a dial with two static
  hands is a painted clock, and a painted clock says nothing is happening.
- **The flight case, `m`.** The manual's `THE NIGHT` sheet is illustrated with
  the case Meridian wheeled in through the goods entrance, and this is that
  case at a fourteenth of the size, in the sectors themselves. Half of them
  stand shut; half lie open with a rifle-shaped hole in the foam, which is
  where the rifles, the frags and the rocket the player keeps picking up came
  from. Placing one two tiles from a `Z` is the cheapest sentence of plot in
  the game — and every interior sector has one now, because the prop used to
  stop at sector 8 and the sectors it skipped are the ones where the plot is
  actually escalating. A sector that hands the player a weapon and shows them
  nothing it came out of is a sector telling them the armoury is the level
  designer's rather than Meridian's.
- **The radio check.** A pair of guards standing together already chat
  (`ENEMY_TALK_*`); one on his own calls in on the crew's own net
  (`ENEMY_RADIO_*`, `update_radio_checks` in
  [gameplay_ai.c](../src/gameplay_ai.c)), because twelve men badged into one
  building under one contractor name are a crew running a schedule. It reuses
  the chat wholesale and **is** a chat with no partner — `enemy_on_radio` in
  [enemy.h](../src/enemy.h) derives the pose and the sound from that rather than
  storing a second flag, so every path that ends a chat ends this too. One
  thing about it is deliberate and tested: a chat blinds a guard past
  `ENEMY_TALK_NOTICE_RADIUS` and **a radio check does not**. An ambient beat
  that quietly hands the player a stealth window is a balance change wearing a
  costume. What the call actually *says* is [The net](#the-net) below.
- **The alarm reddens the room.** While the alarm is up, every ceiling fixture
  in the sector and the pool it throws on the floor swing between the theme's
  own lamp colour and the emergency circuit (`alarm_wash` in `render_world`).
  The ambient bounce is deliberately left alone: repainting that as well floods
  the frame and takes the level's material with it.
- **The cordon.** The demand broadcast at 00:04 put every unit in the city on a
  ring around this tower and nobody at all inside it, and the player crosses
  that ring twice. On the drive in, junctions fill up with squad cars standing
  on the pavement with their bars lit, more of them the nearer the route gets
  to the building (`cordon_side` in [chase.h](../src/chase.h), drawn by
  `draw_cordon_car`); they stand in the cross street beyond the pavement and
  never in a lane, because a car the player's own car drives straight through
  is a bug rather than a detail, and
  `test_chase_cordon_thickens_toward_the_building` pins both the ramp and the
  quiet first blocks. **That ramp is distance from the tower, not time**, so
  the whole ring is already standing when the drive starts — which is why the
  broadcast is timed eight minutes ahead of the abduction rather than after it.
  `render_cordon_caption` in [chase_render.c](../src/chase_render.c) names it once
  on the road, because otherwise the most legible object out there is scenery
  the drive never explains and the player reads as traffic to dodge. Out on the wall it is the same cordon from above:
  `facade_cordon` in [level_art.c](../src/level_art.c) washes the lower face in
  out-of-phase blue and red, strongest on the first and lowest climb and gone
  entirely by the last two, so the climb is also a climb away from it. **That
  fade is keyed on the sector's theme and not on its backdrop**, which is the
  one thing about it worth writing down: `FACADE_SLEET` borrows the storm's
  backdrop, so asking the backdrop gave the highest climb in the game the
  storm's own wash and put more street under sector 15 than under the two
  climbs below it. Four backdrops, five climbs; anything that means *per climb*
  asks the theme.
  `facade_news_helicopter` is the one aircraft allowed near the tower — a
  **news** ship, drawn behind the shell so it passes *behind* the face Chuck is
  on, because the helicopter waiting on the roof at the end of the night is the
  crew's ride out and nothing in the sky may contradict it.

Two colours are owned outside the palette for this, both named once with the
reason: `CORDON_BLUE` in [level_art.c](../src/level_art.c) and `COL_BEACON_BLUE`
in [chase_render.c](../src/chase_render.c) are the same emergency-beacon blue,
which `FX_CYAN` (technology) and `FX_LAMP` (a fluorescent tube) cannot supply.
The red half of a light bar is `FX_RED`, which is exactly what the palette's
danger red is for.

## People who are not in the fight

Three kinds of NPC live in the gameplay core without taking part in it: the
ambient janitor (`J`), the fleeing civilian (`f`) and the receptionist (`k`),
all in [gameplay_ai.c](../src/gameplay_ai.c) beside the guards. They collide with
the static map so they stay grounded, and that is the whole of their contact
with the simulation — no perception, no damage, no collision against the
player, no scoring. Guards do not see them and bullets pass through them. Keep
it that way: the moment one of them can be shot or can block a route, every
level holding one has to be re-solved.

Level 1 is the lobby they walked Ellen through, so it empties as Chuck walks
in: four civilians freeze, shout and run for the tile the player started on —
the street entrance — dissolving into the doorway rather than stepping out of
frame, because the entrance itself is painted on a parallax layer
(`lobby_entrance`) and nothing in the world plane can line up with it for long.
The part plays once, at `gameplay_ai_spawn_level_entities`, and stops mattering
a few seconds later. Their shouts go through the ordinary event buffer, so the
tests can assert on the evacuation without any audio.

The receptionist is what the room has left once they have gone, and is the one
ambient NPC with a place to be rather than a route to walk: a post at the
counter, an errand two to five tiles out every ten seconds or so, and a walk
back. Every target is measured from `post_x`, never from wherever the last walk
stopped, so the desk is still staffed after ten minutes in the sector instead
of empty with someone standing in the next room —
`test_receptionist_works_a_post_and_returns_to_it` pins the round trip. Drawn
on the same layer as the janitor, so the `n` counter renders over the post and
the staff side of the desk stays legible.

**She belongs to the lobby and to nothing above it**, and that is the fiction
rather than a limit of the code. A `k` stood on the penthouse floor of sector 14
for a while, which put a civilian calmly working a counter three sectors below
the roof at 00:51 — half an hour after the building emptied, on a floor
twelve armed men have been running all night, ignored by every one of them and
by Chuck. The counter itself may stand anywhere: an `n` run is dressing, it was
here yesterday and will be here tomorrow. The person behind it is not, and the
only sector she can still be in at 00:22 is the one the crew walked Ellen
through.

**The janitor is deliberately not held to that**, and the difference is worth
stating because the two read as the same objection: a `J` stands in sectors 2,
5, 6, 9 and 10, which is a civilian still working at around 00:44, after the
shooting has started. What separates them is what a post is. A staffed counter
is the building *serving somebody* — it says the night is normal, and it cannot
say that once the lobby has been emptied at gunpoint two floors below. A
janitor is one man alone on a floor with a cart, and nobody told him anything:
the crew took the lobby and the controller, not the whole night shift, and the
building's own alarm is only up when the player puts it up. He is the last
person in the tower who does not know, which is the point of him rather than a
hole in it. Either way it is now a decision on this page instead of an
absence — `k` is confined and `J` is not, and both are written down.

**And the one room he may not stand in is the lobby**, which is the same
sentence read backwards. What licenses a janitor anywhere above is that nobody
has told him; sector one is the one floor where somebody does, out loud, in
`CHATTER_PANIC` — four `f` break for the street shouting `THEY HAVE GUNS!
EVERYBODY OUT!`, and a `J` stood one tile from the nearest of them, five tiles
from where Chuck walks in, mopping. That is not a man who has not heard, it is a
man ignoring a room being emptied at gunpoint, and it undercuts the rule the
other four are allowed by. He is gone from
[levels/level1.txt](../levels/level1.txt); the lobby keeps five ambient figures
without him, which is still more than any other sector has. So the list above is
the list: **2, 5, 6, 9 and 10**, and the lobby is the exception with a reason
rather than a gap waiting to be filled in.

**Sector 10 is the one that was added afterwards, and it is the one that argues
the rule rather than following it.** The other four sit in the first half of the
night, so the building's ambient life stopped dead at the halfway point — from
the security floor upward there was nobody in the tower who was not either
fighting or being fought, which reads as the game forgetting the fiction exactly
where the fiction is doing its most work. A man with a mop on the monitor-wall
floor at 00:44 is the least likely place to find one and therefore the most
useful: what licenses him is the same sentence as before — nobody told him — and
the floor he is on is the one place the player has just learned somebody
*would* have.
