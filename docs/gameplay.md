# What the simulation does

## Hearts, damage and the two real deaths

The player has hearts (`Player.hp`, `PLAYER_MAX_HP`), and the rule is one
sentence: **what hits you costs hearts, what crushes you or breaks your fall
kills you.** Ordinary contact — a guard, a bullet, a bite, spikes, a fan, a
brick, a bird — goes through `gameplay_damage_player`
([gameplay_world.c](../src/gameplay_world.c)): one heart (explosions cost
`EXPLOSION_DAMAGE`), a `PLAYER_HIT_INVULN` mercy window, and a vertical pop
away from the source (vertical only, because the walk speed is rewritten from
input every frame; on a ladder or the facade even that is skipped). Only a
fatal fall and an elevator crush still call `gameplay_hit_player` directly,
and so does the last heart. The mercy window reuses `invuln_timer`, so every
damage source already respects it and the renderer already blinks it.

Two consequences worth knowing. The spike pop is what lifts the boots back
out of the spike bed, so one misstep costs one heart rather than locking into
a loop. And a dog's bite is announced: the first contact only starts a
`DOG_BITE_WINDUP` crouch-and-growl, the teeth land a beat later if Chuck is
still there, and stepping clear cancels the lunge (the windup ticks, and is
cancelled, in the dog's AI update, which owns `dt`).

A guard downed in direct combat — bullet, knife or stomp — drops a magazine
(`gameplay_spawn_ammo_drop`, `AMMO_DROP_BULLETS`); explosions destroy it with
its owner. The drop is only collected while the sidearm is short, so it waits
on the floor instead of vanishing into a full clip.

**A body stays, and it has to, because the AI already reads it.**
`update_body_discovery` sends a calm guard who sees a fallen comrade over to
look and often on to the nearest alarm switch. Nothing drew the bodies, so the
player watched a man cross the room to an empty patch of carpet and wake the
building: a rule that is simulated, documented and punishing, whose whole
trigger was invisible. `draw_downed_enemy` / `draw_downed_dog` in
[render_figures.c](../src/render_figures.c) lay the same figure along the floor — dead
visor, no health pips, no speech bubble — and three consequences follow.
`settle_body` in [gameplay_ai.c](../src/gameplay_ai.c) drops a body that died in
mid-air, because one hanging in the air is also a guard investigating thin air —
and it falls the way a *climber* falls, since `level_move` makes every rung a
one-way platform for anyone who is not climbing and a guard shot halfway up a
shaft was therefore caught by the next rung down and left lying across the
ladder in the open air. Only the rungs are transparent to a body; solid tiles,
falling panels and moving platforms all still catch it.
`find_enemy_slot` takes a **fresh** slot before a dead one, so a reinforcement
no longer deletes the corpse standing in front of the player — and only when the
array is full does it take the body furthest from Chuck. And the kill tally
moved off the `dead` flags onto `GameplayState.hostiles_neutralized`, counted as
each one goes down: the flags are the population still standing, so reading them
lost one kill per reused slot and the report between sectors under-credited the
floor the player had actually cleared.

**One man down is counted twice, and the two counts answer different
questions.** `gameplay_record_neutralized` bumps the sector's tally *and*
`CampaignState.hostiles_down`, which is the **run's**, and it is the one place
either is touched. The sector's is what the report between floors prints, so it
is wiped with the sector; the run's is what the crew's own net reads — twelve
men who have noticed how few of them are answering — and it has to outlive the
floor they were lost on, so it sits beside the score, which is the other number
that belongs to the run rather than to the floor. See
[The net](story.md#the-net).

**And a guard is owed one walk over per body, not one per sector.** The
discovery was latched by a single `bool` on the guard, which did the job it was
written for — it stopped him re-triggering every frame he stood beside the same
corpse — and then kept going: having looked at one body he was blind to every
other for the rest of the floor. So the rule switched itself off at exactly the
point a player starts leaving bodies about, and on a sector with ten guards on
it the first kill disarmed the mechanic for all of them. `Enemy` carries a
`bodies_investigated` mask now, one bit per corpse (`enemy_body_bit`, guards
first and dogs after them, with a `_Static_assert` that they fit in a word), so
the body he has already dealt with stops calling him and a second one lying
somewhere else still does. A bit is released again by `find_enemy_slot` /
`find_dog_slot` when the slot behind it is handed back to a live body —
otherwise the mark would belong to the reinforcement standing in it and would
silently cancel the walk over for whoever finds *him*. A chain through several
corpses is the right answer and terminates on its own: each is marked as it is
taken, `another_guard_is_raising_alarm` keeps the roll to one man at a time, and
the moment anybody reaches a switch the alarm is up and the whole function
returns at the top. `test_a_guard_notices_the_second_body_as_well` pins it.

**Only the magazine comes back.** `ITEM_GUN` is the one pickup on
`ITEM_RESPAWN_TIME` (`gameplay_collect_items` in
[gameplay_interaction.c](../src/gameplay_interaction.c)), because the sidearm is
what a sector is played with and a player who has spent it must not be left
walking the rest of the floor with a knife. The grenade used to regrow with it,
which made a single `N` an unlimited supply at ten seconds apiece — enough to
clear a floor a blast at a time, and enough to open every `%` in the campaign
without the bazooka those patches were placed for. A one-shot explosive that
regrows is not a decision about when to spend it;
`test_only_the_magazine_comes_back` pins both halves.

**And nothing that cannot come back is spent on a counter that is already
full.** This is the rule `gameplay_update_ammo_drops` has always kept for the
magazine a downed guard leaves — *"Left lying until it is actually useful, so a
full magazine does not eat the pickup"* — and it was missing from all three of
the boxes on the floor that get no second chance. Walking over a second `N`
while carrying a grenade set `collected` with a nought respawn timer and played
`SFX_PICKUP_GRENADE`: the scarcest thing in the sector destroyed by crossing a
tile, announced with the sound of a successful pickup. It was not a corner —
sector 12 carries two grenades, sectors 10, 12 and 15 two medkits apiece, and
**every restroom hands out the grenade the campaign's own budget is balanced
on**, so a player who took the detour still holding one paid for it with
nothing. `item_would_be_wasted` in
[gameplay_interaction.c](../src/gameplay_interaction.c) leaves all three where
they are, to be collected on the way back once they are worth something.

The magazine is deliberately *not* on that list, and the difference is the
respawn: `ITEM_GUN` comes back, so taking one on a full clip costs the player
nothing and the box is there again before it is wanted. Nothing else gets a
second chance, which is exactly why nothing else may be spent for nothing.
`test_a_pickup_that_would_be_wasted_is_left_alone` pins both sides.

**A grenade and a rocket survive the way out of a sector, and nothing else
does.** `player_begin_sector` ([player.c](../src/player.c)) is the rule and
`load_level` is the only caller; the sidearm does not travel because
`player_reset` hands over a full clip either way, and the weapon *in the hand*
deliberately does not travel either, because "a pickup never arms itself" is a
rule about a doorway as much as about a floor tile.

**The facade is why it exists.** Nothing on a climb can be thrown or fired at
all — `update_facade_playing` clears `shoot` for the whole of it and
[gameplay_climb.c](../src/gameplay_climb.c) has no notion of a weapon — so the `N`
standing mid-wall on each of the four climbs is a pickup whose entire value is
in the sector above it. Wiped at the doorway, as it used to be, that was a
detour paid for in wind and thrown bricks that bought nothing whatever, and the
fourteen grenades the campaign lays out included four that could never be
spent. The rule lives in the SDL-free half precisely so the suite can hold it:
`load_level` is shell code the tests link no SDL to reach, which is how a
doorway went that long unexamined. `test_a_sector_hands_its_explosives_to_the_next`
pins what crosses and what does not, and
`test_every_climb_carries_an_explosive_out` pins the reason.

**And the same rule read the other way says what a climb must *not* carry.** All
four used to lay out a `G` as well, which is the one pickup a wall can do nothing
with whatever: it cannot be fired up there, and it does not cross the doorway
either, because the clip the next sector opens with is a full one however this
one ended. So walking over it changed no counter, paid no score, and played
`SFX_PICKUP_AMMO` — the sound of a pickup that worked. That is precisely the
failure `item_would_be_wasted` exists to stop for the grenade, the rocket and the
medkit, and `ITEM_GUN` is exempt from it for a reason that only holds indoors:
the box comes back on `ITEM_RESPAWN_TIME`, which costs a player on a *floor*
nothing and buys a player on a *wall* nothing.
`test_no_climb_lays_out_a_pickup_it_cannot_use` keeps the map honest instead, so
the four boxes cannot quietly return; [levels/LEGEND.md](../levels/LEGEND.md)
carries the authoring half of it.

**And every pickup answers, the live key card included.** The unlock fanfare
belongs to the door, so `gameplay_unlock_exit` stays silent when there is no
door to open — an interior whose stair core is welded and whose way on is the
window, or a sector where a finished hack has already opened it. Read as the
card's only voice, that made the *right* card the one pickup in the game that
made no sound at all, in a sector where the strip reads BLOCKED and cannot
report it either, while a decoy still buzzed `SFX_CARD_WRONG` — the feedback
exactly backwards, and the kind of thing a player reads as their own mistake.
The live card now plays `SFX_CARD_SCAN` whenever the door's own sound did not
fire. No shipped map puts a `C` in a window sector, which is the only reason
nobody has heard it; `test_the_live_card_is_never_silent` is what stops the
next map finding out.

**And picking a weapon up is not deciding to use it.** Nothing the player walks
over changes what is in their hand, and no weapon the game puts there by itself
is ever a one-shot explosive. Both halves used to be the other way round: an
`N` armed itself on contact, so crossing a floor mid-firefight with the trigger
already on the pistol threw the scarcest thing in the sector at whatever
happened to be in front of Chuck, and firing the last rocket walked the cycle
straight onto the grenade so the follow-up shot threw that too. So
`gameplay_collect_items` sets the count and nothing else, and every place a
weapon is spent calls `player_fall_back_to_sidearm`
([player.c](../src/player.c)) — the pistol if it is loaded, the knife if it is not
— rather than `player_select_next_weapon`, which is now the bumpers' alone. The
HUD already draws the grenade and the rocket whether or not they are raised, so
carrying one is still visible; the bumpers are how it reaches the hand.

There is **one exception, and it is the one that costs nothing**: a magazine
picked up while Chuck is holding the knife with a dry clip raises the sidearm,
because an empty clip is the whole reason he is holding a blade and that is the
same sentence "only the magazine comes back" is already written to finish. A
knife chosen on purpose, with rounds still in the clip, is not that case and
survives the pickup. `test_a_pickup_never_arms_itself` pins the rule, the
fall-back and the exception together.

## Stomping a guard

Walking into a guard costs a heart, but `gameplay_combat_check_contacts`
([gameplay_combat.c](../src/gameplay_combat.c))
carves out one free answer: landing on its head. It tells a stomp from a side
collision without swept collision by comparing penetration depth on each
axis — a falling player (`vy > 0`) whose vertical overlap with the guard is
shallower than the horizontal overlap only just tagged the top of the box, so
it bounces Chuck upward (`ENEMY_STOMP_BOUNCE_SPEED`) and calls the same
`damage_enemy` a bullet or knife hit would, instead of hurting him. Dogs are
unaffected; only guards can be stomped. The bounce also clears
`jump_cut_ok`, because it is not a player-started jump: releasing the jump
key must never shorten it back down into the guard.

**A shallow overlap does not say which of the two is on top**, only that the
boxes just met on that axis, so the test also asks that Chuck's centre is above
the guard's. Without it, jumping up into a man standing on the ledge overhead
satisfied everything else the moment the rise turned into a fall: a head
butting a pair of boots read as a boot landing on a helmet, and wounded him for
it. `test_a_stomp_has_to_come_from_above` pins it.

A stomp lands mid-climb as often as mid-jump, and that case needs its own
fix: the ladder branch of `player_update` ([player.c](../src/player.c)) sets `vy`
from the climb input every frame, so a bounce set while `on_ladder` is true
would be overwritten the very next frame by the climb speed, driving Chuck
back down into what now reads as a deep side hit. The stomp handler
also clears `on_ladder` and arms `ladder_lockout_timer`
(`ENEMY_STOMP_LADDER_LOCKOUT`) so the ladder cannot be re-grabbed until the
bounce has had time to actually clear the guard.

## One blast, one rule

Four things explode — a mine, a grenade, a rocket and a gas canister — and they
differ in exactly three ways: where they go off, how far they reach, and how
hard they shake the frame. What a blast *does* is one function,
`apply_blast` in [gameplay_combat.c](../src/gameplay_combat.c), and each
explosive's own code is now the event, the sound, the shake and a call to it
with its radius.

They used to be four hand-written copies, and every one of them had drifted
somewhere different: a rocket set off a gas canister but a grenade landing
against the same canister did nothing, and a mine brought a wall down without
troubling the guard standing in the hole it had just made. A blast that picks
which of the things beside it are real is a blast the player cannot reason
about, and none of those gaps were anything a player could have predicted from
the ones that worked. `test_every_blast_reaches_the_same_things` pins the two
that were missing.

Three properties of the shared rule are worth knowing. A guard taken by a blast
leaves no magazine — the drop belongs to direct combat
(`gameplay_spawn_ammo_drop`), and an explosion destroys it with its owner. The
player can only be hurt once however many blasts a chain sets off, because the
first one opens the mercy window that `gameplay_damage_player` checks on entry.

And **everything explosive in reach goes off with it**, which is the third of
those and the one that had to be learned three times. Only the canister chained
for a long time, so a rocket fired into a mined corridor cleared the canisters
and stepped over the mines, and a grenade lying where it had been thrown
survived the blast it was thrown into — a list of what chains rather than a
rule, and nothing on screen said which of the three the player was looking at.
Fixing that left **the rocket** off the list, which is the same bug one item
shorter: a warhead crossing a fireball is a charge sitting in it, and with
`MAX_ROCKETS` at one the exception was rare enough to survive precisely because
nobody could trip over it. All four chain now. Every chain still terminates for
the reason the canister's always did: each charge is deactivated *before* its
own blast is applied, so the set of live explosives strictly shrinks and nothing
can re-enter through its own radius. `explode_gas_canister`, `explode_mine`,
`explode_grenade` and `explode_rocket` each open with the same `active` guard,
which is what makes that safe to say — and the rocket's is what lets its own
impact test and a chain both call it in one frame.
`test_a_blast_sets_off_every_charge_it_reaches` pins all four.

Only the player's weight *arms* a mine, but the delay between the step and the
blast is long enough to run out of — and long enough for whoever is chasing him
to run into. A blast is the other way to set one off, and it does not wait out
`MINE_TRIGGER_DELAY` or care whether anybody ever stepped on the thing: the
delay is the beat between a boot and the bang, and there is no boot in a
sympathetic detonation.

## Forgiving input, checkpoints, continues

The jump is deliberately forgiving, and all of it lives in `player_update`:
a `PLAYER_COYOTE_TIME` window keeps a ledge jumpable for a beat after the
boots leave it, a `PLAYER_JUMP_BUFFER` keeps a press alive until the boots
arrive, and releasing the key mid-rise caps the climb at
`PLAYER_JUMP_CUT_FACTOR` of the jump speed (only for rises the player
started — `jump_cut_ok` — so stomp bounces are never cut). Whether a press is
honoured, now or a few frames later, is the player module's decision rather
than the input layer's, and `Player.jumped` reports the frame a jump actually
started so the shell can play the sound. Tests pin all three.

**A ladder needs a jump key that is not the climb key.** `UP` is the
keyboard's jump everywhere except over a ladder, where the same key has to
mean climb — which left the keyboard as the one input that could not take
`player_update`'s jump-off-the-ladder branch at all, while the pad had it all
along under A. `LSHIFT` is that key: it reports the press unconditionally, the
way the pad's A does, and it is read into `jump_held` as well or every jump
started on it would be cut back to a hop on the very next frame. The rule it
restates is the one above — the input layer names presses, the player module
decides what they mean.

**And the rung has to stay let go of for a beat afterwards**, which is the
other half of the same sentence and was missing from it. The ladder branch sets
`vy` from the climb input every frame, and the grab only asks that the box is
over a rung with up or down held — so a jump taken *while climbing* was let go
of and caught again on the very next frame, and the climb speed wrote the jump
straight back out. Holding up and pressing jump therefore did nothing at all:
the separate jump key exists precisely because `UP` is the climb over a ladder,
so the player pressing it is nearly always already holding up, and the one way
of asking for this jump that comes naturally was the one way that was ignored.
`PLAYER_LADDER_JUMP_LOCKOUT` arms the same `ladder_lockout_timer` a stomp
bounce already arms (see [Stomping a guard](#stomping-a-guard)), for the
identical reason, and it is short on purpose — about a tile and a half of
rise — so a jump up a shaft catches the ladder again on the way and reads as a
boost rather than as the rungs going dead.
`test_a_jump_off_a_ladder_survives_a_held_climb_key` pins both ends of that.

**And getting *onto* a ladder from the tile above it is a snap, not a press,
because otherwise the two halves of the rule fight each other for ever.** A
player standing on the top edge overlaps no rung — his feet are on the tile and
the ladder is under them — which is why `descend_from_top` samples below the
box instead of through it. But it also requires `on_ground`, and the first
thing climbing does is take `on_ground` away. So the grab gets exactly one
frame to carry the box far enough down for `player_over_ladder` to take over,
and at `SIM_STEP_DT` it cannot: `PLAYER_CLIMB_SPEED` covers 0.42px of the 1px
it needs. On the frame after, neither predicate holds, the grip is released,
and `level_move` — called with `climbing` false now, so the rung is a one-way
platform again — catches the fall and returns him to the pixel he started on.
Then it happens again, sixty times a second. Every ladder in the game was
one-way: up worked, down was a man juddering on the spot.

So the mount snaps the box `LADDER_TOP_GRAB_OVERLAP` into the rung, exactly as
it already snaps him horizontally into the rung's column on the same line, and
for the same reason — a few invisible pixels are what make the move catch from
wherever the player happened to stop.

**The reason this survived so long is the reason it is worth writing down.** It
is a *distance*, not a rule, so it only exists at some step sizes: at 1/60 the
same climb covers 1.67px and latches on the first frame. Every test written
against a hand-picked 1/60 therefore saw a working ladder, on every map, while
the shipped 1/240 game was deadlocked on all of them — the exact hazard
`test_the_jump_apex_does_not_depend_on_the_frame_rate` exists for, arrived at
from the other side. `test_every_ladder_in_the_campaign_can_be_climbed_down`
walks every top rung in every shipped sector, from five approaches, **at
`SIM_STEP_DT` and three rates either side of it**. Anything that integrates
against `dt` and then tests itself at a rate of its own convenience is not
tested.

Progress is banked and a death resumes at it. Facade climbs bank height every
`FACADE_CHECKPOINT_STEP`; interiors bank at real progress — any key card, a
finished hack, a teleport door, a medkit (`gameplay_bank_checkpoint`, called
from [gameplay_interaction.c](../src/gameplay_interaction.c)) — and
`gameplay_restore_checkpoint` handles both modes, clearing whatever is in the
air that could land on the man who has just been put back — enemy bullets
inside, thrown bricks and birds on the wall. What Chuck himself threw is left
alone deliberately: it is part of the world he changed, and the respawn's own
`INVULN_TIME` already covers him. A death keeps the carried grenade and rocket
(`finish_player_death` transfers the loadout across `player_reset`), refills
the sidearm, and never reloads the level, so the world keeps its dead guards
and opened walls.

Running out of lives always offers a retry of the current sector:
`campaign_begin_continue` no longer gates on the continue count. Continues
are the score insurance — while one is left the retry keeps the score, after
that it costs it (`campaign_accept_continue`). **That last retry is a way a
run ends**, and `continue_game` banks the score before taking it: the zeroing
never reaches the game-over card, the outro or the pause sheet, so left out it
meant letting the countdown expire kept the record while accepting the retry
the prompt was offering destroyed it — silently, and in the player's favour
nowhere. The campaign never returns to level one uninvited. The score itself
now pays out: every `EXTRA_LIFE_SCORE_STEP` points is an extra life
(`campaign_check_extra_life`, polled once per playing frame).

ESC (or START on a pad) pauses — `STATE_PAUSED` holds the interrupted state
in `Game.pause_return_state` and resumes it directly, never through
`game_enter_state`, which would replay `STATE_LEVEL_START`'s reveal.
Pause is a menu of three (`PauseItem`): resume, the options sheet, and
abandoning the run. **The cursor opens on RESUME every time**, and that is a
rule rather than a default — a menu that remembers where it was left is a menu
where the next press of confirm might be the one item on it that cannot be
taken back, so ABANDON RUN is last, set in the danger red, and never under the
thumb on arrival. `Q`/BACK still abandons directly, which is the deliberate
second step it always was. The reveal, the key-card sweep and the game-over
hold all accept confirm to skip.

## The facade climb

Levels flagged `MODE FACADE` are climbed, not walked
([gameplay_climb.c](../src/gameplay_climb.c)). Four things make the wall a route
rather than a straight line up, and each is tested:

- **Masonry.** `#` tiles are stone cornices the climber collides with
  (axis-separated, so he slides along a ledge instead of sticking to it). They
  are also cover: thrown objects shatter on them and birds break off against
  them. Because the player box is exactly one tile tall, a lone solid tile
  inside a two-row band would seal the band — see
  [levels/LEGEND.md](../levels/LEGEND.md); plant is painted on cornices instead.
- **Wind.** One building-wide phase machine (calm → warning → gust) seeded from
  the level RNG. The warning beat plays `SFX_WIND_GUST` and pushes nothing; the
  gust pushes sideways unless a solid tile within `FACADE_WIND_SHELTER_REACH`
  upwind of Chuck breaks it, which is what makes the shelters worth using.
- **Telegraphed throwers.** An `r` source shouts and leans out for
  `THROWN_OBJECT_WINDUP` before releasing, so every brick can be answered.
- **Checkpoints.** Height is banked every `FACADE_CHECKPOINT_STEP` at a
  position Chuck actually held, and a lost life resumes there
  (`gameplay_climb_restore_checkpoint`, called from `finish_player_death`).

`update_facade_playing` also runs `gameplay_collect_items`, so pickups on the
wall are real detours — and the explosive among them is spent in the sector
above, because **nothing out here can be thrown or fired**: the branch clears
`shoot` every frame and this module has no notion of a weapon. That is the
whole reason a grenade and a rocket cross a sector boundary at all; see
`player_begin_sector` and
[Only the magazine comes back](#hearts-damage-and-the-two-real-deaths) above.
A climb that stops carrying one is a climb the handover is paying for nothing,
which is what `test_every_climb_carries_an_explosive_out` is watching.

## Sublevels

`Game` holds two `GameplayState`s: `gameplay` (active) and `inactive_gameplay`.
Entering the WC door swaps them (`swap_gameplay_areas`), so the parent level is
frozen intact rather than reloaded, and only the player's loadout crosses over
(`transfer_player_loadout`). Sublevel doors (`U`/`R`) are a separate mechanism
from the paired teleport doors (`D`), which are matched by index 0↔1, 2↔3, ….

**The paused sector is not ticked, and that is the decision.** `update_playing`
only ever advances `gameplay`, so while Chuck is in the restroom the sector's
alarm countdown is frozen rather than running behind the door. Running would
make the restroom the one place in the building where an alert can be waited
out — precisely the safe room the sector never granted — so a detour neither
winds the alarm down nor stands it up, and the player comes back to the floor
they left.

**The strip reports the sector, never the room.** The restroom is a room of the
building, so every field in `render_hud` that names the building's state — the
ACCESS chip and the SECURITY/ALERT readout both — reads through the `sector`
pointer (`game->in_sublevel ? &game->inactive_gameplay : &game->gameplay`) and
not through the active simulation, which while Chuck is inside is the WC's own.
Read from the active one, ACCESS fell back to a blinking LOCKED for the length
of a detour a card had already ended, and a ringing alarm went quiet on the way
in and started again on the way out — a countdown that pauses when the player
hides is the HUD offering a safe room the sector never granted. SECTOR beside
them already names the sector rather than the room, and all of them have to
agree.

**And the report between sectors reports the sector too, kills in the room
included.** `hostiles_neutralized` is a field of whichever simulation was
running when a guard went down, so the one the restroom keeps was thrown away
with the room and the floor was credited with one hostile fewer than the player
had actually cleared — while the score, which lives in the shared
`CampaignState`, had paid out for him all along. A room that pays for a kill in
points but not in the count is the same bug the `sector` pointer above exists to
prevent, only a beat later. `leave_restroom` carries the room's tally across and
zeroes it, rather than the kill being counted differently where it happens: the
gameplay core has no idea a sublevel exists and must not gain one, and the
zeroing is what stops a second visit banking the first one's kills again.

**There are four rooms, one per sector that carries a `U`, and which one a door
opens on is decided by the sector's theme.** There was one for a long time,
with `EMBEDDED_SUBLEVELS[0]` written into the shell, so the marble washroom off
the lobby, the works toilet beside the plant hall, the room behind the archive
stacks and the executive suite two floors under the roof were the same
twenty-nine tiles with the same guard standing in the same place — four times,
across half an hour of climbing. `level_theme_sublevel` in
[level.c](../src/level.c) is the table, filed by theme for the reason
`level_theme_music` is: a room belongs to the *place* it hangs off, and the
theme is what names the place, so a sixteenth sector names its room by being
what it already is. It sits in the SDL-free half deliberately — a filename
written down in C is only safe while something checks it, and
`test_every_restroom_theme_names_a_room_that_exists` walks the table through
the same matcher the shell resolves the door with, against the files actually
embedded. A theme with no room falls back to the lobby's, because a `U` that
opens on nothing is worse than one that opens on a room the player has seen.

**What the four pay is deliberately identical, and only the plan changes.** The
campaign is balanced on four grenades coming out of these doors (see below), so
every room holds one magazine on the floor and one grenade and one medkit up
top; what differs is the climb, the geometry and what is waiting — a guard and
a janitor in the lobby's, a guard and a pair of chaining canisters in the
plant's, two guards in the archive's, and two guards and a dog in the
penthouse's, where the riser is also the way out of the dog's reach because
dogs do not climb. **Every one of the four is guarded**, which this paragraph
spent a while denying by listing the plant's canisters and forgetting the man
standing beside them; [levels/LEGEND.md](../levels/LEGEND.md) tabulates all four
and had it right.

A restroom is a full small level rather than a free item cache: it is guarded,
it is climbed, and what is up there is worth naming, because it is the whole
reason to take the detour and it is easy to under-report — a **medkit and a
grenade** up on a walkway, with a magazine down on the room floor, and in every
room at least one of the two behind a one-tile gap that has to be jumped.
**The manual has to name both of them**, and for a long time it named only the
medkit: `ON FOOT` sold the door as "a room of its own, with a medkit in it",
which to a player at full hearts is a sentence saying there is nothing in there.
Those are precisely the players the budget assumes took the detour, because four
of the eighteen explosives a run can hold come out of these four doors.
**One tile, and the width is load-bearing.** A walkway band
is two rows, so the ceiling caps the jump at about 48px of ground
([levels/LEGEND.md](../levels/LEGEND.md) writes the arithmetic out) — the medkit
spent a while sitting across a two-tile gap, which the route model calls
unreachable and which a player could in fact only cross inside a 25px window of
where they started the jump. A pickup the whole detour is sold on must not be a
timing trick, and `test_embedded_restroom_sublevels` walks the route model to
the medkit, the grenade and the way back out in **every** room rather than only
checking that they are high up — these are not campaign sectors, so nothing
else ever walks them. It pins that no two share a footprint as well, for the
reason no two sectors do: the shape is the first thing the player recognises.
So a run that visits all four restrooms comes away with four grenades on top of
the fourteen the campaign lays out itself — every sector but the lobby and the
plant hall holds at least one `N`, and sector 12 holds two. Only one is carried
at a time, so what those counts buy is how often the player may spend one; it
is the amount of explosive the campaign is balanced around, and a line to check
against before either half of it moves. It is one visit each:
`sublevel_initialized` is cleared by `load_level`, so the room is fresh per
sector and spent within it. Its interior art
is derived from the map's own wall bounding box, so the room can be reshaped
without touching the renderer; a slab with open air above and below is drawn as
a railed catwalk rather than as the room's floor.
