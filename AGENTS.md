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
make editor   # build ./chuck-editor, the level editor
make run-editor # build and launch the editor
make test     # build and run the core test suite (build/core_tests)
make sanitize # rebuild game + tests with ASan/UBSan into build/sanitize
make clean    # remove build/, ./chuck and ./chuck-editor
```

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
  [intro.c](src/intro.c), [cutscene.c](src/cutscene.c),
  [particle.c](src/particle.c).
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
`LevelRuntime` (mutable per-run: items, crates, elevators, unlock flags, which
weak walls have been blown open), and
`LevelReveal` (the tile-by-tile reveal animation). `level_load_data` parses the
text grid; it also makes the seeded choices — which key card is the real one and
which terminal is active (kept at least `TERMINAL_MIN_START_TILES` from the
player start).

Maps live as text in `levels/level*.txt` (campaign, natural-sorted) and
`levels/sublevels/*.txt`. [tools/embed_levels.py](tools/embed_levels.py) turns
them into `build/embedded_levels.c` on every build. **Adding
`levels/level16.txt` is all that is needed for a new campaign level** — the
Makefile wildcards it in and progression is driven by `EMBEDDED_LEVEL_COUNT`.
A level is scored by its theme, not by its index, so the new sector's music
comes with the `THEME` line. Maps are text and can be edited as text, but
`make editor` is the tool that knows the rules — see
[The level editor](#the-level-editor).

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
called from the four explosions in [gameplay_combat.c](src/gameplay_combat.c)
and reports one `SFX_WALL_BREAK` per blast plus `GAME_EVENT_DUST` per tile.
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

### People who are not in the fight

Three kinds of NPC live in the gameplay core without taking part in it: the
ambient janitor (`J`), the fleeing civilian (`f`) and the receptionist (`k`),
all in [gameplay_ai.c](src/gameplay_ai.c) beside the guards. They collide with
the static map so they stay grounded, and that is the whole of their contact
with the simulation — no perception, no damage, no collision against the
player, no scoring. Guards do not see them and bullets pass through them. Keep
it that way: the moment one of them can be shot or can block a route, every
level holding one has to be re-solved.

Level 1 is the lobby the kidnappers came through, so it empties as Chuck walks
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
  helmets go on *after* the face for the same reason, so their fill covers the
  face's own top outline row instead of being cut in half by it. Parts narrow
  enough that a chamfer would eat them whole — a forearm, a trouser leg — stay
  rectangular.
- **A figure is a lit solid too, and it is drawn out of the same three passes
  as a wall.** Every body block in [game_render.c](src/game_render.c) goes
  through `sprite_form`/`sprite_body` → `fx_form_block`/`fx_form_mass`, which
  lays the garment down, puts the crown the ceiling reaches on top of it, drops
  the underside into shade and
  runs one rim pixel down the *leading* flank — the side the figure is facing,
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
- **The floor casts the shadow, not the boots.** `fx_contact_shadow` is a soft
  three-pass pool, and for the player `character_ground` finds the first solid
  tile *below* him and puts it there, shrinking and thinning it with height. A
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
  and the pupil goes at the *front* of the white, because a dark pixel centred
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
  corner carry the same cut. And the sweeping beam is weighted *away* from the
  top faces (`take[]` in `mark_face_color`): they are already near cream, so a
  highlight spent there is a whiter white nobody sees, and the sweep has to land
  on the body and the flanks to read at all.
- **`SDL_RenderDebugText` is an 8x8 bitmap: draw it at scale 1.0 or a multiple
  of it.** Any other scale resamples the glyphs, and a line of mushy type
  cheapens a screen faster than anything else on it. If a row does not fit at
  1.0, cut words, not scale.
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

- [editor_doc.c](editor/editor_doc.c) — the document: a map *as characters*,
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
