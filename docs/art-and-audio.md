# Tuning, art, audio

## Tuning, art, audio

- **All tuning constants live in [game_config.h](../src/game_config.h)** — speeds,
  ranges, cooldowns, entity caps, perception angles. Add new magic numbers there
  rather than inline.
- **Every pixel in the game is drawn as a BBC Micro in MODE 2 would draw it.**
  Eight colours — black, red, green, yellow, blue, magenta, cyan, white, at full
  intensity on every gun — and fat pixels twice as wide as they are tall. The
  800x552 frame is a grid of 200 by 276 of them, four logical pixels across and
  two down, so a 32-pixel tile is eight BBC pixels by sixteen: the proportions
  of a MODE 2 sprite. [bbc.h](../src/bbc.h) is the only way a pixel reaches the
  frame and everything it draws lands on that grid, opaque, in one of the
  eight. There is no alpha, no gradient, no glow, no vignette and no scanline
  anywhere in the tree, because the machine had none of them, and **none of it
  is a filter**: nothing is drawn modern and then reduced. Every renderer draws
  in the eight colours on the grid to begin with.

  The one exception is type. The system font (`bbc_text`, a face in the manner
  of the Acorn MOS character set, in [bbc_art.c](../src/bbc_art.c)) is drawn at
  one logical pixel to a font pixel, which is MODE 1's text on a MODE 2 screen
  and is what keeps every line in the game the width it was measured at — the
  cell is the eight by eight `SDL_RenderDebugText` had, so not one fit check in
  the suite had to change. It is still drawn in the eight colours. Scales 1, 2,
  3 and 4 only; if a row does not fit at 1, cut words, not scale.

  **The rule is held by the output, not the source.** `tools/check_bbc_frame.py`
  reads a `--shot` capture pixel by pixel and fails on any colour that is not a
  corner of the RGB cube, which is the only way to catch a colour made at run
  time by blending two legal ones. [tools/soak.sh](../tools/soak.sh) runs it
  over a capture of every staged screen. It also reports how much of a frame is
  off the 4x2 grid and never fails on that: text is most of the figure.

- **What the machine had instead of light, every renderer uses instead of
  light.** Three things, all in [bbc.h](../src/bbc.h):

  - **Dither.** A BBC game's brown is red and yellow on a checkerboard and its
    grey is black and white; the eye mixes them at arm's length. `bbc_dither`
    fills a rectangle with two colours in a pattern (`CHECK`, `SPARSE`,
    `DENSE`, `ROWS`, `COLS`, `DOTS`), and `bbc_stipple` lays only the ink over
    what is already there. Patterns are anchored to the world through
    `bbc_set_pattern_origin`, or a checkerboard floor boils as the camera
    moves. Dither is for a *material* that has no colour of its own; it is
    never a way of fading something out.
  - **Flashing colours.** Logical colours 8 to 15 alternated with their
    complements at the flash rate, which is why every alarm on the machine
    blinked red and cyan. `bbc_flash(c)` is that, at the hardware's half
    second, and it holds steady under the player's reduced-motion switch.
    Alarms, a pickup that matters, a cursor, a fuse — anything that has to
    shout.
  - **The palette.** VDU 19 re-pointed a logical colour at a different
    physical one, everywhere on the screen at once. `bbc_palette_set` is that,
    and `bbc_palette_dim(n)` is a fade done the way a BBC did one: every colour
    one rung down a fixed ladder (white, cyan, blue, black; yellow, red,
    black), never a blend. A fade to black is three steps, a building whose
    lights go red is one palette change, and both are free.

- **Rooms are black.** The back wall of an interior is black with the room's
  own lines drawn on it in the theme's `backdrop` colour — blue, everywhere —
  sparsely: a window frame, a shelf edge, a panel seam. Masonry and the lip a
  foot lands on are saturated, never black and never the backdrop's colour,
  because those are what a player reads a floor plan from. The cast is drawn
  over all of it with no outline. That is the whole of the depth a BBC screen
  had, and it is enough: the eye reads bright on black as near and blue on
  black as far.

  **Buried masonry is one rung darker than its face.** A tile with masonry on
  all eight sides touches nothing anybody can stand on, walk past or jump
  under, so `level_art_wall_tile` draws it with the palette dimmed one step
  (`bbc_palette_dim(1)`, then put back): white marble goes cyan, yellow brick
  red, red plate black. Every material gets its darker self from the machine's
  own palette ladder rather than from a second drawing, and the slab over the
  lobby stops being the brightest thing on the first screen of the game.

  Each theme's eight-colour paint is one row of
  [level_paint.c](../src/level_paint.c), which both renderers that draw a floor
  read, and which is held by
  `test_every_theme_has_a_paint_it_can_be_seen_in` — every row visible, and no
  two sectors in a row in the same masonry, which is the cheapest way the climb
  has of saying it has moved on.

- **The colours mean things, and are rationed.** Cyan is technology and
  security, and Chuck — the one figure in cyan, so he is the first thing found
  on any screen. Yellow is light, warning and anything worth picking up. Red is
  danger and the enemy's fire. Green is access granted, and the guards' cloth.
  Magenta is the story — Voss, the docket, the penthouse. White is type, skin
  and the brightest thing on the screen: a muzzle, a blast. Blue is distance:
  the room behind, the city behind, the night. Black is everything else.

- **The cast is drawn once.** Every figure that appears on more than one kind of
  screen is a sprite in [bbc_cast.c](../src/bbc_cast.c): Chuck in every pose,
  the guards, the heavy's plate, the dog, the man with the mop, staff and
  visitors, the gull, Ellen, Voss, the crew at the kerb, and every icon the HUD,
  the floor and the manual share (the heart, the round, the card, the medkit,
  the three throwables, the rocket). A sprite is rows of characters, one to a
  BBC pixel — which is how every one of them was designed on squared paper in
  1983 and is still the only form a 7x16 figure can be read and edited in — and
  a slot character (`1` to `4`) is filled at draw time, so one drawing serves a
  guard and a heavy, or a car in five paints. Standing figures are eight by
  sixteen over their 26x32 boxes, face right, and stand on their bottom row,
  which is how `bbc_sprite_at_feet` places them. The standard paints (a guard,
  a wounded guard, the heavy, the dog, the crew, four kinds of civilian) are in
  the same file, because the guard on the manual's sheet and the guard on the
  floor have to be the same man.

  Animation is frames, not interpolation: a walk is three drawings, a climb is
  one drawing and its mirror, and a figure moves a whole BBC pixel at a time.
  The camera is snapped to the grid before anything is drawn, so a figure, the
  floor under it and the dither on the floor all move together.

- **Effects are pixels in the eight colours.** A muzzle flash is a white star
  for the frame it fires on. A blast is rings of yellow, red and white, and
  flashes. A particle is one BBC pixel. A flash charge whites out the screen
  for a beat. The alarm is red that flashes, and the lights going red is a
  palette change. Nothing glows, because nothing can.

- **A screen behind a menu is stippled, not dimmed.** A sheet opened over a
  scene clears its own plate to black and lays a black `CHECK` stipple over the
  rest of the frame — the BBC's shade, and the only kind there is. A panel is
  black with a one-pixel border in a colour, doubled for one that matters;
  headings are yellow or cyan, body text white, values yellow, danger red. The
  highlight is inverse video (`bbc_text_inverse`): a bar of colour with the text
  in black on it, which is what the machine's own menus did.

- **A backdrop belongs to its room, not to the screen.** `level_backdrop_plan`
  in [level.c](../src/level.c) reads the map into rooms, and a theme lays its
  wall out against `ArtRoom` — things stand on its floor, hang from its ceiling
  and are sized to its height — so it moves with the slabs on both axes.
  `test_the_back_wall_is_laid_out_per_room` holds the reading. **Only repeating
  architecture belongs in a backdrop**: a rack row or a curtain wall tiles
  happily, one reception desk stamped every few hundred pixels reads as a bug,
  and unique furniture belongs in the map as a decoration. **What a layer
  varies per repeat is keyed to the repeat** (`art_repeat` in
  [level_art.c](../src/level_art.c)) and never to where it is on the screen,
  or a skyline repaints its lights every time the camera moves — which no gate
  can see, because a still of it is a still of a skyline. And **a backdrop
  layer sinks as the climb rises, and never wraps** (`level_backdrop_sink`):
  a wrap is a snap, and a layer moving up past a wall moving down is moving
  the wrong way.

- **One tile has a front and a back, and it is the one something is inside.**
  Trunking is masonry to a man on his feet and a gap to a man on his elbows
  (see [The duct](gameplay.md#the-duct)), so its louvres are laid back over
  whichever of its tiles the player is in after the figures are down
  (`render_duct_fronts`). Drawn in one piece, a crawl through a shaft paints
  Chuck over the louvres, and the tile whose whole cost is that they are
  opaque both ways reads as a man crawling in front of them.

- **The heart and the round are the two glyphs a player is asked to recognise
  everywhere**, so they are drawn from the same sprite on every screen:
  `SPR_ICON_HEART`/`SPR_ICON_HEART_EMPTY` and `SPR_ICON_ROUND`/
  `SPR_ICON_ROUND_SPENT` in the HUD, the manual and the outro alike.

- **The finish is that there is none.** The frame is drawn into one offscreen
  texture at the logical size and copied to the window last, scaled as pixel
  art (`frame_prepare` in [game_render.c](../src/game_render.c)) — the one
  place a BBC pixel is ever resized, and therefore the one place that has to
  keep it square-edged. A scripted run's window is exactly the logical size
  (see the note on `GameRunKind` in AGENTS.md), so the copy is one to one and
  a `--shot` is the frame exactly as drawn: eight colours, nothing else.
- Sound effects are synthesised once during `audio_init` and cached as PCM,
  replayed through a 16-voice pool. A new effect means: an entry in the
  `SoundEffect` enum in [sound_id.h](../src/sound_id.h) (before `SFX_COUNT`) plus
  a case in `synth_sound` ([audio.c](../src/audio.c)). Audio init failure is
  non-fatal by design — the game runs silently.
- **Music is one score per level theme**, and a score is a table row rather
  than a hand-sequenced routine: a `MusicPlan` in [audio.c](../src/audio.c) names
  a key, a tempo, the 1/16 rhythms of each part and a colour (sweep, clank,
  sparkle, wind, tick, drip), and `synth_music_plan` reads the loop as four
  sections — a statement, a full one, a breakdown that hands the bar to the pad
  and the drone, and a last one that pushes hardest. Only the hand-written
  title theme is built during `audio_init`; a level's loop is built the first
  time it is asked for, and only the title theme, the current track and the one
  before it stay resident (twenty forty-second loops would not). That is why
  the restroom can be scored as its own room — the door switches away and
  straight back without rebuilding the sector's music.
  `level_theme_music` ([level.c](../src/level.c)) owns the theme-to-track
  mapping; because it is one to one, `test_campaign_themes_keep_changing`
  already pins that no two consecutive sectors share a score.
