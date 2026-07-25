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
  [audio.c](src/audio.c), [intro.c](src/intro.c), [cutscene.c](src/cutscene.c),
  [particle.c](src/particle.c).
- **Gameplay core** (no SDL, no knowledge of `Game`): `src/gameplay_*.c`,
  [level.c](src/level.c), [player.c](src/player.c), [enemy.c](src/enemy.c),
  [rng.c](src/rng.c), [game_event.c](src/game_event.c). These only include each
  other plus libc. That is what makes them deterministic and directly testable.

Gameplay code never plays a sound, spawns a particle, or shakes the camera
itself. It appends to `GameplayState.events` (a `GameEventBuffer`, see
[game_event.h](src/game_event.h)) via `game_events_sound`,
`gameplay_world_sound`, `game_events_particles`, `game_events_explosion`,
`game_events_camera_shake`. The shell drains that buffer once per frame in
`dispatch_game_events` ([game.c:44](src/game.c#L44)) and turns events into audio
and presentation. Keep new gameplay feedback on this path — calling
`audio_play` from a gameplay module would both break the layering and break the
tests, which assert on emitted events.

### `Game` composition

[game.h](src/game.h) composes four areas: `PlatformState` (window, renderer,
audio), `CampaignState` (level index, lives, score, timers), `GameplayState`
(the whole simulation), `PresentationState` (camera, shake, particles, cutscene
and HUD animation state). Scene changes go through the single
`game_enter_state`; starting a level goes through the single `load_level`.

### Frame flow

`SDL_AppIterate` clamps `dt` to 0.05s → `game_update` clears the event buffer,
reads input, then `update_scene`. If `update_scene` returns true the frame was
consumed by a non-playing state (intro, cutscenes, transitions, game over);
otherwise `update_playing` runs the simulation. Events are dispatched last.

`update_playing` ([game.c:686](src/game.c#L686)) has a deliberate ordering:
terminal hold → player physics → elevators/falling/moving platforms → crates →
platform carry & snap → doors and sublevel travel → AI spawns → player attack →
AI movement → item pickup → hazards → player bullets → AI combat → enemy
bullets → contact damage → alarm countdown (**after** perception, so a guard
seeing Chuck on the final frame keeps the alarm alive) → exit check → camera
lerp. Reordering these has caused real bugs; several tests pin the resulting
behavior.

### Determinism and RNG

Gameplay randomness uses the explicitly seeded `Rng`
([rng.h](src/rng.h)) held in `GameplayState`; `game_init_seeded` lets tests fix
the seed. `SDL_rand` is reserved for purely visual effects (camera shake,
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
them into `build/embedded_levels.c` on every build. **Adding `levels/level3.txt`
is all that is needed for a new campaign level** — the Makefile wildcards it in
and progression is driven by `EMBEDDED_LEVEL_COUNT`. Level music cycles with
`MUSIC_LEVEL_ONE + index % (MUSIC_TRACK_COUNT - MUSIC_LEVEL_ONE)`.

Every map character is documented in [levels/LEGEND.md](levels/LEGEND.md); keep
it in sync when touching the parser. An optional trailing `SPAWNS n0 n1 ...`
line gives per-door spawn counts and must list exactly one number per door.

### Sublevels

`Game` holds two `GameplayState`s: `gameplay` (active) and `inactive_gameplay`.
Entering the WC door swaps them (`swap_gameplay_areas`), so the parent level is
frozen intact rather than reloaded, and only the player's loadout crosses over
(`transfer_player_loadout`). Sublevel doors (`U`/`R`) are a separate mechanism
from the paired teleport doors (`D`), which are matched by index 0↔1, 2↔3, ….

### Tuning, art, audio

- **All tuning constants live in [game_config.h](src/game_config.h)** — speeds,
  ranges, cooldowns, entity caps, perception angles. Add new magic numbers there
  rather than inline.
- [fx.h](src/fx.h) is the shared palette and lighting vocabulary for every
  renderer (world, HUD, intro, cutscenes). Use its ramps instead of new literal
  colors so the screens stay one visual system.
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
