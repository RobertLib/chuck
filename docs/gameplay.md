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
no longer deletes the corpse standing in front of the player — and only when
the array is full does it take the body furthest from Chuck. **And
`MAX_ENEMIES` is sized so that the full case does not arise on any floor that
ships**, which is the half that was missing: a console hacked under the alarm
sends for two men apiece out of a door, so sector 14's twelve guards and three
consoles wanted eighteen slots against a ceiling of sixteen, and the fallback
above — a correct fallback, written for an array that had genuinely run out —
became the ordinary behaviour of the busiest floor in the game that has doors.
It deleted the body the rule below is built on.
`test_every_sector_can_seat_the_reinforcements_it_can_call` derives the
requirement from the maps rather than trusting the number, and the editor warns
an author who draws a floor over it. And the kill tally moved off the `dead`
flags onto `GameplayState.hostiles_neutralized`, counted as each one goes down:
the flags are the population still standing, so reading them lost one kill per
reused slot and the report between sectors under-credited the floor the player
had actually cleared.

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
sector 12 carries two grenades, sectors 10, 12, 14, 16 and 17 two medkits apiece,
and **every restroom hands out the grenade the campaign's own budget is
balanced on**, so a player who took the detour still holding one paid for it
with nothing. `item_would_be_wasted` in
[gameplay_interaction.c](../src/gameplay_interaction.c) leaves all three where
they are, to be collected on the way back once they are worth something.

The magazine is deliberately *not* on that list, and the difference is the
respawn: `ITEM_GUN` comes back, so taking one on a full clip costs the player
nothing and the box is there again before it is wanted. Nothing else gets a
second chance, which is exactly why nothing else may be spent for nothing.
`test_a_pickup_that_would_be_wasted_is_left_alone` pins both sides.

**A grenade, a rocket and a flash charge survive the way out of a sector, and
nothing else does.** `player_begin_sector` ([player.c](../src/player.c)) is the
rule and
`load_level` is the only caller; the sidearm does not travel because
`player_reset` hands over a full clip either way, and the weapon *in the hand*
deliberately does not travel either, because "a pickup never arms itself" is a
rule about a doorway as much as about a floor tile.

**That rule is only fair if the player can see the thing that did not arm
itself, which is what the carried row on the strip is for — and for a release the
row's own label pointed at the wrong number.** The strip prints one label over
two readouts: the cartridges, which are always the pistol's because all carried
ammunition stays visible, and the three slots of the carried row. The label names
whatever the next press will fire. So selecting the launcher printed `BAZOOKA`
over six lit cartridges — which reads as six rockets — while the one rocket the
player actually had was a glyph three slots to the right with nothing joining the
word to it. Neither readout was wrong; what was missing was which of them the
label meant. A rule under the slot the label names says so, and `PISTOL` and
`BOLTS` mark nothing, which is right: one *is* the cartridges and the other is a
pocketful with no count anywhere in the game. Every gate was green over it and
always would have been — a counter cannot tell a frame that was drawn from a
frame anybody could read.

**The facade is why it exists.** Nothing on a climb can be thrown or fired at
all — `update_facade_playing` clears `shoot` for the whole of it and
[gameplay_climb.c](../src/gameplay_climb.c) has no notion of a weapon — so the `N`
standing mid-wall on each of the five climbs is a pickup whose entire value is
in the sector above it. Wiped at the doorway, as it used to be, that was a
detour paid for in wind and thrown bricks that bought nothing whatever, and the
sixteen grenades the campaign lays out included five that could never be
spent. The rule lives in the SDL-free half precisely so the suite can hold it:
`load_level` is shell code the tests link no SDL to reach, which is how a
doorway went that long unexamined. `test_a_sector_hands_its_explosives_to_the_next`
pins what crosses and what does not, and
`test_every_climb_carries_an_explosive_out` pins the reason.

**And the same rule read the other way says what a climb must *not* carry.**
All four of the climbs there were then laid out a `G` as well, which is the one
pickup a wall can do nothing with whatever: it cannot be fired up there, and it
does not cross the doorway either, because the clip the next sector opens with
is a full one however this one ended. So walking over it changed no counter,
paid no score, and played `SFX_PICKUP_AMMO` — the sound of a pickup that
worked. That is precisely the failure `item_would_be_wasted` exists to stop for
the grenade, the rocket and the medkit, and `ITEM_GUN` is exempt from it for a
reason that only holds indoors: the box comes back on `ITEM_RESPAWN_TIME`,
which costs a player on a *floor* nothing and buys a player on a *wall*
nothing. `test_no_climb_lays_out_a_pickup_it_cannot_use` keeps the map honest
instead, so no climb can quietly lay one out again — it walks every facade the
campaign ships, which is five of them now rather than the four that ever
carried a box; [levels/LEGEND.md](../levels/LEGEND.md) carries the authoring
half of it.

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
HUD draws all three carried things whether or not they are raised, so carrying
one is still visible; the bumpers are how it reaches the hand.

**That last sentence was the rule's whole justification and it was two thirds
true**, which is worse than a missing argument: a pickup that does not arm
itself is only fair if the player can see they have it, and the sector strip
drew the grenade, drew the rocket and drew nothing at all for the flash charge.
It is `draw_hud_carried` now, one row of three slots shared by both strips, laid
out from constants in [game_config.h](../src/game_config.h) that
`test_the_carried_row_gives_every_throwable_its_own_place` can reach. The wall's
strip had the same defect the other way up — it drew the charge *on top of* two
of the six cartridges — which is this repository's most reliable smell: two
screens, the same question, two answers, and here neither of them right.

There is **one exception, and it is the one that costs nothing**: a magazine
picked up while Chuck is holding the knife with a dry clip raises the sidearm,
because an empty clip is the whole reason he is holding a blade and that is the
same sentence "only the magazine comes back" is already written to finish. A
knife chosen on purpose, with rounds still in the clip, is not that case and
survives the pickup. `test_a_pickup_never_arms_itself` pins the rule, the
fall-back and the exception together.

## Going quiet

Three mechanics answer one rule, and the rule is the one this building has
always kept and the player could previously do nothing about: **the guards read
what they see and hear.** The cone, the peripheral radius, the sight timer, the
walk to a heard disturbance, the body a calm man finds and the switch he runs
to are all simulated in detail — and until these three, the only way the player
could reach any of it was to fire a gun, which puts the noise and the man who
made it in the same place every time. The whole perception model could only be
used *against* them.

**The blade behind an unaware man takes him down in one.** `player_knife_attack`
([gameplay_combat.c](../src/gameplay_combat.c)) asks two questions before it
swings: `guard_is_unaware` — the alarm is down, nobody has shot him, he is not
running for a switch, he has not decided an encounter, and he is not already
mid-aim — and `player_is_behind_guard`, which is Chuck on the side `enemy->dir`
is not pointing at. Level with him is not behind him, and neither is a spawn
that has not picked a direction yet.

**What separates a takedown from `damage_enemy` is not the damage.** It is that
`takedown_enemy` never calls `gameplay_provoke_enemy` — and provoking wakes the
man who was hit *and the partner he was talking to*, which is what turns one
guard going down into two guards hunting. A blade behind an unaware man wakes
nobody, so the floor stays exactly as quiet as it was. That is the trade being
offered, and getting behind him at all costs the player their range and their
mercy window. It pays `PLAYER_TAKEDOWN_SCORE` against the `ENEMY_SCORE` every
other kill pays, because nothing on screen announces the mechanic and the number
that comes up is the only thing that can say it happened.

**Deliberately not a line-of-sight test.** Sight is what sets those flags in the
first place; asking it again would make the rule depend on which frame the swing
landed on rather than on what the guard had noticed, and a takedown that works
or does not according to where a patrol turn was in its cycle is not a rule
anybody can learn. `test_a_guard_who_never_saw_it_coming_goes_down_at_once` pins
all four ways it can fail and
`test_a_takedown_does_not_wake_the_man_he_was_talking_to` pins the half that is
the actual point of it.

**A dog is never taken from behind**, and that is a rule about the animal
rather than a gap. A guard is beaten by getting outside the cone he is looking
down; a dog has no cone to get outside of — `dog_sees_player` is a range and a
rough heading, and `DOG_BITE_WINDUP` announces itself precisely because the
animal has already found you. A silent answer to one would make the quietest
way through a sector the one that walks straight past the thing placed to stop
it. A flash charge is not that, and the distinction is the whole of why it
works on an animal the blade does not: it goes off with a bang and a shake and
it buys seconds rather than a kill, so the dog is still there and still hunting
when it comes back. What it removes is the moment, not the obstacle.

**And a standing round goes over it**, which is not a rule about animals either —
it is the height the muzzle sits at. A shot leaves the hand at
`PLAYER_H * 0.35`, so its underside is 0.8px above anything 16 tall standing on
the floor, and `DOG_H` is the same sixteen as `GAS_CANISTER_H`. The canister's
version of this is a mechanic the legend states and
`test_gas_canister_requires_crawling_shot` pins; the animal's version had never
been written down anywhere, so the sidearm — the one weapon that never runs out —
quietly does nothing to the fastest thing on the floor unless Chuck goes down onto
his elbows first. So the answers to a dog are the blade (which reaches it, unlike
the takedown behind a man), the same round from a crawl, the same round up or down
a ladder, and anything that goes off; what it is not is the thing a player reaches
for without thinking. `test_the_shot_line_is_chest_high` holds all of that
together in one place, because the two consequences have one cause and neither is
guessable from the constant that produces them.

**A bolt is thrown to be heard somewhere Chuck is not.**
`PLAYER_WEAPON_DECOY` is the fifth weapon and the only one that is not a weapon:
never carried, never picked up, never spent, and limited by nothing but
`DECOY_COOLDOWN`. A count would make it a resource to hoard, and a resource
being hoarded is one nobody experiments with — fatal for the one mechanic in the
game that has to be discovered by trying it. `gameplay_combat_update_decoys`
flies it on the ordinary collision, and where it stops it plays
`SFX_GRENADE_BOUNCE` and calls the same `gameplay_alert_enemies_to_noise` that
gunfire and explosions already use, so what the player gets out of it is
exactly the behaviour they have already learned from being shot at.
`DECOY_NOISE_RADIUS` is under `ENEMY_HEAR_RADIUS_SHOT` and has to stay under
it, or the quietest option in the game would also be the one that reaches
furthest. Both are written as a whole number of tiles so a `_Static_assert` in
[game_config.h](../src/game_config.h) can hold the order of them: swapping the
two is a build failure rather than a rule that quietly stopped being true.

It is the dumbest projectile in the tree on purpose: no sweep, no impact test,
and it passes through guards, dogs, crates and canisters alike. A bolt that
could break a crate or set off a cylinder would be a weapon, and the one thing
this has to stay is not a weapon — the player must be able to throw it into a
room full of people and have the only consequence be that they all look at it.
`test_a_bolt_pulls_a_guard_to_where_it_landed` pins that the curiosity lands on
the bolt's position rather than on Chuck's, and that nothing was hurt by it.

**And a body can be hauled out of the room it fell in.**
`update_body_discovery` has always sent the next calm guard who sees a corpse
over to look at it and often on to the nearest alarm switch — a rule the hazard
budget assumes and which the player could only *hope* about, because a patrol
route is the single thing about a sector that cannot be read off the map.
`gameplay_update_body_drag` ([gameplay_interaction.c](../src/gameplay_interaction.c))
is the answer, and **nothing in it tells the AI anything**: it moves a body, and
the perception model that was already running does the rest. A corpse behind a
partition is a corpse `enemy_sees_point` returns false for, and that is the
entire mechanic.

Four things about it are decisions rather than details.

- **It answers the same held button the terminal does, and the terminal wins.**
  A second binding for "put your hands on the thing in front of you" would be a
  key the manual has to teach and the options sheet has to carry, for an action
  nobody can want at the same moment as a hack. Where the two overlap the
  console takes it: a hack is a decision the player made, a body is furniture
  they happened to stop beside.
- **The doorway wins on the same grounds, and that half was missing.** `USE` is
  one physical key read two ways — `interact` while it is down, `use_door` on the
  press — so standing on a `D` or a `U` with a corpse at his feet, Chuck was told
  `HOLD E TO DRAG BODY` and the press did both: it took hold of the body *and*
  walked him through the door, which is a teleport across the sector, a
  checkpoint banked, and the corpse back on the floor where it lay a frame later
  when the leash broke. The prompt named one action and the key performed a
  different and more expensive one, on the two sectors where a guard can go down
  standing in a doorway. Only *starting* a grab is refused now, and a door tile is
  one tile, so the grab still works from either side of it exactly as it does
  beside a terminal — and a body already in hand is hauled straight across one,
  because carrying it means holding `USE` and the door needs a fresh press.
- **The prompt asks the same function the grab does.**
  `gameplay_body_within_reach` is what `render_interaction_prompt` calls, so a
  prompt cannot appear where the grab then does nothing — the failure this
  codebase refuses everywhere a button is named. That is also why both claims on
  the button live inside `nearest_body_in_reach` rather than as a check at each
  call site: the console's was, the doorway's was not, and one of the two
  therefore disagreed with the prompt.
- **`Player.dragging` is a flag, not a -1 in the index.** A zeroed
  `GameplayState` has to be a simulation nobody is dragging anything in, because
  that is what every test and every fresh sector starts from; an index sentinel
  would put slot nought in Chuck's hands the moment anybody wrote `{0}`. It is
  the same rule the assist switches keep.
- **The drag writes only `x`.** Gravity stays with `settle_body` in
  [gameplay_ai.c](../src/gameplay_ai.c), which already runs over every corpse
  once a frame; a second helping would drop the body in Chuck's hands visibly
  faster than the one lying beside it. That is also why the function takes no
  `dt` at all.

**And a dog is hauled the same way a man is**, which is worth stating because the
mechanic reads as being about corpses in coats. `nearest_body_in_reach` scans the
fallen animals after the fallen men and `Player.dragging_is_dog` says which of the
two arrays the index is in, so a shot dog is furniture to be moved exactly like its
handler. It has to be: `update_body_discovery` sends a calm guard to a fallen
animal as readily as to a fallen man, and `W` puts a dog on ten of the seventeen
sectors, so an animal left in an open corridor wakes a floor as surely as a body
does. The two halves were written together and only the man's had ever been
simulated — `dragging_is_dog` appeared in the suite exactly once, as `false` —
which is why `test_a_dragged_dog_stops_being_found_where_it_fell` exists beside the
two named below.

It costs `PLAYER_DRAG_SPEED`, which is deliberately under `PLAYER_CRAWL_SPEED`
— crawling is the other way to be hard to see and has to stay the quicker of
the two, or hauling a dead man would be the fastest careful way across a floor.
A `_Static_assert` beside the constant is what keeps that order. A ladder, the
facade, a crawl, leaving the ground, dying, the console's claim on the button,
and a slot that has stopped being a corpse all let go of it;
`test_a_dragged_body_is_dropped_by_everything_that_should_drop_it` walks every
one of them, and `test_a_dragged_body_stops_being_found_where_it_fell` is the
one that matters — a witness who finds the body where it fell and has nothing
to find once it has been moved, with the witness put back on his own tile
between the two looks so the only thing that changed is where the body is.

All three are taught on the manual's ninth sheet, `GOING QUIET`, because none of
them is announced anywhere else on screen except the drag's own prompt — see
[The field manual](screens.md#the-field-manual).

## The duct

A `=` is trunking let into a wall, and it is the one tile in the game the two
solidity questions answer differently: masonry to a man on his feet, a gap to a
man on his elbows. Hold down and crawl in.

**Everything that makes a shaft safe is that one answer rather than a rule of
its own.** `level_is_solid` is what stops a line of sight, a round, a blast and
anybody following on their feet, and trunking keeps answering it — so a guard
does not see into a duct, cannot shoot into one, cannot walk into one, and the
lighting pass leaves the inside dark. None of that is written down anywhere as a
special case, which is the whole reason solidity was split in two:
`level_blocks_stance` is asked only by the four collision tests in `level_move`
and by the crush pass, and only Chuck on his elbows ever passes it anything but
`STANCE_UPRIGHT`.

**What it costs is the other half of the same fact.** The louvres are opaque
both ways, so the player cannot see the room they are about to come out in; and
a crawl already denies hauling a body. The shaft is a bet rather than a shortcut
— you go in knowing where you entered and not what is waiting at the far mouth.

**And that sentence used to deny two more things that the crawl does not deny,
which is this page arguing with the sheet the player reads.** It said *"a crawl
already denies the sidearm, the hack, and hauling a body"*, and only the last of
the three is true — `player_can_drag` refuses `crawling` in as many words, and
nothing else does.

- **The sidearm is the opposite of denied: it is the reason to crawl.** The
  manual's own movement sheet sells the posture as *"the only way to hit
  something sitting on the floor"* and its combat sheet says *"Crawl and shoot a
  GAS CANISTER"*; `test_the_shot_line_is_chest_high` exists because a standing
  round passes over anything sixteen tall on the floor, which is the whole
  mechanic. Measured, a crawling press spends a round and puts one in the air.
  So the one page describing the duct denied, by name, the one thing the two
  player-facing sheets are built on. **Two documents, one question, two answers**
  — which is this tree's most reliable smell, and the first time it has pointed
  at a page rather than at a screen.
- **The hack is not denied either, and nothing was ever going to deny it.**
  There is no `crawling` test anywhere in the terminal path, and driven against
  every console in the campaign a crawler is in range of exactly what an upright
  man is in range of. What is true is narrower and is about the tile rather than
  the posture: a console cannot be *inside* trunking, because a `T` needs a tile
  and the duct is the wall.

The shape is the one the `%`-and-`P` note further down this page already has a
name for: **a claim about what a shaft is for, read as a claim about what the
simulation refuses.** The duct's other sentence had the same defect in its JUMP
clause and that one was fixed in the code, because the game was wrong; these two
were fixed here, because the game was right. `test_what_the_crawl_takes_away`
holds all three of them from now on — that the crawl fires, that it reaches a
console, and that it will not drag — so the next person to write this sentence
has to be right about it.

**And the picture says the same thing, which took a fix.** A shaft whose whole
cost is opaque louvres has to be *drawn* opaque, and it was not: the tile layer
goes down at the top of the frame and the figures near the bottom of it, so Chuck
was painted over the trunking he had just crawled into — inside the wall by every
rule and outside it on screen. `render_duct_fronts` lays the louvres back over
him, so what the player sees is a slot of shirt at a time and the half of him not
in yet still out in the room. [Tuning, art, audio](art-and-audio.md) is where the
split that allows it is written up, and `--screen aftermath --level N --page 2` is
the one frame in the sweep that draws it.

**A duct is horizontal.** The tile a crawler is inside stops blocking them, so
what holds them up is whatever the map put underneath, which for a duct set into
a storey is that storey's own slab. Trunking over a hole is trunking the player
falls out of; the editor says so, and [levels/LEGEND.md](../levels/LEGEND.md)
carries the authoring rules.

**And the vertical axis is where both halves of that were wrong.** The
horizontal crawl was written, tested and right; nothing had asked what trunking
does to a man moving *up or down* through it, and both existing duct tests ask
only about the direction that works. It does the same thing in both directions,
to two moves that had no business making it.

A player standing on top of a run who pressed DOWN was rocked between the two
postures **240 times a second**. The crawl lowers his box, the tile he had been
standing on stops holding him — `level_blocks_stance` opens a shaft to a crawler
downwards exactly as it opens it sideways — `on_ground` goes out with the floor,
and `want_crawl` requires `on_ground`, so the very next step stood him back up
onto the lid and handed it straight back. Every run on sector 12 has its own
storey's air above it, so every one of them was a walkway on which the crouch key
did that: the drawn pose alternating, the collision box 14px taller and shorter by
turns, and `crawling` — one of the two ways of being hard to see — true on half
the sight checks a guard made. **The lid is a walkway, not somewhere to lie
down.** The crawl is refused where the only thing holding the player up is a
duct, which is the same answer that keeps a shaft entered at its mouths rather
than through its roof.

And from inside, one press of JUMP put him on top of it, because a rise is
resolved in the posture he is in as well. That is the sentence
[levels/LEGEND.md](../levels/LEGEND.md) already spends on the route model — the
crawl is the only move a shaft allows from inside one — being true of the model
and false of the game: a shaft could be left anywhere along its length, "a duct
with one mouth is not a route" described nothing, and the opaque louvres the bet
above is made against could be lifted at any tile. The jump is refused inside the
shaft and the buffered press is left standing, so it fires the moment he crawls
out of a mouth, which is what `PLAYER_JUMP_BUFFER` is for.

`test_the_lid_of_a_shaft_is_not_somewhere_to_lie_down` and
`test_a_shaft_is_left_by_its_mouths` hold the two, and most of the first is about
not breaking the game to mend it. A rung, a cracked panel and a moving platform
hold a player up with no solid tile under his feet at all — `level_move` catches
them with one-way tests that know nothing about posture — so a rule reading "the
tile under the feet must be masonry" would take the crawl away on the seven
floors that carry a `P` or an `F` in order to close a hole on one. Only a tile
that is solid to one stance and open to the other can take the floor away by
being crouched on, and there is exactly one of those.

**Sector 12 is the floor the campaign spends on them**, which is the sector this
page already described as six crawl levels stacked a riser apart. It carries four
runs and 61 tiles of trunking: most of the storey that leaves by the window, most
of the middle storey, a third across the storey below it, and the pillar that
divides the one under that. Two of them are long enough that crossing one is a
decision about the whole floor — slower than walking it, and blind at both ends,
against a storey with a guard, a dog and a mine on it.

**The crush pass is where this quietly went wrong once and would again.**
`gameplay_resolve_player_crush` asks what is over the player's head, and the
tile a crawler is *inside* is the tile that loop reads — so asked in the upright
posture it finds masonry over the head of every man in every shaft, tries to
push him clear at both walls, finds those blocked too, and takes a heart. On
screen that is a hazard that reads as nothing happening at all.
`test_the_ducts_sector_can_actually_be_crawled_through` is what holds it, and it
holds it by *driving* the sector rather than modelling it: the shipped map, the
pad held down, `player_update` and the crush pass in the order the frame runs
them, hearts checked every step. A shaft nobody has crawled is a shaft nobody
has checked.

**The route model treats the crawl as its own move rather than as cheaper
walking**, because what the player can do inside a duct is not what they can do
on a floor. `route_masonry` still answers wall — that is what keeps a jump, a
step up and a hole hop from being routed through trunking — and
`route_neighbours` adds exactly one edge for a duct: along its row, in from a
tile beside its mouth, out onto one at the far end. Without that separation the
model walked out of a duct into open air and took the fall, which would have
certified a shaft over a hole as a way down through it.

## The charge that answers being seen

The three mechanics above are all about *before*. A bolt moves attention
somewhere else, a blade removes a man who never looked, a dragged body removes
the reason the next one looks — and every one of them stops being available the
moment somebody is actually shooting. At that point the player's whole
repertoire is "shoot back" and "survive `ALARM_CALM_TIME`", which is the same
repertoire they had before any of this existed.

`!` is a **flash charge** ([levels/LEGEND.md](../levels/LEGEND.md)), and it is
the answer to *after*. `detonate_flashbang`
([gameplay_combat.c](../src/gameplay_combat.c)) is the shape of `apply_blast`
with the whole of `apply_blast` taken out: nothing is killed, no crate breaks,
no weak wall opens, no charge in reach chains, and **the player is untouched** —
a flash that could hurt Chuck would be a grenade with a worse radius, and the
point of it is that it is safe to use in the room you are standing in.

**One is laid out on sectors 8, 10, 12, 14, 16 and 17**, which is a decision
about where a sector can go wrong: the lab where the mines start, the security
floor, the duct run, the penthouse, the vault and the roof. Nothing below the lab
carries one, because a floor a player can still walk away from does not need the
escape.

That list went unwritten for as long as it existed, and the vault was missing from
it — the second-hardest interior in the campaign by the hazard budget, and the one
late floor with no answer to having already been seen. Nothing could have said so:
the fit checks measure whether a line is readable and the route check measures
whether a pickup can be reached, and neither is a question about which floors were
given one at all. **A rule about where something belongs is a rule a list can be
held to**, which is why it is a sentence here now rather than a judgement made once
per map.

What it takes is `FLASH_BLIND_TIME` of the room's attention. `Enemy.blind_timer`
is read in `enemy_has_los` and `enemy_sees_point`, which is why one field covers
the sight timer, the encounter, the shot solution *and* body discovery at once —
everything downstream asks those two rather than asking the world. The movement
pass stops him where he stands, because a man walking his patrol with his eyes
shut would read as the charge having done nothing. Cameras in reach lose what
they had, and recover: only a round or a blast takes one off the ceiling.

**"The room" is a sight line, not a radius, and for a long time it was a
radius.** `FLASH_RADIUS` is five tiles and `game_config.h` has always said what
that number is for — *"it has to catch the room the player is in, and it must not
reach the one next door, or 'throw it and walk' would be the answer to every
floor"* — but the charge tested distance alone. Five tiles is wider than any
partition in the building and wider than a storey is tall, so a charge thrown in
one room blinded the men in the next one and on the floors above and below it,
through solid masonry. Sector 12 is six crawl levels stacked one riser apart and
sector 14 is panelled rooms off a single doorway apiece; both of them were a
whole floor that one charge switched off. It goes through
`gameplay_sight_line_clear` now — the same segment test a guard's own cone and the
ceiling lens are stopped by, moved out of the perception model into
[gameplay_world.c](../src/gameplay_world.c) once the third caller turned out not
to be a pair of eyes. A slab, a crate or an unopened weak wall stops the flash
exactly as it stops a guard, which is the whole of what one solidity rule buys:
the player never has to learn a second answer to "can this be seen from here".
`test_a_flash_charge_stops_at_the_masonry` pins both halves — the man in the room
goes blind, the man behind the wall does not.

**The lenses were the half of that promise nothing was holding.** The paragraph
below argues for the dog by saying the charge already stopped "the men and the
cameras", and the men were tested and the dog was tested and the cameras were a
sentence: fourteen lines at the end of the flash that no test had ever run. It
matters more than its size, because a lens is the one thing in the room the charge
can help against *retroactively* — a camera part way through `CAMERA_NOTICE_TIME`
is a second from putting the whole floor on Chuck, and what the charge does is
take that count back to nought. Nothing on screen says so, which is precisely why
a play-through cannot check it. `test_a_flash_charge_makes_a_camera_forget` does,
including that the fitting is still bolted to the ceiling afterwards: only a round
or a blast takes a camera down, or the charge would be the answer to a thing the
player is meant to route around.

**And the dog, which is the one it used to step over.** `Dog.blind_timer` is
the same field doing the same job, and the argument for it is the one this
function already made out loud for the fittings: a lens is glass and a sensor
and a charge this bright in front of one is the same event it is for a pair of
eyes — and then the only thing in the room with an actual pair was skipped. So
the charge stopped the men and the cameras and left the teeth coming, which is
a blast picking which of the things beside it are real, and it failed in
exactly the case the mechanic exists for: a dog is the one enemy that has
*already* found you, which is why `DOG_BITE_WINDUP` announces itself at all, so
"the one answer to having already been seen" was no answer to it whatever. The
manual had been promising it for longer than the code did — `GOING QUIET`'s own
sheet reads *everyone near it stops seeing*, and the dog was an exception
nobody had written down. A bite already wound up is cancelled, because that is
the animal's `aim_timer`; the chase is not, because the guards keep theirs.
Walking into a blinded dog still gets you bitten, and that is deliberate rather
than a gap: contact with a blinded *guard* still costs a heart too, because
neither of them stopped being in the room. The charge takes their attention,
not their mass. `test_a_flash_charge_reaches_the_dog_as_well` pins the lot.

**`update_enemy_reactions` needed the flag written out separately, and that is
worth knowing.** Retaliation is the one perception path that never asks
`enemy_has_los` — it is the beat where somebody walks up behind a guard and he
*turns* — so a blinded man went on spinning round and aiming at a room he could
not see. `test_a_flash_charge_blinds_the_room_without_changing_it` is what found
it, by standing Chuck in front of a flashed guard and asking whether anything
happened.

**It does not clear the encounter, and that is the design.** Nobody is
unprovoked, no pursuit target is dropped and the alarm keeps running. Everyone
comes back exactly as they were, still hunting, still remembering where Chuck
was standing. The charge buys seconds, and the seconds are for leaving. One
thing it *is* allowed to take is a man's walk to an alarm switch, because the
alternative is a charge that goes off in a guard's face and does not stop him
pulling the handle two paces later.

It is carried one at a time like the grenade, travels between sectors with the
explosives for the same reason they do (nothing can be thrown on a climb), and
is drawn as the grenade's opposite — steel and a white band and a cyan
tell-tale against olive and brass — because one of the two is about to kill
whoever is standing beside it.

## The camera on the ceiling

**And then there is the one that none of the three answers work on.** `I` is a
ceiling camera ([levels/LEGEND.md](../levels/LEGEND.md)), and it exists because
the mechanics above are, between them, a complete answer to a guard: he has a
facing, so there is a side of him to be on; he has ears, so a bolt moves him; he
can be taken from behind, and the body can be carried away. A sector where every
problem has that shape is a sector with one puzzle in it repeated.

A camera has no facing to get behind, no ears, and crawling does nothing to it —
it is on the ceiling looking down at the floor the crawl is on. What it has is a
**sweep**, which is a clock rather than a perception: `gameplay_camera_angle`
is a triangle wave either side of straight down, so the beam turns at a constant
rate and spends equal time on each half of its arc. A sine would loiter at the
ends and read as the mounting hesitating, and the one thing this motion must not
be is coy about where it is going next — the whole value of the fitting is that
its timing can be learned.

Four properties are decisions.

- **It sees through the same `gameplay_sight_line_clear` a guard does.** A slab, a crate or an
  unopened weak wall stops the beam exactly as it stops a man. One solidity
  rule; a camera that could see through a wall a guard cannot would be a special
  case nothing on screen explains.
- **The alarm it raises points at Chuck, not at the camera.** It calls the same
  `gameplay_trigger_alarm` a guard reaching a wall switch calls, with the
  player's own position as the source. An alarm that sent the floor to the
  fitting on the ceiling would be an alarm that helped.
- **A round takes one down and a bolt does not.** Shooting one is permanent for
  the visit, pays `CAMERA_SCORE`, and is the loudest thing the player can do —
  so the sector trades a standing problem for an immediate one, which is the
  bargain the weak wall already makes with an explosive. The bolts must not be
  able to do it, because they are not a weapon and the whole feature rests on
  that. **A guard's own round cannot either**, and that one is not an oversight
  to be tidied up later: a camera hangs over exactly the ground guards shoot
  across, so letting enemy fire break them would hand the player the answer for
  free and constantly. Only `gameplay_combat_update_player_bullets` tests them.
- **A blast takes them with it**, for the reason `apply_blast` exists: a grenade
  that brought the wall down and left the camera bolted to what was left of it
  looking at the hole would be a blast that picked which of the things beside it
  were real.

`CAMERA_NOTICE_TIME` is what makes the beam readable — crossing it is
survivable, standing in it is not — and the lens flushes to the palette's danger
red for the whole of that beat, then fades through amber for
`CAMERA_SUSPICION_FADE` so a player who got clear can see that they nearly did
not. It is longer than a guard's `ENEMY_NOTICE_TIME` on purpose: a man who
spots you shoots you, and a camera only ever tells everybody else.
`test_a_camera_takes_longer_to_be_sure_than_a_man_does` holds the gap, in the
suite rather than in an assertion, because two floats cannot be compared in a
`_Static_assert` without the extension this tree refuses.

**The renderer draws the very cone the simulation tests against.**
`gameplay_camera_angle` is public for that one reason — a second copy of the
sweep in the draw call, even a correct one, would be a picture free to disagree
with the rule about where the player is safe. Reduced motion stops the lens
*flashing* and never the sweep, because a beam held still is a corridor nobody
can cross rather than a timed one.

They stand in sectors 5, 6, 8, 10, 12, 14 and 16 — the machine hall, the
canteen, the clean room, the monitor-wall floor, the crawl ducts where the
crouch stops working, the penthouse and the vault — and each is worth
`CAMERA_HAZARD_WEIGHT` of that sector's budget; see
[what a new sector owes](levels.md#one-plan-per-sector).

**The first three of those are why the list starts at five rather than at ten.**
The camera is the one watcher none of the quiet answers work on, which makes it
the most interesting thing in the perception model and, for a long time, the
last thing a player met: it appeared in sector 10 of 17, so two thirds of the
campaign taught a set of answers and the exception to all of them arrived once
the building was already at its busiest. A single lens in the machine hall, the
canteen and the clean room is the mechanic introduced where a floor still has
room to teach it — one beam, over ground the route has to cross, on a storey
with a way round.

## The man in the vest

For the whole campaign the building held one kind of man, and every mechanic the
player learns is an answer to that one man: three rounds from the front, a
stomp for the cheap kill, a blade behind him if he has not looked. A floor where
every problem has that shape is a floor with one puzzle in it repeated.

`Q` is a **heavy** ([levels/LEGEND.md](../levels/LEGEND.md)) — the same guard in
a plate carrier and a full helmet — and he exists to take exactly one of those
answers away. It is the stomp, and the choice is not arbitrary: it is the *free*
one. It costs no ammunition, no position and no noise, and it is what anybody
reaches for the moment a floor gets busy. `enemy_kind_can_be_stomped` is the
whole of the rule in `gameplay_combat_check_contacts`; landing on him costs the
heart a side contact costs, which is what the existing branch already does.

**The blade behind him is deliberately unchanged**, and that is the half that
makes him interesting rather than merely tough: a takedown is a knife across a
throat and not damage, so the vest is no help at all against it. A heavy is
therefore a floor's clearest argument for the quiet route — the man it is least
worth shooting and most worth walking behind.

Three numbers differ and nothing else does. `ENEMY_HEAVY_HP` from the front,
`ENEMY_HEAVY_SPEED` on his feet — because carrying it has to cost him something
the player can see, or he is an ordinary guard with a longer health bar — and
`ENEMY_HEAVY_HAZARD_WEIGHT` of the sector's budget. `EnemyKind` is a field on
the guard rather than a second entity type, because everything else about him
*is* a guard: the same patrol, the same cone, the same bodies investigated, the
same run for the same switch. A separate `HeavyGuard` would be a second copy of
eleven hundred lines of AI to keep in step with the first, which is the failure
`apply_blast` was written to end.

**Reinforcements out of a door are never heavies.** He is a fixture of a floor
plan, placed where the author wanted the stomp denied; one arriving at random
out of a doorway would be a difficulty spike nobody drew, and a raised alarm
could hand out two of them.

He is drawn by silhouette rather than by tint — a second plate across the chest
and a pad on each shoulder, both wide enough to break the outline — because the
player has to recognise him *before* they jump at him. A rule learned only by
losing a heart to it is a rule the game never taught. The wounded colours are
read against `enemy_kind_hp` rather than against `ENEMY_HP`, which is the number
that stopped being right the day a man with six of them walked in.

They stand in sectors 10, 12, 14, 16 and 17 — one in the control wing, two
apiece on the ducts and the penthouse, and four each on the vault and the roof.
More of them the closer the roof gets, which is the shape the finale is for.
Never on a climb: sector 15 is a facade and a facade carries no men at all.

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

**And the animals were the third copy nobody had checked.** `apply_blast` walks
the guards and then the dogs, and the dog loop had never run: twenty lines that
kill an animal, yelp, and pay `DOG_SCORE` through the same
`gameplay_record_neutralized` the tally is ranked on. That is not an edge case —
a dog is the one thing on a floor with no silent answer to it, so an explosive is
an entirely ordinary way to meet one, and the score is the only thing on screen
that says the animal counted. `test_a_blast_kills_a_dog_in_reach` runs it through
a grenade, which by the rule above is the rocket, the mine and the canister as
well, and also pins that a second blast on the same corpse pays nothing: a body
that could be counted twice would make an explosive the cheapest way to inflate
the docket.

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
from [gameplay_interaction.c](../src/gameplay_interaction.c)) —

**Which makes the safety net a property of the map, and three floors had
none.** A climb banks by climbing, so it cannot go wrong; an interior banks only
where an author put one of those four things. The sectors that leave by a window
lay out neither a card nor a terminal and bank on doors and medkits alone —
sectors 2, 6, 10 and 12 — because a welded stair door leaves nothing for either
of them to open. Two of those four had a door pair on the line and were fine.
The rest — 2, 6 and 12 — had put every bank they owned *off* the route, so a
death anywhere on those floors replayed the whole of them: 133 route steps of
mines and fans on sector 12, which is the longest walk in the campaign. Nothing
could see it. The hazard budget counts what stands on a floor, and the route
model only ever asked whether the way out could be reached at all.
`test_no_sector_asks_for_a_long_walk_with_nothing_banked` measures it now, by
flooding the route twice — from the spawn and from the way out — so that "on the
route" means a bank the player passes rather than one they would have to go
looking for. Every interior has to bank at least once on the line, and no
stretch may run longer than `CHECKPOINT_MAX_STRETCH` steps of the model. The
nine sectors that were already right run between 17 and 35; the three that were
not were fixed by moving a medkit each onto the line they were beside. and
`gameplay_restore_checkpoint` handles both modes, clearing whatever is in the
air that could land on the man who has just been put back — enemy bullets
inside, thrown bricks and birds on the wall. What Chuck himself threw is left
alone deliberately: it is part of the world he changed, and the respawn's own
`INVULN_TIME` already covers him. A death keeps everything Chuck was carrying
(`finish_player_death` calls `player_carry_loadout` across `player_reset`),
refills the sidearm, and never reloads the level, so the world keeps its dead
guards and opened walls.

**"The carried grenade and rocket" is what this said, and it was a list of
three written as a list of two.** The loadout rule was written out twice — once
in [player.c](../src/player.c) for a sector boundary and once in the shell for a
death and the restroom door — and the flash charge was added to the first copy
only. So dying destroyed the one item in the game whose entire subject is a
floor having *already* gone wrong, on the six sectors that lay one out, and one
`!` a floor does not respawn. Both rules share `carry_throwables` now, on the
side of the boundary the suite can reach, and
`test_every_doorway_hands_over_the_whole_pack` asks the property off the weapon
enum rather than off a list of fields — which is the only version of the check
that cannot go stale the way the thing it checks went stale.

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
whole reason the three carried things cross a sector boundary at all; see
`player_begin_sector` and
[Only the magazine comes back](#hearts-damage-and-the-two-real-deaths) above.
A climb that stops carrying one is a climb the handover is paying for nothing,
which is what `test_every_climb_carries_an_explosive_out` is watching.

## Sublevels

`Game` holds two `GameplayState`s: `gameplay` (active) and `inactive_gameplay`.
Entering the WC door swaps them (`swap_gameplay_areas`), so the parent level is
frozen intact rather than reloaded, and only the player's loadout crosses over
(`player_carry_loadout`, the same call a death makes). Sublevel doors (`U`/`R`)
are a separate mechanism from the paired teleport doors (`D`), which are matched
by index 0↔1, 2↔3, ….

The door used to take the flash charge off him for the length of the visit — the
shell had its own copy of the loadout rule and the charge was missing from it —
so the four rooms, which have men in them and one of which has a dog, were the
one place in the building where the answer to being seen was in the sector
outside. Same one-line cause as the death above, same fix.

**And the blink travels with the hearts**, which for a long time it did not.
`invuln_timer` is a field of `GameplayState` rather than of `Player`, so the
swap took it with the frozen area and the shell handed back only the pack and
the hearts: a player who took a hit and stepped through arrived with no mercy
window at all and could be hit on the first frame of a room holding two men and
a dog, and coming back out he was handed the sector's *old* window, banked at
the moment he went in, however long the visit had taken. The blink belongs to
the body the way the hearts do, and the whole rule is
`gameplay_carry_through_doorway` in
[gameplay_state.c](../src/gameplay_state.c) now rather than three assignments in
the shell — which is also what lets
`test_the_doorway_hands_over_the_blink` reach it, since
`leave_restroom` is executed by neither gate. It is still not a heal: what
crosses is what he had, never more.

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
of the twenty explosives a run can hold come out of these four doors.
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
the sixteen the campaign lays out itself — every sector but the lobby and the
plant hall holds at least one `N`, and sector 12 holds two. Only one is carried
at a time, so what those counts buy is how often the player may spend one; it
is the amount of explosive the campaign is balanced around, and a line to check
against before either half of it moves. It is one visit each:
`sublevel_initialized` is cleared by `load_level`, so the room is fresh per
sector and spent within it. Its interior art
is derived from the map's own wall bounding box, so the room can be reshaped
without touching the renderer; a slab with open air above and below is drawn as
a railed catwalk rather than as the room's floor.
