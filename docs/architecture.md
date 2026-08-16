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
terminal hold → player physics → elevators/falling/moving platforms → crates →
platform carry & snap → doors and sublevel travel → AI spawns → player attack →
AI movement → item pickup → hazards → player bullets → AI combat → enemy
bullets → contact damage → alarm countdown (**after** perception, so a guard
seeing Chuck on the final frame keeps the alarm alive) → exit check → camera
lerp. Reordering these has caused real bugs; several tests pin the resulting
behavior.

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

## The three renderers

The sector is drawn by three files, and the split is by what each one knows.

- **[render_sprite.c](../src/render_sprite.c)** — the vocabulary. The wrappers
  every draw call goes through, and the `sprite_*` family that is *how a figure
  is built*: forms with a pixel or two off the corners, the outline run along
  the same taper a pixel further out, and one light direction for all of it.
  Nothing here knows what it is drawing.
- **[render_figures.c](../src/render_figures.c)** — the cast. Chuck, the guards
  and their bodies, the dogs and theirs, the janitor, the civilians, the
  receptionist, and the brick and the bird thrown at a climber. They are one
  module because they are one drawing problem: every one of them comes through
  the vocabulary above, drops its legs a long way under its torso, and casts
  its shadow on the floor rather than under its own boots. A figure added here
  inherits all three; a figure added anywhere else would not.
- **[game_render.c](../src/game_render.c)** — the world the cast stands in, the
  props, the HUD, the overlays, and `game_render` itself, which is still the
  one place a frame is finished.

It was one 6,200-line file, which is 13% of the tree in the least-tested corner
of it: `make test` links no SDL and so could never reach a line of it. The
vocabulary being `static` is what had kept it that way — nothing could move out
without taking a copy of the lighting rules with it, and two copies of how a
figure is lit is two answers to the question. What holds this side of the split
now is `make smoke`, which boots the real binary through the title screen, all
fifteen sectors, every screen `--scene` names and every sheet of the manual
under ASan and UBSan, each for the whole of its own length; the split was made
only after that target existed, because a pure move in a file nothing exercises
is a pure move nobody can check.
