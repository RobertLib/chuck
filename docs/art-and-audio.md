# Tuning, art, audio

## Tuning, art, audio

- **All tuning constants live in [game_config.h](../src/game_config.h)** — speeds,
  ranges, cooldowns, entity caps, perception angles. Add new magic numbers there
  rather than inline.
- [fx.h](../src/fx.h) is the shared palette and lighting vocabulary for every
  renderer (world, HUD, intro, cutscenes). Use its ramps instead of new literal
  colours so the screens stay one visual system. The accents are semantic and
  rationed — cyan is technology, amber is light and warning, red is danger,
  green is granted, `FX_RUST` is weathering (never danger), `FX_FLAME` /
  `FX_FLAME_HOT` are the one fire, `FX_LAMP`/`FX_WARM`/`FX_SODIUM` are the
  only three light temperatures, `FX_LABEL` is the one grey interface labels
  are set in. A literal that repeats an fx.h value, or lands within a few units
  of one, is that constant misspelt. The heart and the ammo cartridge are drawn
  by `fx_heart`/`fx_ammo_pip` — one glyph across the HUD, the manual and the
  outro, because the player is asked to recognise them everywhere.

  **Where the rule binds is anything the player is asked to read**, and that is
  narrower than this page used to claim. It said a renderer may keep a colour of
  its own "only if it names it once, with a comment saying why the palette
  cannot supply it" — and counted against the tree that is about 950 anonymous
  `(SDL_Color){…}` literals, nearly all of them inside an illustration or a
  material: 461 in [game_render.c](../src/game_render.c), 196 in
  [render_figures.c](../src/render_figures.c), 186 in
  [cutscene.c](../src/cutscene.c), 97 in [manual.c](../src/manual.c). A brick two
  shades off its neighbour is not a constant anybody should have to name, and a
  rule stated more absolutely than it is kept is worse than one stated loosely,
  because the next reader trusts it — which is the same objection this page
  already makes twice, about a guard's cone and about the pad's SELECT.

  So it is two rules. **A semantic colour must be named**: an accent that tells
  the player something — a HUD chip, a strip, a readout, a warning, a state —
  comes out of fx.h, or, where the palette genuinely has nothing that means
  *this*, out of a named `static const` with a comment saying so
  (`COL_CHATTER_IDLE` in [game_render.c](../src/game_render.c) for the one crew
  line where nothing is happening, `COL_KEYCAP` in [manual.c](../src/manual.c) for
  a moulded key against the pad column's `FX_LABEL`). **Material may be
  literal**, and the check on it is `make lint` below: not within two units of a
  palette entry, and every accent above still reserved for what it means.

  **The misspelling half has something behind it.** `make lint`
  ([tools/check_palette.py](../tools/check_palette.py)) reads the palette out of
  fx.h, walks every colour literal in `src/` and `editor/`, and fails the build
  on one within two units of a palette entry — a distance no eye can see, so a
  literal that close is a constant spelled from memory rather than a decision.
  Further out, up to eight, it prints a note and passes: a dark two shades off
  `FX_NIGHT` may genuinely be a plane sitting behind another plane, and
  rewriting it is an art decision a script does not get to make. `make test`
  depends on `lint`, so the two are one gate. Measured the day it was written,
  the rule had drifted to four literals reproducing a palette colour exactly
  and eleven more within two units of one.

  **Each colour is written once and reachable two ways**, which is what closed
  the hole the rule had. `FX_X` is the `SDL_Color` a draw call wants;
  `FX_X_RGBA` is the same four numbers as a bare list, which is what a *static
  initialiser* wants, and C will not accept the first in place of the second —
  a `static const` struct is not a constant expression. That is why the theme
  tables in [level_art.c](../src/level_art.c) spent so long spelling three palette
  colours out in digits: they physically could not name them, so the rule had a
  hole exactly where the game's art direction is decided. Use `FX_X` wherever
  it compiles and reach for `FX_X_RGBA` only inside a static table.
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
- [level_art.c](../src/level_art.c) holds the per-level wall materials and
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
- **One tile in the game has a front and a back, and it is the one something is
  inside.** Every other tile is drawn once, in the structural pass at the top of
  the frame, and that is right because nothing is ever inside masonry — the
  figures go down near the bottom of the frame and never overlap a wall. A duct
  is the exception the whole mechanic rests on: masonry to a man on his feet, a
  gap to a man on his elbows (see [The duct](gameplay.md#the-duct)). So trunking
  is drawn as `draw_vent_plenum` — the unlit shaft — and `draw_vent_grille`, the
  louvres screwed over it, and `render_duct_fronts` lays the grille back over
  whichever of its tiles the player is in after the figure layers are down. Drawn
  in one piece, as it first was, a crawl through a shaft painted Chuck over the
  louvres: the tile whose entire documented cost is that those louvres are opaque
  both ways read as a man crawling along in front of them. Behind them he is what
  he should be — a slot of shirt at a time, and the half of him not in yet still
  out in the room.
  Four details are what make it a picture rather than a clip. The grille goes
  back and the plenum's shading does not: he is lying against the louvres, the
  gradient is the far wall of the shaft behind him, and a second pass of it
  dulled the two pixels of shirt that are the whole of what the player has to
  follow. A tile either side of him is covered as well, because the pose reaches
  past the box it is drawn from — a knife thrust and a launcher muzzle by about a
  dozen pixels — while a muzzle flash's *light* is deliberately left in front of
  the louvres, since light through a grille belongs on the outside of it. His own
  pool of light is laid back on top, because that is drawn with the tiles and
  would otherwise leave a two-tile hole in the one glow that exists to say where
  the hero is. And only the tiles the man is in: the pass runs off his own box
  rather than over every duct on screen, because a blast beside the trunking is
  drawn in front of it and has to stay there.
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
  (`lobby_entrance` in [level_art.c](../src/level_art.c)), on a multiple of the
  layer's period so it lands on the grid the rest of the layer tiles to.
- **What a layer varies per repeat is keyed to the repeat, not to where the
  repeat is on screen.** Which blind is shut, which bank of ceiling lights is
  on, what colour a file spine is, which window in the city is lit: all of it
  comes off the repeat's own world index (`art_repeat` in
  [level_art.c](../src/level_art.c)), because a screen position changes every
  time the camera moves and the thing it describes does not. Keyed the other
  way a backdrop does not scroll, it *boils* — and the failure is invisible to
  every gate here, since a skyline repainting its lights thirty times a second
  executes exactly as much code as one standing still, and a photograph of it
  is a photograph of a skyline. This has now been the same bug three times, in
  three different backdrops, in the same file.
- **A backdrop layer sinks as the climb rises, and never wraps.** The climbs
  are the only place the camera travels on the vertical — a facade map is
  exactly one viewport wide, so `cam_x` is nought out there — and a distant
  tower sits at eye level whatever storey Chuck is on, so what height does to
  the city is put it further down the frame. Two things follow. The offset
  carries `-cam_y` and not `+cam_y`: added, a layer slides *up* the frame while
  the wall drawn in front of it slides down, which is a backdrop moving the
  wrong way past the thing it is behind. And it is not taken modulo anything,
  because a wrap is a snap: the skyline used to jump 110px partway up the taller
  walls and the HIGH climb's cloud deck 60px. `level_backdrop_sink` is the one
  answer to both, and it is in [level.c](../src/level.c) rather than in the
  renderer so that the suite can ask it the two questions worth asking — it
  only ever runs one way, and it never jumps. A star field is the one layer
  that may wrap, and does: its period is wider than the frame, so a star turns
  over off screen.
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
  as a wall.** Every body block in [render_figures.c](../src/render_figures.c) goes
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
  rear-facing climbing pose in [render_figures.c](../src/render_figures.c) spends its
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
  [intro.c](../src/intro.c) is the first thing anyone sees, and it is built as one
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
