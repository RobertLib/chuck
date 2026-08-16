# AGENTS.md

## Project

Chuck is a 2D action platformer written in C17 against SDL3. All art is drawn
procedurally at runtime and all audio is synthesized at startup, so the shipped
executable has no external asset files — levels are embedded into the binary at
build time.

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
  names**, and they are written down once, in [crew.c](src/crew.c) — see
  [The net](#the-net) below. Nothing in the simulation depends on which name a
  guard wears, but the manual's `THE CREW` sheet and every line the player
  overhears in a sector come off that one list, so a thirteenth name anywhere
  is a thirteenth man the docket does not have.
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
cutscene and the first dial agree to the minute; **00:22-00:57** the fifteen
sectors, `NIGHT_CLOCK_*` in [game_config.h](src/game_config.h) climbing the
dial at two and a half minutes a sector; **01:00** the helicopter, the outro's
own caption. A change to any one of them is a change to the two cutscene
captions, the `TRANSITION_INTEL` table, the manual's `THE NIGHT` sheet, the
drive's cordon caption, `NIGHT_CLOCK_FIRST_MINUTE` and both prose pages.
- **The ending** — the helicopter on the roof is the getaway, not a rescue.

Where the player actually reads it, in the order they meet it: the title
screen's one line; the abduction cutscene's captions; the drive's captions; the
opening cutscene outside the tower; **the `TRANSITION_INTEL` table in
[cutscene.c](src/cutscene.c)**, one line on the report between sectors;
**the crew's own net** ([crew.c](src/crew.c)), overheard while a sector is
being played; the manual's `THE NIGHT` and `THE CREW` sheets; and the outro. A
change to any of those is a change to all of them.

The last two of those are the ones that reach the player *while they are
playing* rather than between beats, and they carry the plot differently. The
intel table is Chuck working it out — one considered line, after the fact. The
net is the other side saying it themselves, in the room, with no idea he is in
it. Neither is a substitute for the other and neither may contradict the other.

**The building's height is forty floors**, and it is stated in three places
that have to agree: the title screen's tagline, the men shouting down off the
facade (`CHATTER_WALL`) and the prose in [README.md](README.md). Fifteen
sectors is the *route*, not the storey count — a sector is a stretch of the
climb, not a floor.

The table is indexed by finished sector, but only sectors that leave by a
**stair door** show a report at all — a window is a continuous physical route
onto the facade and cuts straight to the next sector. In the campaign as
shipped that is six reports, after sectors 1, 4, 5, 8, 9 and 14, and those six
carry the arc on their own. Adding or moving a facade sector therefore changes
which beats of the story are told, which is the one thing about the level
layout that reaches all the way into the script.

## Commands

```sh
make          # build ./chuck
make run      # build and launch
make debug    # build build/debug/chuck-debug (-O0 -g3 -DCHUCK_DEBUG)
make run-debug  # build and launch it; its title screen has the level picker
make editor   # build ./chuck-editor, the level editor
make run-editor # build and launch the editor
make test     # build and run the core test suite (build/core_tests)
make sanitize # rebuild game + tests with ASan/UBSan into build/sanitize
make app      # build dist/Chuck.app, universal and signed (macOS)
make notarize # notarize and staple it, and cut dist/Chuck-<version>.dmg
make clean    # remove build/, dist/, ./chuck and ./chuck-editor
```

The debug build is the only one with the level picker on its title screen
(`</>` or `[`/`]` to choose, `F5` to start there); the release build has no
`CHUCK_DEBUG` in it at all.

`./chuck --level N` boots straight into campaign sector N, skipping the title
screen and the prologue; it is what the editor's playtest button launches.

SDL3 must be discoverable through `pkg-config`. The **test binary links no SDL**
(`TEST_CFLAGS` omits the SDL flags), so `make test` works even where SDL3 is
unavailable, and it runs in well under a second.

There is no test filter: `tests/test_main.c` is one binary whose `main()` calls
every `test_*` function in sequence. To run a single test, temporarily comment
out the others in `main()`, or just run the whole suite. Failures are reported
by the `CHECK` macro as `file:line: check failed: <expr>` and the process exits 1.

## Architecture

### The SDL boundary

Two layers, and the split is the most important invariant in the codebase:

- **Application shell** (SDL-dependent): [main.c](src/main.c) (SDL callbacks),
  [game.c](src/game.c) (state machine, level loading, per-frame orchestration),
  [game_input.c](src/game_input.c), [game_render.c](src/game_render.c),
  [chase_render.c](src/chase_render.c), [audio.c](src/audio.c),
  [intro.c](src/intro.c), [manual.c](src/manual.c),
  [cutscene.c](src/cutscene.c), [pad_hint.c](src/pad_hint.c),
  [particle.c](src/particle.c). [crew.c](src/crew.c) and
  [credits.c](src/credits.c) sit on this side too and link no SDL, because they
  are tables of strings — but they are presentation, and no gameplay module may
  include them. See [The net](#the-net) and [The credits](#the-credits).
- **Gameplay core** (no SDL, no knowledge of `Game`): `src/gameplay_*.c`,
  [level.c](src/level.c), [level_route.c](src/level_route.c),
  [player.c](src/player.c), [enemy.c](src/enemy.c),
  [chase.c](src/chase.c), [rng.c](src/rng.c),
  [game_event.c](src/game_event.c). These only include each other plus libc.
  That is what makes them deterministic and directly testable.

There is a second SDL binary, the level editor in [editor/](editor/); see
[The level editor](#the-level-editor) below.

Gameplay code never plays a sound, spawns a particle, or shakes the camera
itself. It appends to `GameplayState.events` (a `GameEventBuffer`, see
[game_event.h](src/game_event.h)) via `game_events_sound`,
`gameplay_world_sound`, `game_events_particles`, `game_events_explosion`,
`game_events_camera_shake`, `gameplay_crew_chatter`. The shell drains that
buffer once per frame in
`dispatch_events` ([game.c:50](src/game.c#L50)) and turns events into audio and
presentation; the prologue pursuit reports its feedback through the same
function with its own buffer. Keep new gameplay feedback on this path — calling
`audio_play` from a gameplay module would both break the layering and break the
tests, which assert on emitted events.

### `Game` composition

[game.h](src/game.h) composes four areas: `PlatformState` (window, renderer,
audio), `CampaignState` (level index, lives, score, timers), `GameplayState`
(the whole simulation), `PresentationState` (camera, shake, particles, cutscene
and HUD animation state), plus the self-contained `Chase` used by the prologue.
Scene changes go through the single `game_enter_state`; starting a level goes
through the single `load_level`.

### Frame flow

`SDL_AppIterate` clamps `dt` to `MAX_FRAME_DT` → `game_update` clears the event
buffer, reads input, then `update_scene`. If `update_scene` returns true the
frame was consumed by a non-playing state (intro, the prologue drive, cutscenes,
transitions, game over); otherwise `update_playing` runs the simulation. Events
are dispatched last.

**That clamp is a collision invariant, not a stutter guard.** Every projectile
tests the tile under its leading edge *after* it has moved rather than sweeping
the path it crossed, so it is only correct while one step is shorter than one
tile. (Against *entities* a round is swept: a shot fired up a ladder is four
pixels by eight and a dog is sixteen tall, so at the clamp the two together are
shorter than one step and a destination-only test walks the round straight
through the animal. The tile test stays a point test because the asserts below
prove it; nothing proves an entity is a tile wide.)
`MAX_FRAME_DT` is therefore written as `1 / MIN_FRAME_RATE` — a whole
number of steps per second, so the `_Static_assert`s beside the projectile
speeds in [game_config.h](src/game_config.h) can be integer constant
expressions under `-Wpedantic`. Raising `BULLET_SPEED` past a tile per frame is
now a build failure rather than shots quietly passing through one-tile walls.

**`MAX_FALL_SPEED` is on that line too, and it is the tightest number on it.**
`level_move` resolves the vertical axis exactly the way a projectile resolves a
tile — one row tested under the leading edge *after* the step — so a body
falling further than a tile in one frame drops through a one-tile floor. Every
falling thing in the game is clamped to this one speed (the player, guards,
dogs, crates, grenades, magazines, settling bodies, and the bricks thrown off
the facade), so one assertion covers all of them; the facade brick is clamped
to it explicitly in [gameplay_climb.c](src/gameplay_climb.c), because uncapped
it passed a tile per step after about a second of fall and sailed through the
cornices it is supposed to burst on. At 620 against a 32px tile the margin is
one pixel, which is exactly why the assertion is worth having: the number reads
like a free tuning knob and it is not one.

The scene order the player walks through is `STATE_INTRO` → `STATE_ABDUCTION`
→ `STATE_CHASE` → `STATE_OPENING_CUTSCENE` → level one, with `STATE_MANUAL`
hanging off the title screen as a dead end that only leads back to it. The far
end of the campaign closes the same loop: the last sector → `STATE_LEVEL_CLEARED`
→ `STATE_OUTRO` → `STATE_CREDITS` → `STATE_INTRO` again, so a finished run lands
where a new one starts rather than parking on a card somebody has to dismiss.
The chase branch owns its own event dispatch and camera-shake tick because those
normally run only on playing frames.

`update_playing` ([game.c:686](src/game.c#L686)) has a deliberate ordering:
terminal hold → player physics → elevators/falling/moving platforms → crates →
platform carry & snap → doors and sublevel travel → AI spawns → player attack →
AI movement → item pickup → hazards → player bullets → AI combat → enemy
bullets → contact damage → alarm countdown (**after** perception, so a guard
seeing Chuck on the final frame keeps the alarm alive) → exit check → camera
lerp. Reordering these has caused real bugs; several tests pin the resulting
behavior.

### Hearts, damage and the two real deaths

The player has hearts (`Player.hp`, `PLAYER_MAX_HP`), and the rule is one
sentence: **what hits you costs hearts, what crushes you or breaks your fall
kills you.** Ordinary contact — a guard, a bullet, a bite, spikes, a fan, a
brick, a bird — goes through `gameplay_damage_player`
([gameplay_world.c](src/gameplay_world.c)): one heart (explosions cost
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
[game_render.c](src/game_render.c) lay the same figure along the floor — dead
visor, no health pips, no speech bubble — and three consequences follow.
`settle_body` in [gameplay_ai.c](src/gameplay_ai.c) drops a body that died in
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

**Only the magazine comes back.** `ITEM_GUN` is the one pickup on
`ITEM_RESPAWN_TIME` (`gameplay_collect_items` in
[gameplay_interaction.c](src/gameplay_interaction.c)), because the sidearm is
what a sector is played with and a player who has spent it must not be left
walking the rest of the floor with a knife. The grenade used to regrow with it,
which made a single `N` an unlimited supply at ten seconds apiece — enough to
clear a floor a blast at a time, and enough to open every `%` in the campaign
without the bazooka those patches were placed for. A one-shot explosive that
regrows is not a decision about when to spend it;
`test_only_the_magazine_comes_back` pins both halves.

### Stomping a guard

Walking into a guard costs a heart, but `gameplay_combat_check_contacts`
([gameplay_combat.c](src/gameplay_combat.c#L961))
carves out one free answer: landing on its head. It tells a stomp from a side
collision without swept collision by comparing penetration depth on each
axis — a falling player (`vy > 0`) whose vertical overlap with the guard is
shallower than the horizontal overlap only just tagged the top of the box, so
it bounces Chuck upward (`ENEMY_STOMP_BOUNCE_SPEED`) and calls the same
`damage_enemy` a bullet or knife hit would, instead of hurting him. Dogs are
unaffected; only guards can be stomped. The bounce also clears
`jump_cut_ok`, because it is not a player-started jump: releasing the jump
key must never shorten it back down into the guard.

A stomp lands mid-climb as often as mid-jump, and that case needs its own
fix: the ladder branch of `player_update` ([player.c](src/player.c)) sets `vy`
from the climb input every frame, so a bounce set while `on_ladder` is true
would be overwritten the very next frame by the climb speed, driving Chuck
back down into what now reads as a deep side hit. The stomp handler
also clears `on_ladder` and arms `ladder_lockout_timer`
(`ENEMY_STOMP_LADDER_LOCKOUT`) so the ladder cannot be re-grabbed until the
bounce has had time to actually clear the guard.

### One blast, one rule

Four things explode — a mine, a grenade, a rocket and a gas canister — and they
differ in exactly three ways: where they go off, how far they reach, and how
hard they shake the frame. What a blast *does* is one function,
`apply_blast` in [gameplay_combat.c](src/gameplay_combat.c), and each
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
(`gameplay_spawn_ammo_drop`), and an explosion destroys it with its owner.
Canisters chain, and the chain always terminates because a canister is
deactivated *before* its own blast is applied. And the player can only be hurt
once however many blasts a chain sets off, because the first one opens the
mercy window that `gameplay_damage_player` checks on entry.

Only the player's weight arms a mine, but the delay between the step and the
blast is long enough to run out of — and long enough for whoever is chasing him
to run into.

### Forgiving input, checkpoints, continues

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

Progress is banked and a death resumes at it. Facade climbs bank height every
`FACADE_CHECKPOINT_STEP`; interiors bank at real progress — any key card, a
finished hack, a teleport door, a medkit (`gameplay_bank_checkpoint`, called
from [gameplay_interaction.c](src/gameplay_interaction.c)) — and
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
that it costs it (`campaign_accept_continue`). The campaign never returns to
level one uninvited. The score itself now pays out: every
`EXTRA_LIFE_SCORE_STEP` points is an extra life
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

### The options sheet

Everything the player is allowed to decide is one struct and one table, both in
[settings.c](src/settings.c) / [settings.h](src/settings.h): two audio levels,
fullscreen and the CRT filter, and the three assist switches. The sheet opens
from the title screen or from pause (`J`, or X on a pad) and returns to
whichever opened it. It replaced a three-switch assist sheet, an `M` key and a
fullscreen key that were the whole of the game's settings, and four decisions
carry it.

**The table is the sheet.** A `SettingRow` names a value, says one sentence
about it and says whether it is a level or a switch, and `draw_settings_sheet`
in [game_render.c](src/game_render.c) draws whatever it finds there — including
sizing the plate from the rows, so a new section costs no layout. A setting in
the struct and not in the table is one nobody can reach; a row naming a value
the struct does not hold will not compile. The cursor steps over headings, and
`test_settings_cursor_only_lands_on_rows` walks two laps each way to prove it
and to prove every reachable row explains itself.

**The module links no SDL**, the way [crew.c](src/crew.c) does not, so the test
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
`Settings.assist` and are handed to the gameplay core as two flags applied at
level load (`assist_more_hearts`, `assist_slow_enemies` — read through
`gameplay_player_max_hp` / `gameplay_enemy_speed_scale`), so the gameplay core
stays deterministic and menu-free.

**Music and effects are separate buses**, in `AudioSystem.music_volume` and
`sfx_volume`, sitting on top of the mix rather than inside it: every effect and
every score is still built at the gain it was written with. Both default to
full, so a fresh install hears the mix everything was balanced at. `M` survives
as a kill switch on top of both and is deliberately **not** saved — a game that
starts silent with nothing on screen explaining why is a game the player thinks
is broken. The pad's old mute (Y from pause) is gone: a pad muting to silence
while the sheet beside it still read 100 was two answers to the same question,
and a pad now reaches both levels in two presses.

### The letter on the button

SDL names a face button by its **position** — `SDL_GAMEPAD_BUTTON_SOUTH` is the
bottom one on every pad ever made — and binding straight to those positions is
what the game used to do. It is right on an Xbox pad and quietly wrong on a
Nintendo one, which prints A where an Xbox pad prints B: the title screen asked
for A, and the button printed A was the one that quit the game.

So the game binds by **letter**, in [pad_hint.c](src/pad_hint.c). A pad is asked
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
  It does not close the game from the title screen, because the player did not
  open the game the way they opened a sheet, and a first press of B on the
  first screen ending the session is the worst version of that mistake;
  quitting is `ESC` or the window's close box. During a sector, a cutscene,
  the report between sectors, the continue prompt or the drive it does nothing
  at all: dropping a run on one press of the button players use to say "not
  that" is the same bug wearing a hat. The way out of a run is deliberate and
  from the pause screen only (SELECT, `abandons_run`).

  **`ESC` keeps the same promise, and the list of states it keeps it in is
  written out in [main.c](src/main.c) rather than implied.** It pauses whatever
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
  gamepad illustration (`illus_controls` in [manual.c](src/manual.c)) is the one
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
which is why `E` and the `UP`/`W` jump are gated on `STATE_PLAYING` in
[game_input.c](src/game_input.c) alongside `LSHIFT`. Ungated, `E` reached the
drive — which reads `use_door` as the skip the pad puts on Y — and so handed
the keyboard a second way past the prologue that the drive's own prompt, which
asks for `ENTER`/`SPACE`, never mentions. The `UP` branch has a second reason:
it tests for a ladder by reading the live player out of `game->gameplay`, and
outside a sector that is whatever the last one left behind.

### The prologue: three beats, one shot

Pressing START plays the whole abduction before the platformer begins, in
three scenes that are staged to read as one continuous take. All three are
skippable with confirm.

**The kerb** (`STATE_ABDUCTION`, `abduction_cutscene_*` in
[cutscene.c](src/cutscene.c)) is the beat the campaign hangs off: Chuck's car
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

**The drive** ([chase.c](src/chase.c)) is a top-down, forward-only car chase:
Chuck tails the SUV through night traffic until it parks at the building the
first level opens in. It is a gameplay-core module — no SDL, seeded `Rng`, its own
`GameEventBuffer` — and [chase_render.c](src/chase_render.c) is the only part
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

### The field manual

`H` on the title screen (`Y` on a pad, or a click on the line naming it) opens
`STATE_MANUAL`: eight sheets in [manual.c](src/manual.c) that say who is in
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

It **replaced the title screen's row of control hints** rather than joining it,
and that is a composition decision as much as an editorial one: a plate, a
second plate under it and a keycap row under that left the bottom eighty pixels
of the shot carrying three bands of interface, which reads as a menu stacked on
a picture. The hints were only ever there because there was nowhere else to
learn the controls; the manual is that place, and it is named on the line they
used to occupy. That line now carries two chips — the manual and the assist
options — centred together as one line of things to know about, at the same
hint weight; it is still one band. Anything added to the title screen from
here owes the same question — the shot holds the wordmark, START and one quiet
line, and a fourth element has to earn a band of its own.

Three decisions carry it, and they are the reason it is one file.

**The text is a table.** Every sheet is a `ManualPage` — a title, a strap, a
list of typed lines and one illustration — and the line kinds (`LINE_HEAD`,
`LINE_BODY`, `LINE_BULLET`, `LINE_KEY`, `LINE_GAP`) are the whole layout
language. A rule that changes in [game_config.h](src/game_config.h) is a string
edited in one place rather than a paragraph hunted through a draw function, and
the control rows are `key|pad|action` so the two chip columns can be sized from
the widest label on the sheet instead of per row. **Prose that names a button
takes the same bar**, as `pad wording|keyboard wording` — the paragraphs used to
spell `E` at everybody, which is exactly what the `$` tokens exist to prevent,
said to the one reader with no E to press. A line with no bar in it is printed
as written, which is every line naming no button.

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
[level_art.c](src/level_art.c) draws them. A manual illustrated in a second
style would be a manual for a different game. Two things that cost a revision
each and are worth keeping: a caption is set from the panel's left edge, so it
is clipped to `CAPTION_MAX` rather than trusted to be short — a line that
outgrows the plate runs off the sheet — and `dash_arc` takes the height the
curve actually reaches, because a quadratic only rises halfway to its handle
and an arc drawn through the handle leaves the figure hanging above its own
jump.

### The credits

The outro holds its thank-you frame for the rest of `OUTRO_CUTSCENE_DURATION`
and then hands the frame to `STATE_CREDITS`: the names rise through the same
letterboxed, grainy frame the rescue ended in, and when the roll runs out the
game is back on the title screen. That last part is why it is a state and not a
card bolted onto the outro — a finished campaign used to park on a frame the
player had to dismiss. Confirm means "get on with it" while the names are
moving and "done" once they have stopped; `ESC` leaves at any point, and the
pad's B is inert here exactly as it is during a cutscene.

**The table is the roll.** [credits.c](src/credits.c) links no SDL, for the
same reason [crew.c](src/crew.c) and [settings.c](src/settings.c) do not: it is
a table of typed lines — `CREDIT_TITLE`, `CREDIT_ROLE`, `CREDIT_NAME`,
`CREDIT_NOTE`, `CREDIT_RULE`, `CREDIT_GAP` — plus the height and the scale each
kind stands at, and `draw_credits_roll` in [game_render.c](src/game_render.c)
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

### Determinism and RNG

Gameplay randomness uses the explicitly seeded `Rng`
([rng.h](src/rng.h)) held in `GameplayState`; `game_init_seeded` lets tests fix
the seed. The chase seeds its own `Rng` from that stream when the state is
entered, so one game seed still decides the drive and every level after it. `SDL_rand` is reserved for purely visual effects (camera shake,
particles). Do not introduce `SDL_rand`, `rand()`, or wall-clock reads into
gameplay modules — reproducibility is a tested property.

`gameplay_state_begin_level` wipes all per-level simulation state while
preserving the RNG stream; `test_gameplay_reset_preserves_rng_only` enforces it,
so any new `GameplayState` field must be cleared there.

### Levels

`Level` ([level.h](src/level.h)) separates `LevelMap` (immutable parsed data),
`LevelRuntime` (mutable per-run: items, crates, elevators, unlock flags, which
weak walls have been blown open), and
`LevelReveal` (the tile-by-tile reveal animation). `level_load_data` parses the
text grid; it also makes the seeded choices — which key card is the real one and
which terminal is active (kept at least `TERMINAL_MIN_START_TILES` from the
player start).

Maps live as text in `levels/level*.txt` (campaign, natural-sorted) and
`levels/sublevels/*.txt`. [tools/embed_levels.py](tools/embed_levels.py) turns
them into `build/embedded_levels.c` on every build. **Adding
`levels/level16.txt` is all the *build* needs for a new campaign level** — the
Makefile wildcards it in and progression is driven by `EMBEDDED_LEVEL_COUNT`.
A level is scored by its theme, not by its index, so the new sector's music
comes with the `THEME` line. Maps are text and can be edited as text, but
`make editor` is the tool that knows the rules — see
[The level editor](#the-level-editor).

What the *campaign* needs of it is a longer list, and `make test` is where it
is written down: a size no other sector already has, a storey rhythm no other
sector already has, a hazard budget above the sector before it, a theme
different from its neighbour's, and a route the conservative model can walk. A
sixteenth sector is therefore an interior — sector 15 has no `Y`, so the
alternation puts an interior next, and `test_all_embedded_levels_parse` pins
`facade_levels == 4` outright.

**A fifth climb is not a map away, it is a constant away.** The test also
requires each climb to be taller than the last, and the four run 40, 44, 46 and
48 rows — level 13 is standing on `MAX_LEVEL_HEIGHT` itself. Another facade
sector means raising that cap first (it sizes `LevelMap`, so it is a memory
decision as much as an authoring one) and then relaxing the two rules above.
The editor already says so, as a note on level 13.

### Walls that open

A `%` tile is a weak wall: a blocked-up opening that is solid in every way a `#`
is until a blast takes it out. Three decisions carry the whole feature, and each
is tested.

**One solidity rule.** `level_is_solid` is the only thing that knows a weak wall
can stop being one, and everything that collides, shades, blocks a bullet or
breaks a line of sight already went through it. So opening a wall opens it for
the player, the guards, the crates, the ambient NPCs, the lighting pass and the
ambient occlusion in the same frame, and there is no second list of places to
update. Where a module had its own copy of the rule it now calls
`level_is_solid` instead ([gameplay_climb.c](src/gameplay_climb.c)'s facade
collision, the janitor and receptionist probes in
[gameplay_ai.c](src/gameplay_ai.c)) — a tile that is solid to the player and air
to a guard is a bug however it is drawn.

**The hole is runtime, not map.** `LevelMap` stays exactly what the file said, so
the editor, the parser and the tests all keep one answer for what a sector is;
the opened tiles live in `LevelRuntime.wall_broken` beside the fallen panels and
the broken crates. A lost life therefore keeps the hole and reloading the sector
restores the wall, which is the same bargain `F` panels make.

**Only a blast opens one**, and gameplay code never plays the sound itself:
`gameplay_break_walls_in_radius` ([gameplay_world.c](src/gameplay_world.c)) is
called from `apply_blast` in [gameplay_combat.c](src/gameplay_combat.c) — see
[One blast, one rule](#one-blast-one-rule) — and reports one `SFX_WALL_BREAK`
per blast plus `GAME_EVENT_DUST` per tile.
Dust is a new event rather than the existing spark burst because masonry is not
blood: sparks arcing away from a broken wall read as the wrong material however
many of them there are.

The route model in [level_route.c](src/level_route.c) counts a weak wall as wall
in both directions — impassable, because opening one costs an explosive the
model knows nothing about, and floor, because a patch set into a slab must not
cut the storey in two. That is what keeps a `%` a shortcut and never the way
out, and it means placing one where a wall already stood cannot change whether a
sector is solvable. The editor adds the two rules the model cannot see: a sector
with a patch needs a grenade or a bazooka in it, and a patch on a climb never
opens at all, because nothing out there can set off a blast.

### One plan per sector

The campaign used to be one floor plan fifteen times: a sealed rectangle
stacked out of "slab plus two open rows" storeys, drilled with ladder columns
and sprinkled with props at a constant density. Levels 1 and 2, 10 and 14, and
12 and 15 had byte-identical storey rhythms, so the sectors could only differ
in width and in how much was in them.

Every sector now has a plan that belongs to its theme — a lobby atrium,
partitioned office floors, serpentine server aisles, catwalk towers, a galley,
a spine of sealed bays, shelving canyons, a ring around a bunker, branching
crawl ducts, a symmetrical suite, a rooftop skyline — and
`test_campaign_levels_are_distinct_and_solvable`
([tests/test_main.c](tests/test_main.c)) pins three things the parser cannot
see: no two sectors share a size or a storey rhythm, the hazard budget rises
strictly from sector to sector (and from climb to climb, along with the climb's
height), and a conservative model of the player can reach the way out, every
key card, every terminal and the restroom door without ever being stranded by a
one-way drop. That model never stands on a falling panel, so a sector has to
work once every `F` has gone, and it never walks through a weak wall, so a
sector has to work before any `%` has been opened. [levels/LEGEND.md](levels/LEGEND.md)
tabulates the plans, the budgets and what the model will and will not do.

That route model lives in [level_route.c](src/level_route.c) rather than in the
test file, because the editor asks it the same question about a map that is
still being drawn. Two copies would drift, and a sector the editor calls
solvable that `make test` then rejects is worse than no check at all.

### Level themes

A `THEME <name>` metadata line picks the level's art direction and its score;
the palettes, wall materials and parallax backdrops all live in
[level_art.c](src/level_art.c), which reads nothing but the immutable
`LevelMap` and so can never change how a level plays. Fifteen sectors of one
building would otherwise be fifteen runs down the same corridor, so every
campaign level names a different theme — a lobby, an office floor, a server
hall, an archive, a plenum, and four exterior climbs at different hours. Every
theme name, what it draws and what it sounds like is tabulated in
[levels/LEGEND.md](levels/LEGEND.md). A server aisle and a rooftop are not the
same place; one loop for the whole building would say they were, so the same
table that gives a sector its palette gives it its music
(`level_theme_music`).

Two properties are pinned by `test_campaign_themes_keep_changing`: no two
consecutive levels wear the same theme, and facade levels use the `FACADE_*`
themes while interiors never do. A map with no `THEME` line still loads with
its mode's default, so a new sector works before it has a look of its own;
a misspelt name is a parse error. **New tuning belongs in the theme table, not
in `game_render.c`** — a colour hard-coded in a draw function is a colour the
other fourteen sectors cannot change.

The campaign is fifteen levels that alternate interior sectors with exterior
climbs: levels 3, 7, 11 and 13 are `MODE FACADE`, and each is entered through
the `Y` window of the sector below it, whose `E` stair door is welded shut.
Every other level ends at a normal `E`. Four sectors (1, 5, 9 and 14) have a
`U` into the restroom, and every **even-numbered sector** — 2, 4, 6, 8, 10, 12
and 14, which is the odd-numbered *index* the test counts from zero — carries
exactly one bazooka. Say it as the sector number wherever a player will read
it: the manual spent a while telling them to look in the odd ones, which is the
half of the campaign that has no `Z` in it at all.
`test_all_embedded_levels_parse` pins that shape, so a new level has
to keep it: the alternation, the campaign ending inside the building, and no
rocket left out on a wall where nothing can be fired.

Every map character is documented in [levels/LEGEND.md](levels/LEGEND.md),
along with the authoring rules the geometry has to respect (jump reach, spike
and fan clearance, gap widths); keep both in sync when touching the parser. An
optional trailing `SPAWNS n0 n1 ...` line gives per-door spawn counts and must
list exactly one number per door.

### The facade climb

Levels flagged `MODE FACADE` are climbed, not walked
([gameplay_climb.c](src/gameplay_climb.c)). Four things make the wall a route
rather than a straight line up, and each is tested:

- **Masonry.** `#` tiles are stone cornices the climber collides with
  (axis-separated, so he slides along a ledge instead of sticking to it). They
  are also cover: thrown objects shatter on them and birds break off against
  them. Because the player box is exactly one tile tall, a lone solid tile
  inside a two-row band would seal the band — see
  [levels/LEGEND.md](levels/LEGEND.md); plant is painted on cornices instead.
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
wall are real detours whose loadout carries into the next sector.

### Sublevels

`Game` holds two `GameplayState`s: `gameplay` (active) and `inactive_gameplay`.
Entering the WC door swaps them (`swap_gameplay_areas`), so the parent level is
frozen intact rather than reloaded, and only the player's loadout crosses over
(`transfer_player_loadout`). Sublevel doors (`U`/`R`) are a separate mechanism
from the paired teleport doors (`D`), which are matched by index 0↔1, 2↔3, ….

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

The restroom is a full small level rather than a free item cache: a guard, an
ambient janitor, a shovable crate, a gas canister and a service catwalk reached
by ladder. What is up there is worth naming, because it is the whole reason to
take the detour and it is easy to under-report: a **medkit and a grenade**,
both past a one-tile gap that has to be jumped, with a magazine down on the
room floor. **One tile, and the width is load-bearing.** The catwalk band is
two rows, so the ceiling caps the jump at about 48px of ground
([levels/LEGEND.md](levels/LEGEND.md) writes the arithmetic out) — the medkit
spent a while sitting across a two-tile gap, which the route model calls
unreachable and which a player could in fact only cross inside a 25px window of
where they started the jump. A pickup the whole detour is sold on must not be a
timing trick, and `test_embedded_restroom_sublevel` walks the route model to
the medkit and the grenade now rather than only checking that they are high up.
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

### People who are not in the fight

Three kinds of NPC live in the gameplay core without taking part in it: the
ambient janitor (`J`), the fleeing civilian (`f`) and the receptionist (`k`),
all in [gameplay_ai.c](src/gameplay_ai.c) beside the guards. They collide with
the static map so they stay grounded, and that is the whole of their contact
with the simulation — no perception, no damage, no collision against the
player, no scoring. Guards do not see them and bullets pass through them. Keep
it that way: the moment one of them can be shot or can block a route, every
level holding one has to be re-solved.

Level 1 is the lobby they walked Ellen through, so it empties as Chuck walks
in: five civilians freeze, shout and run for the tile the player started on —
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

### Things that are only true tonight

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
  (`NIGHT_CLOCK_*` in [game_config.h](src/game_config.h)), so the minute hand
  climbs the face across the fifteen sectors and is nearly back at the top on
  the roof. It is the **one prop that hangs**, so it asks the tile above it for
  support where every other prop asks the tile below (`decoration_hangs` in
  [level.h](src/level.h), and `editor_symbol_hangs` on the editor's side of the
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
  [gameplay_ai.c](src/gameplay_ai.c)), because twelve men badged into one
  building under one contractor name are a crew running a schedule. It reuses
  the chat wholesale and **is** a chat with no partner — `enemy_on_radio` in
  [enemy.h](src/enemy.h) derives the pose and the sound from that rather than
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
  to the building (`cordon_side` in [chase.h](src/chase.h), drawn by
  `draw_cordon_car`); they stand in the cross street beyond the pavement and
  never in a lane, because a car the player's own car drives straight through
  is a bug rather than a detail, and
  `test_chase_cordon_thickens_toward_the_building` pins both the ramp and the
  quiet first blocks. **That ramp is distance from the tower, not time**, so
  the whole ring is already standing when the drive starts — which is why the
  broadcast is timed eight minutes ahead of the abduction rather than after it.
  `render_cordon_caption` in [chase_render.c](src/chase_render.c) names it once
  on the road, because otherwise the most legible object out there is scenery
  the drive never explains and the player reads as traffic to dodge. Out on the wall it is the same cordon from above:
  `facade_cordon` in [level_art.c](src/level_art.c) washes the lower face in
  out-of-phase blue and red, strongest on the first and lowest climb and gone
  entirely above the cloud deck, so the climb is also a climb away from it.
  `facade_news_helicopter` is the one aircraft allowed near the tower — a
  **news** ship, drawn behind the shell so it passes *behind* the face Chuck is
  on, because the helicopter waiting on the roof at the end of the night is the
  crew's ride out and nothing in the sky may contradict it.

Two colours are owned outside the palette for this, both named once with the
reason: `CORDON_BLUE` in [level_art.c](src/level_art.c) and `COL_BEACON_BLUE`
in [chase_render.c](src/chase_render.c) are the same emergency-beacon blue,
which `FX_CYAN` (technology) and `FX_LAMP` (a fluorescent tube) cannot supply.
The red half of a light bar is `FX_RED`, which is exactly what the palette's
danger red is for.

### The net

The crew talk, and now the player can read it. A guard alone calls in, a pair
standing together chat, whoever reaches a wall switch shouts, a thrower four
hundred feet up leans out of a window and shouts down, and the first person out
of the lobby shouts on the way — five beats the game already had as poses and
sounds, and none of which had ever said a word.
The words are in [crew.c](src/crew.c), one file, and the reason it is one file
is that a strip printing `KARL` while a manual sheet lists a different twelve
would be two answers to the same question.

`CHATTER_PANIC` is the odd one out and deliberately so: the people leaving the
lobby are not on anybody's docket, so it is the one kind that prints **without
a callsign** — a name on them would file them as staff the player is meant to
keep track of. Only the *first* of the five gets a line
(`civilian_begin_run` in [gameplay_ai.c](src/gameplay_ai.c)). They break over
`CIVILIAN_STARTLE_SPREAD`, which is shorter than `CHATTER_HOLD_TIME`, so five
speaking would replace the caption four times before anybody could read the
first — five half-second flashes read as a bug, not as a room emptying.

**The boundary is the whole design, and it is the same one a sound effect
crosses.** The simulation reports that somebody spoke — `GAME_EVENT_CHATTER`,
carrying a `ChatterKind`, an enemy slot and one opaque number drawn off the
level RNG in `gameplay_crew_chatter` ([gameplay_world.c](src/gameplay_world.c))
— and holds no string at all. The shell folds the number into a table and
spells it. That is what keeps a hundred lines of flavour text from being a
hundred edits to deterministic gameplay modules: writing a new one changes no
gameplay file, costs the RNG stream nothing, and cannot alter a single seeded
choice. `test_the_net_carries_words` pins it from the simulation's side.

Three rules follow, each of which cost something to learn:

- **A callsign is filed, never drawn.** `crew_callsign` maps an enemy slot to
  one of `CREW_SIZE` names and wraps. Drawing one would both spend a number out
  of the seeded stream — shifting every choice downstream of it — and make the
  same guard answer to a different name on a retry of the same sector, which
  is the one thing a name is for. On the facade there is no enemy array at all
  and the *window index* stands in: the men out there are the same crew.
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

### Tuning, art, audio

- **All tuning constants live in [game_config.h](src/game_config.h)** — speeds,
  ranges, cooldowns, entity caps, perception angles. Add new magic numbers there
  rather than inline.
- [fx.h](src/fx.h) is the shared palette and lighting vocabulary for every
  renderer (world, HUD, intro, cutscenes). Use its ramps instead of new literal
  colors so the screens stay one visual system. The accents are semantic and
  rationed — cyan is technology, amber is light and warning, red is danger,
  green is granted, `FX_RUST` is weathering (never danger), `FX_FLAME` /
  `FX_FLAME_HOT` are the one fire, `FX_LAMP`/`FX_WARM`/`FX_SODIUM` are the
  only three light temperatures, `FX_LABEL` is the one grey interface labels
  are set in. A renderer may keep a colour of its own only if it names it
  once, with a comment saying why the palette cannot supply it; a literal
  that repeats an fx.h value, or lands within a few units of one, is that
  constant misspelt. The heart and the ammo cartridge are drawn by
  `fx_heart`/`fx_ammo_pip` — one glyph across the HUD, the manual and the
  outro, because the player is asked to recognise them everywhere.
- **Every frame is finished exactly once, in `game_render`.** The vignette
  and scanlines are applied at the bottom of `game_render` and nowhere else,
  with two strengths and a rule between them: screens being *played*
  (the sector, the chase) get `FX_VIGNETTE_PLAY`, screens being *watched*
  (title, manual, cutscenes) get `FX_VIGNETTE_SCENE`; scanlines are
  `FX_SCANLINE_ALPHA` everywhere and the cutscenes add `fx_grain` inside
  their own render as their film texture. A renderer that finishes its own
  frame puts every overlay drawn after it (the pause sheet, the assist
  sheet, the debug picker) on top of the finish instead of under it, which
  is exactly the bug this rule replaced.
- [level_art.c](src/level_art.c) holds the per-level wall materials and
  backdrops. It is the only place a level's look is decided; the themes shift
  hue and value inside the fx.h system rather than inventing one per sector.
- **A material is not a lit solid, and the difference is three passes.** A wall
  drawn as plating, brick or ceramic and nothing else is a texture swatch, and
  a grid of swatches is what a flat tile layer looks like however good the
  swatch is. `level_art_wall_tile` therefore runs the material, then
  `wall_form_shading` over it, then the edges on top of that — in that order,
  because the arris along a floor is a highlight and a highlight that gets
  dimmed by the shading pass stops being one. The shading is broad patches of
  light and shade across the whole wall (`art_drift`, one smooth value per tile
  over a four-tile lattice), a mass falling away from its own surface
  (`tile_depth`, so a shell reads as the part standing in the room and the
  middle as the part behind it), and one light direction from the ceiling down,
  so each exposed face is shaded by the way it points. Everything a tile needs
  to know for this is in `tile_open_mask` — including where a slab ends and has
  to return its lip down the flank to show how thick it is.
- **The air beside a wall is lit too.** `render_world` walks the empty tiles
  and lays ambient occlusion against every face the air touches, not just the
  ceiling; the gradients overlap where two faces meet, so concave corners come
  out darker than either wall without being a special case. The same pass gives
  a floor a hard contact line and a soft bounce fading upward off it, scaled by
  the theme's `lamp_alpha` — the plenum has nothing to bounce and must not glow
  — and lands each ceiling fixture's cone in a pool on the first floor beneath
  it, because a beam that fades out in mid-air is a beam with nothing at the
  end of it.
- **A material's rhythm is separate from its texture.** The panel grid tells
  the player how big a panel is; only something on a longer module — a bolted
  stiffener every fourth course, a shadow-gap reveal every third, a brick header
  course every fifth, a day joint where one pour met the next — tells them how
  big the wall is, and a wall with no scale reads as wallpaper whatever it is
  made of.
- **Only repeating architecture belongs in a backdrop.** Every backdrop layer
  tiles at a fixed parallax period, and a sector is often barely wider than the
  window, so each repeat is on screen at once. A curtain wall or a rack row
  genuinely runs the length of a floor and tiles happily; one reception desk
  stamped every few hundred pixels reads as a bug. Unique furniture belongs in
  the map as decorations, where it is placed once. A one-off piece of
  _architecture_ — the lobby's street entrance — cannot move to the map,
  because a decoration sits in the world plane and would drift against the
  glazing it is set into; anchor it to a fixed point on its own layer instead
  (`lobby_entrance` in [level_art.c](src/level_art.c)), on a multiple of the
  layer's period so it lands on the grid the rest of the layer tiles to.
- **A figure is a mass, not a stack of rectangles.** A body built out of boxes
  reads as assembled however well each box is shaded, and the corners are the
  tell — four of them on every part. `fx_taper` takes one or two pixels off
  them, with the top and the bottom given separately because a body is not
  symmetrical about its waist: shoulders slope where a hem runs straight, a
  skull is domed where a jaw comes to a chin, an ankle is narrower than the sole
  under it. `sprite_body` runs the **outline** along the same taper a pixel
  further out, which is the part that matters — a rounded fill inside a square
  outline is still a box with something drawn in it. Anything laid over a form
  has to follow it too (`sprite_mass`): hair, a helmet, a cap, the shade along a
  jaw. A rectangle of hair puts the corners of the head straight back. Hair and
  helmets go on _after_ the face for the same reason, so their fill covers the
  face's own top outline row instead of being cut in half by it. Parts narrow
  enough that a chamfer would eat them whole — a forearm, a trouser leg — stay
  rectangular.
- **A figure is a lit solid too, and it is drawn out of the same three passes
  as a wall.** Every body block in [game_render.c](src/game_render.c) goes
  through `sprite_form`/`sprite_body` → `fx_form_block`/`fx_form_mass`, which
  lays the garment down, puts the crown the ceiling reaches on top of it, drops
  the underside into shade and
  runs one rim pixel down the _leading_ flank — the side the figure is facing,
  which at twenty-six pixels across is much of what says which way someone is
  turned. The trailing flank is deliberately left alone: it sits against the
  sprite's own outline, where a second dark column reads as a thicker outline
  rather than as a surface turning away. Both steps of the ramp come from
  `fx_ramp` (warm toward the light, cool into the shade) rather than from more
  literals, so a jacket cannot drift out of the lighting system it is drawn in.
  Limbs get the cylinder version of the same idea in `sprite_limb_segment` —
  outline, shaded underside, garment, one lit pixel along the top — and that one
  function is why the whole cast gained the treatment at once instead of each
  figure being hand-shaded.
- **A lit step is a value lift, not a mix toward cream.** `fx_ramp`'s bright end
  scales each channel through `fx_lit_step` — red fastest, blue slowest, so the
  ceiling lamp's warmth comes out of the gains themselves — instead of blending
  the garment toward a pale neutral. Every mix toward a neutral spends part of
  the colour's chroma, which put the least coloured pixels of a figure exactly
  where a thirty-two pixel body has to do its talking, and a cast lit that way
  reads grey in a grey room however bright the highlight is. The knee inside
  `fx_lit_step` is what keeps an already-pale garment — a white shirt — from
  clamping to a flat 255 the moment it is lit.
- **A figure is two values: the garment carries, the legs recede.** Chuck's
  trousers, the guards' fatigues, the janitor's work trousers and the
  receptionist's suit trousers all sit a long way under the torso above them,
  and the civilians were built that way from the start. Legs drawn a few steps
  under a jacket in the same hue give a figure no read at all at this size — it
  is one column of colour with a belt across it — where dropping them into the
  dark makes the torso the mass the eye lands on, which is how the cast is drawn
  in the cutscenes and in the rear-facing terminal pose. Anything new joining the
  cast owes the same gap.
- **The floor casts the shadow, not the boots.** `fx_contact_shadow` is a soft
  three-pass pool, and for the player `character_ground` finds the first solid
  tile _below_ him and puts it there, shrinking and thinning it with height. A
  hard slab pinned under the feet travels up with a jump and so states that the
  floor came along; the pool staying behind on the floor is most of what sells
  how high the jump was. Keep new figures on this path — the old flat
  `color_rect` under a sprite is a shape with a harder edge than anything else
  in the frame.
- **Weight is squash, stretch and dust, and none of it belongs to gameplay.**
  The figure draws out while it is in the air and compresses for a beat after
  the boots land; the shell derives that beat in `game.c` from the fall speed
  `player_update` already returns and parks it in `PresentationState`
  (`player_land_squash`), so no gameplay module has to know the figure squashes.
  Landings and footfalls also kick `PARTICLE_DUST` off the floor — pale, hanging
  and nearly weightless, as against the sparks the same system throws for blood.
- **A gait is a cycle, not a sine.** `draw_walking_leg` takes each leg's own
  place in the stride, spends the first half of it in stance tracking the ankle
  straight back under the body and the second half swinging it forward on an
  arc, and the other leg gets the same number half a turn along. A sine is
  slowest exactly where the foot should be carrying the figure fastest, which is
  what makes a sine-driven walk look like skating.
- **A traverse is not a climb, and one beat is all that separates them.** The
  rear-facing climbing pose in [game_render.c](src/game_render.c) spends its
  beat vertically — a hand and the opposite boot rise while the other pair hold
  — and a figure crossing the rungs sideways spends the same beat across them
  instead: the leading hand and boot reach out, the trailing pair gather across,
  the vertical alternation stops, and the body hangs back off the reach and
  rides forward over the gather. Vertical travel wins when both are held,
  because a pose saying both at once says neither. The clock is the same clock:
  `player_update` advances `anim_time` on a sideways ladder move as well as a
  climb (`test_ladder_side_step_advances_the_animation_clock`), and holds it
  still when a wall has stopped the shuffle — a pose that only moved with `vy`
  left the figure sliding off a ladder dragging one frozen grip.
- **A face is five rows, and every one of them has to earn its place.** Below
  the headband there is room for a brow the fringe shades, an eye, a nose that
  has to break the head's outline to be a profile at all, a mouth and a jaw —
  and the pupil goes at the _front_ of the white, because a dark pixel centred
  in it reads as two eyes seen head-on. `fx_blinking` closes the eye every few
  seconds from the animation clock alone, salted per figure so a room full of
  people never blinks in unison.
- **A muzzle flash lights the room.** `draw_muzzle_flash` puts an `fx_glow` at
  the muzzle before the bright rects go down. The brightest thing in the frame
  illuminating nothing around it is what makes a flash read as a decal stuck on
  the gun, and it lasts two frames, so it costs nothing anyone will notice.
- **An interior seen through glass carries its own values.** A view is only a
  view if something separates it from the room: a night sky lit brighter than
  the interior air turns a distant skyline into masonry standing in the hall,
  and towers drawn at the value of the air behind them disappear, leaving their
  lit windows floating like dirt on the screen. Keep the outside dark, let the
  lit windows carry it, and put one tinted veil over the opening.
- **The title screen is key art, not a menu over a diagram.**
  [intro.c](src/intro.c) is the first thing anyone sees, and it is built as one
  deep image — sky, two skylines, the mid-ground slabs, the tower, the wet
  street — where each plane sits a step darker or lighter than the plane behind
  it. Two rules it paid for: a foreground figure cannot be a silhouette when
  the ground plane is the darkest thing in the frame (Chuck keeps his colours,
  dimmed to night, and stands in the lamp's pool), and every window that is lit
  on the tower is asked for twice, once by the facade and once by the pavement
  reflecting it, so the two can never disagree.
- **The wordmark is a thing in the shot, not type over it.** It used to be a
  seven-by-nine bitmap font drawn at eight pixels a cell and filled with a
  cream-to-red gradient, which made it the one surface in the frame lit from
  nowhere — and a grid four times coarser than the picture behind it, so it read
  as a second, cheaper drawing pasted on. It is now five plates of steel bolted
  over the city and lit by the same moon as the tower: the game's own slate ramp
  for the material, the drift-and-edges passes a wall gets for the form, a warm
  bounce off the lit street on every underside, and rust bleeding out of the
  fixings. Two consequences worth keeping. The letterforms are convex polygons
  rasterised at one screen pixel rather than cells of a character grid, because
  that is what lets the K hold an even stroke down a straight diagonal and every
  corner carry the same cut. And the sweeping beam is weighted _away_ from the
  top faces (`take[]` in `mark_face_color`): they are already near cream, so a
  highlight spent there is a whiter white nobody sees, and the sweep has to land
  on the body and the flanks to read at all.
- **`SDL_RenderDebugText` is an 8x8 bitmap: draw it at scale 1.0 or a multiple
  of it.** Any other scale resamples the glyphs, and a line of mushy type
  cheapens a screen faster than anything else on it. If a row does not fit at
  1.0, cut words, not scale. The rule is about interface: text *painted into
  the world* — the WC plate on a door, a stencilled door number, the tower's
  nameplate — is signage, part of the art, and sits at whatever size the prop
  it is painted on demands.

  **But "signage" is not a licence to shrink, and it was being used as one.**
  Every painted string in the game is now on the 8px grid, because in each case
  the plate could be sized to the letters instead of the letters to the plate:
  the exit reader spelled `LOCK` at 0.65 of a scale in five-pixel glyphs that
  ran off their own screen and past the edge of the door, and the terminal
  spelled `LIVE`/`OPEN`/`FAIL`/`OFF` at 0.55, which at four pixels a glyph is
  not four words but four smears that happen to differ. Both carry state the
  player is meant to read, and both are two cells now — `GO`/`NO`/`--` on the
  door, `ON`/`OK`/`NO`/`--` on the terminal — which is what a card reader has
  ever shown anybody and what fits at the only size the font is sharp at. The
  terminal gave up three decorative keys to make room, and that is the trade
  the rule asks for: the readout was the only thing down there saying anything.
- Sound effects are synthesized once during `audio_init` and cached as PCM,
  replayed through a 16-voice pool. A new effect means: an entry in the
  `SoundEffect` enum in [sound_id.h](src/sound_id.h) (before `SFX_COUNT`) plus
  a case in `synth_sound` ([audio.c](src/audio.c)). Audio init failure is
  non-fatal by design — the game runs silently.
- **Music is one score per level theme**, and a score is a table row rather
  than a hand-sequenced routine: a `MusicPlan` in [audio.c](src/audio.c) names
  a key, a tempo, the 1/16 rhythms of each part and a colour (sweep, clank,
  sparkle, wind, tick, drip), and `synth_music_plan` reads the loop as four
  sections — a statement, a full one, a breakdown that hands the bar to the pad
  and the drone, and a last one that pushes hardest. Only the hand-written
  title theme is built during `audio_init`; a level's loop is built the first
  time it is asked for, and only the title theme, the current track and the one
  before it stay resident (eighteen forty-second loops would not). That is why
  the restroom can be scored as its own room — the door switches away and
  straight back without rebuilding the sector's music.
  `level_theme_music` ([level_art.c](src/level_art.c)) owns the theme-to-track
  mapping; because it is one to one, `test_campaign_themes_keep_changing`
  already pins that no two consecutive sectors share a score.

## The level editor

`make editor` builds `./chuck-editor` from [editor/](editor/). It is a separate
binary, but deliberately not a separate idea of what a level is: it links
[level.c](src/level.c) to parse the map, [level_art.c](src/level_art.c) to draw
it, and [level_route.c](src/level_route.c) to judge it. What the canvas shows is
what the game will show, and what the report says is what `make test` will say.
An editor with its own parser and its own opinion of "solvable" would be a
second source of truth about the campaign, and the one that is wrong would be
the one being used.

Four modules, and the split is by what needs SDL:

- [editor_doc.c](editor/editor_doc.c) — the document: a map _as characters_,
  not as a parsed `LevelMap`. A file says things a `LevelMap` cannot say back —
  a space against a `.`, a decoration the loader drops, an absent `THEME` line —
  so the editor keeps the text and hands it to `level_load_data` to find out
  what it means. Undo is two stacks of whole-grid snapshots.
- [editor_legend.c](editor/editor_legend.c) — every character in
  [levels/LEGEND.md](levels/LEGEND.md) as a table: name, the sentence the legend
  gives it, colour, which mode it belongs to. **Both files change together**;
  a character in one and not the other is either an unpaintable tile or a typo
  the editor calls an error.
- [editor_validate.c](editor/editor_validate.c) — the report. Structure the
  loader insists on, the caps in [game_config.h](src/game_config.h), the
  authoring rules in the legend, the route model, and the campaign-wide rules
  `test_all_embedded_levels_parse` and
  `test_campaign_levels_are_distinct_and_solvable` pin.
- [editor_app.c](editor/editor_app.c), [editor_ui.c](editor/editor_ui.c),
  [editor_render.c](editor/editor_render.c) — SDL: state and input, the chrome,
  the canvas.

The first three have no SDL in them, so `TEST_SOURCES` links them and the suite
pins two things the editor cannot be allowed to get wrong. `test_editor_round_trips_every_map_file`
requires that loading and saving every shipped map leaves the file byte
identical — the moment saving reflows a map, editing one sector rewrites it
wholesale and buries the actual change in the diff. `test_editor_report_reads_the_campaign`
requires that the editor reports zero errors for every sector already in the
tree, which is what keeps its rules and the test suite's rules the same rules.

`F5` saves, runs `make` and launches `./chuck --level N`. That switch
([main.c](src/main.c)) and `game_start_at_level` are the whole of the game-side
change; the debug level picker calls the same entry point.

## The shipped macOS app

`make app` builds `dist/Chuck.app`; `make notarize` gets it a ticket from Apple
and cuts the DMG. Everything either one needs is in [packaging/](packaging/),
and the whole of it exists to close the gap between a binary that runs *here*
and a binary that runs on somebody else's Mac. Four decisions carry it.

**`make` and `make app` do not link the same SDL, and they must not.** The
development build takes Homebrew's, which is right for a machine with Homebrew
on it and wrong for everybody else's: it is arm64 only and it is built for the
macOS it was poured on — `minos 26.0` as this was written — so a bundle wrapped
around it starts on this Mac and refuses to launch on any other, with an error
that names nothing the player can act on. The app is therefore built against
libsdl.org's own universal `SDL3.framework`, fetched into `vendor/` by
[packaging/fetch_sdl3.sh](packaging/fetch_sdl3.sh) with **the version and its
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
Apple will not notarize it. [packaging/notarize.sh](packaging/notarize.sh)
refuses to upload anything not signed with a Developer ID and prints how to get
one, rather than letting Apple reject it twenty minutes later.

**Both the app and the DMG are notarized and stapled.** A player may be handed
either, and Gatekeeper checks whichever they got; stapling writes the ticket
into the bundle so the first launch needs no network. Credentials live in a
notarytool keychain profile (`xcrun notarytool store-credentials`), never in the
repository.

The icon is drawn, not stored: [packaging/draw_icon.py](packaging/draw_icon.py)
paints the tower, its lit floors, the roof beacon and Chuck on the flank out of
the fx.h palette on a 128x128 grid, which is the same reason the levels' art is
procedural — the repository holds no binary assets, and an icon checked in as a
blob is one more thing that can drift from the palette everything else is drawn
in.

The version, the bundle identifier and the app name are written once, in
[version.h](src/version.h): the binary hands them to `SDL_SetAppMetadata` (so
the audio device, the window's owner and a crash report all name the game
rather than "SDL Application") and `build_app.sh` greps them out of the same
header for `Info.plist`. `CFBundleVersion` is the commit count, which is a
number that already rises with every build anybody is handed.

## Conventions

- C17, built with `-Wall -Wextra -Wpedantic`; the tree is warning-free, keep it
  that way. `make sanitize` should stay clean too.
- Allman braces, 4-space indent (`game_input.c` is legacy 2-space), `CHUCK_*_H`
  include guards, `/* */` comments used to explain _why_ a rule exists rather
  than restating the code.
- Adding a new gameplay `.c` file: the game build picks it up via
  `$(wildcard src/*.c)`, but `TEST_SOURCES` in the [Makefile](Makefile) is an
  explicit list — add the file there as well or the tests will fail to link.
  The editor wildcards `editor/*.c` but names the `src/` files it links, so a
  new dependency of the editor's goes in `EDITOR_SOURCES` too.
- Tests build levels from small inline map strings and drive the gameplay
  modules directly with a fixed seed, asserting on state and emitted events.
  New behavior in a gameplay module should get a test in that style.
