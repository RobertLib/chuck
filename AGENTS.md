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
make test     # build and run the core test suite (build/core_tests)
make sanitize # rebuild game + tests with ASan/UBSan into build/sanitize
make clean    # remove build/ and ./chuck
```

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
  [intro.c](src/intro.c), [cutscene.c](src/cutscene.c),
  [particle.c](src/particle.c).
- **Gameplay core** (no SDL, no knowledge of `Game`): `src/gameplay_*.c`,
  [level.c](src/level.c), [player.c](src/player.c), [enemy.c](src/enemy.c),
  [chase.c](src/chase.c), [rng.c](src/rng.c),
  [game_event.c](src/game_event.c). These only include each other plus libc.
  That is what makes them deterministic and directly testable.

Gameplay code never plays a sound, spawns a particle, or shakes the camera
itself. It appends to `GameplayState.events` (a `GameEventBuffer`, see
[game_event.h](src/game_event.h)) via `game_events_sound`,
`gameplay_world_sound`, `game_events_particles`, `game_events_explosion`,
`game_events_camera_shake`. The shell drains that buffer once per frame in
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

`SDL_AppIterate` clamps `dt` to 0.05s → `game_update` clears the event buffer,
reads input, then `update_scene`. If `update_scene` returns true the frame was
consumed by a non-playing state (intro, the prologue drive, cutscenes,
transitions, game over); otherwise `update_playing` runs the simulation. Events
are dispatched last.

The scene order the player walks through is `STATE_INTRO` → `STATE_CHASE` →
`STATE_OPENING_CUTSCENE` → level one. The chase branch owns its own event
dispatch and camera-shake tick because those normally run only on playing
frames.

`update_playing` ([game.c:686](src/game.c#L686)) has a deliberate ordering:
terminal hold → player physics → elevators/falling/moving platforms → crates →
platform carry & snap → doors and sublevel travel → AI spawns → player attack →
AI movement → item pickup → hazards → player bullets → AI combat → enemy
bullets → contact damage → alarm countdown (**after** perception, so a guard
seeing Chuck on the final frame keeps the alarm alive) → exit check → camera
lerp. Reordering these has caused real bugs; several tests pin the resulting
behavior.

### The prologue pursuit

Pressing START drops the player into a top-down, forward-only car chase
([chase.c](src/chase.c)) before the platformer begins: Chuck tails the
kidnappers' SUV through night traffic until it parks at the building the first
level opens in. It is a gameplay-core module — no SDL, seeded `Rng`, its own
`GameEventBuffer` — and [chase_render.c](src/chase_render.c) is the only part
that touches SDL.

Road space is measured in pixels: `x` across the road, `y` along the driving
direction and growing forward, so screen-up is forward and the renderer needs no
world scale. Four phases run in order: `DEPARTURE` (the SUV pulls away, Chuck
runs to his car — skippable), `PURSUIT` (`CHASE_PURSUIT_DURATION` seconds of
driving), `ARRIVAL` (both cars brake onto their marks) and `DONE`, which is the
shell's cue to play the opening cutscene. Crashing out or letting the gap exceed
`CHASE_LOSE_GAP` only fails the attempt: `CHASE_PHASE_FAILED` restarts the drive
after a beat, so the prologue can never block the campaign.

Two rules keep it fair, and both are tested: traffic is never generated more
than `CHASE_MAX_CARS_ABREAST` cars wide, so at least two lanes are always open,
and the SUV holds a speed that keeps Chuck at arm's length once it is being
tailed, so holding the accelerator settles into a stable tail instead of ramming
the car his fiancée is in.

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
`LevelRuntime` (mutable per-run: items, crates, elevators, unlock flags), and
`LevelReveal` (the tile-by-tile reveal animation). `level_load_data` parses the
text grid; it also makes the seeded choices — which key card is the real one and
which terminal is active (kept at least `TERMINAL_MIN_START_TILES` from the
player start).

Maps live as text in `levels/level*.txt` (campaign, natural-sorted) and
`levels/sublevels/*.txt`. [tools/embed_levels.py](tools/embed_levels.py) turns
them into `build/embedded_levels.c` on every build. **Adding
`levels/level16.txt` is all that is needed for a new campaign level** — the
Makefile wildcards it in and progression is driven by `EMBEDDED_LEVEL_COUNT`.
Level music cycles with
`MUSIC_LEVEL_ONE + index % (MUSIC_TRACK_COUNT - MUSIC_LEVEL_ONE)`.

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
work once every `F` has gone. [levels/LEGEND.md](levels/LEGEND.md) tabulates
the plans, the budgets and what the model will and will not do.

### Level themes

A `THEME <name>` metadata line picks the level's art direction; the palettes,
wall materials and parallax backdrops all live in
[level_art.c](src/level_art.c), which reads nothing but the immutable
`LevelMap` and so can never change how a level plays. Fifteen sectors of one
building would otherwise be fifteen runs down the same corridor, so every
campaign level names a different theme — a lobby, an office floor, a server
hall, an archive, a plenum, and four exterior climbs at different hours. Every
theme name and what it draws is tabulated in
[levels/LEGEND.md](levels/LEGEND.md).

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
`U` into the restroom, and every odd-numbered index carries exactly one
bazooka. `test_all_embedded_levels_parse` pins that shape, so a new level has
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

The restroom is a full small level rather than a free item cache: a guard, an
ambient janitor, a shovable crate, a gas canister and a service catwalk reached
by ladder, with the medkit past a gap that has to be jumped. Its interior art
is derived from the map's own wall bounding box, so the room can be reshaped
without touching the renderer; a slab with open air above and below is drawn as
a railed catwalk rather than as the room's floor.

### Tuning, art, audio

- **All tuning constants live in [game_config.h](src/game_config.h)** — speeds,
  ranges, cooldowns, entity caps, perception angles. Add new magic numbers there
  rather than inline.
- [fx.h](src/fx.h) is the shared palette and lighting vocabulary for every
  renderer (world, HUD, intro, cutscenes). Use its ramps instead of new literal
  colors so the screens stay one visual system.
- [level_art.c](src/level_art.c) holds the per-level wall materials and
  backdrops. It is the only place a level's look is decided; the themes shift
  hue and value inside the fx.h system rather than inventing one per sector.
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
- **An interior seen through glass carries its own values.** A view is only a
  view if something separates it from the room: a night sky lit brighter than
  the interior air turns a distant skyline into masonry standing in the hall,
  and towers drawn at the value of the air behind them disappear, leaving their
  lit windows floating like dirt on the screen. Keep the outside dark, let the
  lit windows carry it, and put one tinted veil over the opening.
- Sound effects and the three music tracks are synthesized once during
  `audio_init` and cached as PCM, replayed through a 16-voice pool. A new effect
  means: an entry in the `SoundEffect` enum in
  [sound_id.h](src/sound_id.h) (before `SFX_COUNT`) plus a case in
  `synth_sound` ([audio.c](src/audio.c)). Audio init failure is non-fatal by
  design — the game runs silently.

## Conventions

- C17, built with `-Wall -Wextra -Wpedantic`; the tree is warning-free, keep it
  that way. `make sanitize` should stay clean too.
- Allman braces, 4-space indent (`game_input.c` is legacy 2-space), `CHUCK_*_H`
  include guards, `/* */` comments used to explain _why_ a rule exists rather
  than restating the code.
- Adding a new gameplay `.c` file: the game build picks it up via
  `$(wildcard src/*.c)`, but `TEST_SOURCES` in the [Makefile](Makefile) is an
  explicit list — add the file there as well or the tests will fail to link.
- Tests build levels from small inline map strings and drive the gameplay
  modules directly with a fixed seed, asserting on state and emitted events.
  New behavior in a gameplay module should get a test in that style.
