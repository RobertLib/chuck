# How a frame is put together

## `Game` composition

[game.h](../src/game.h) composes four areas: `PlatformState` (window, renderer,
audio), `CampaignState` (level index, lives, score, timers), `GameplayState`
(the whole simulation), `PresentationState` (camera, shake, particles, cutscene
and HUD animation state), plus the self-contained `Chase` used by the prologue.
Scene changes go through the single `game_enter_state`; starting a level goes
through the single `load_level`.

## Frame flow

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
speeds in [game_config.h](../src/game_config.h) can be integer constant
expressions under `-Wpedantic`. Raising `BULLET_SPEED` past a tile per frame is
now a build failure rather than shots quietly passing through one-tile walls.

**A guard's round is the one projectile that is not swept**, and it therefore
clears a second bar of its own. It is tested where it ended up rather than
against the ground it crossed, which is honest only while one step is shorter
than the smallest thing it could step over — Chuck crawling, eighteen pixels of
him under an eight-pixel round fired straight down a shaft. There are seven
pixels of margin at 380, which is exactly the kind of margin that gets spent by
a tuning pass nobody connects to a collision rule, so the assertion beside the
others says so: raising `ENEMY_BULLET_SPEED` past that line means sweeping the
enemy round first, the way the player's already is.

**And it now has a smaller target than Chuck**, which moved that margin from
seven pixels to one. A guard's round used to be tested against tiles, crates
and the player and nothing else, so a gas canister was neither cover nor a
target on this side of the fight: the round went through the steel as if it
were air, the player could not shelter behind one, and a guard could not set
off the cylinder he was standing beside. The manual teaches "crawl and shoot a
GAS CANISTER" as a rule about the world, and a rule only one of the two people
in the room obeys is a special case nothing on screen explains. What keeps this
from being a difficulty change is that the *low profile* crosses with it — a
guard aiming at a standing Chuck fires between `ENEMY_MUZZLE_MIN_Y_FACTOR` and
`ENEMY_MUZZLE_MAX_Y_FACTOR` of body height and passes over the cylinder exactly
as the player's standing shot does; it is the shot at a crawling Chuck that
comes in low enough to find it. A canister is twelve pixels across against
nineteen of travel, so `BULLET_W + GAS_CANISTER_W` is now the tightest of these
assertions and the one to check first when the speed moves.
`test_a_guard_s_round_sets_off_a_gas_canister` pins both heights.

**`MAX_FALL_SPEED` is on that line too, and it is the tightest number on it.**
`level_move` resolves the vertical axis exactly the way a projectile resolves a
tile — one row tested under the leading edge *after* the step — so a body
falling further than a tile in one frame drops through a one-tile floor. Every
falling thing in the game is clamped to this one speed (the player, guards,
dogs, crates, grenades, magazines, settling bodies, and the bricks thrown off
the facade), so one assertion covers all of them; the facade brick is clamped
to it explicitly in [gameplay_climb.c](../src/gameplay_climb.c), because uncapped
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

`update_playing` ([game.c](../src/game.c)) has a deliberate ordering:
terminal hold → player physics → body drag → elevators/falling/moving platforms
→ crates → platform carry & snap → doors and sublevel travel → AI spawns →
player attack → AI movement → item pickup → hazards → player bullets → bolts in
the air → AI combat → enemy bullets → contact damage → alarm countdown
(**after** perception, so a guard seeing Chuck on the final frame keeps the
alarm alive) → exit check → camera lerp. Reordering these has caused real bugs;
several tests pin the resulting behaviour.

Two of those positions are decisions rather than places they happened to fit.
**The body drag runs after the walk and after the terminal**: after the walk so
a corpse follows the step Chuck has just taken rather than the one before it,
and after the terminal because the two answer the same held button and the
console has first claim on it. **The bolts land before the perception pass**,
not after it, so a bolt coming down this frame is a noise the guards get to hear
this frame; ordered the other way it is a frame late, which is invisible on its
own and exactly what makes a mechanic feel unreliable. See
[Going quiet](gameplay.md#going-quiet).

A third is worth naming because it looks like a mistake. **The platform carry
runs before AI movement**, so a guard or a dog standing on a moving plate is
carried on the strength of where the *previous* frame's `level_move` left him,
against a plate that has already taken this frame's step. Per frame that is the
same displacement either way — it is one `vx * dt` — and a body the plate has
just left behind simply is not carried, which is the wanted answer. What it buys
is that "which plate is under this box" is asked in one place for every body
rather than after each of the three movement calls. It is also where the
*support* is not: `level_move` holds every body up on a plate on its own, in the
same block as the falling panel, and for a long time that was the only half
anybody but Chuck had — see `P` in [the legend](../levels/LEGEND.md).

## Determinism and RNG

Gameplay randomness uses the explicitly seeded `Rng`
([rng.h](../src/rng.h)) held in `GameplayState`; `game_init_seeded` lets tests fix
the seed. The chase seeds its own `Rng` from that stream when the state is
entered, so one game seed still decides the drive and every level after it. `SDL_rand` is reserved for purely visual effects (camera shake,
particles). Do not introduce `SDL_rand`, `rand()`, or wall-clock reads into
gameplay modules — reproducibility is a tested property.

`gameplay_state_begin_level` wipes all per-level simulation state while
preserving the RNG stream; `test_gameplay_reset_preserves_rng_only` enforces it,
so any new `GameplayState` field must be cleared there.

## The renderers

Every pixel the game draws goes through one file, and the sector is laid out
by four more, split by what each one knows.

- **[bbc.c](../src/bbc.c)** — the machine. A BBC Micro in MODE 2: eight
  colours, a grid of fat pixels four logical pixels across and two down,
  dither patterns, flashing colours, a palette that can be re-pointed, sprites
  written as rows of characters, and the system font. It is the only file that
  calls an SDL draw function, and `make lint`
  ([check_palette.py](../tools/check_palette.py)) fails the build on one
  anywhere else, or on an `SDL_Color` — the type of every colour this tree used
  to invent. Nothing here knows what it is drawing.
- **[bbc_cast.c](../src/bbc_cast.c)** — the cast as sprites, SDL-free and in
  `TEST_SOURCES`: Chuck in every pose, the guards and the heavy's plate, the
  dog, the man with the mop, staff and visitors, the gull, Ellen, Voss, the
  crew at the kerb, and every icon the HUD, the floor and the manual share. A
  figure that appears on more than one kind of screen is drawn here once,
  because the guard on the manual's sheet and the guard on the floor have to
  be the same man.
- **[render_figures.c](../src/render_figures.c)** — the cast in play: which
  frame a figure is on, from its state and the distance it has travelled, and
  where it stands, from its collision box. The aim telegraph, the wounded
  uniform, the dog's crouch before a bite, the flash charge told from a grenade
  at a glance.
- **[world_render.c](../src/world_render.c)** — the sector as a place: the
  ladders, doors, terminals, cameras, props, pickups and hazards, the climbs'
  windows and ledges, and the order the layers go down in. The walls and the
  room behind them are [level_art.c](../src/level_art.c)'s, in the theme's row
  of [level_paint.c](../src/level_paint.c).
- **[game_render.c](../src/game_render.c)** — everything over the world: the
  HUD, the crew's net, the prompts, the cards and the sheets, and
  `game_render` itself, which puts the frame in the window.

The camera is snapped to the BBC grid before the world is drawn and the dither
patterns are anchored to the world from the same snapped offset —
`bbc_set_pattern_origin(-cam_x, HUD_HEIGHT - cam_y)`, in world_render.c and
level_art.c alike — so a figure, the floor under it and the checkerboard on the
floor move together and nothing boils as the view scrolls.

It used to be one 6,200-line file, and then three files around a shared
vocabulary of lit forms and tapered outlines in a `render_sprite.c`, a palette
of thirty-eight hand-tuned colours in an `fx.h`, and a skeleton Chuck was drawn
from at any scale. The BBC look took all three away: a figure eight pixels by
sixteen in eight colours is drawn in frames, as every BBC game's were, and the
one rule left to share is the machine's.
