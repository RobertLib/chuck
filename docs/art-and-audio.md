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
- **An alpha is only an alpha with blending on.** `color_rect` and `fx_rect`
  draw with the renderer's blend mode off, so an `SDL_Color` whose fourth
  number is under 255 is not a translucent rectangle — it is an opaque one that
  also writes a hole into the frame's alpha. That was the "shadow" under every
  piece of furniture (a pale grey plinth), the lobby turnstile's glass (a lit
  slab), the restroom's rim and a see-through strip down the side of every
  `--shot` of the arrival. `fx_rect_a` is the translucent rectangle; reach for
  it, or for `fx_contact_shadow`, whenever the colour you mean has air in it.
- **The finish opens with the glow.** Every light in the game used to stop at
  its own last pixel, so a lamp strip and the brick beside it were the same kind
  of object with different numbers in them. The frame is now drawn into a
  texture (`PlatformState.frame`), and before the vignette `glow_apply` in
  [game_render.c](../src/game_render.c) copies it down to half size, subtracts
  `GLOW_THRESHOLD` from every channel so only the top of the range survives —
  in its own colour, so a red lamp leaves red and cream type a pale haze — blurs
  that through a quarter, an eighth and a sixteenth, and adds the three back at
  `GLOW_MIX`. So a light no longer needs its own `fx_glow` to read as emitting;
  it only needs to be bright, and the ones that get a halo are the ones that
  are. The threshold sits above every *material* in the palette on purpose —
  pale paint, lit ledges and the cream type were measured glowing at a lower
  one and it read as fog — which is why brightness is now a statement a
  renderer makes on purpose. It is behind the CRT filter switch with the rest
  of the finish. The subtraction is built from additive blending alone —
  invert, add the threshold, invert again, and let the eight-bit target's clamp
  do the `max` — because desktop OpenGL has no subtracting blend and the first
  version switched itself off there; on a renderer with no custom blend at all
  (the software one) the game draws straight into the window as it always did.
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
- **A beam is volume, not a wedge.** A cone is a gradient and a gradient is a
  shape: nothing in it says it is light passing through air rather than a
  translucent triangle painted on the wall. `draw_beam_dust` in
  [game_render.c](../src/game_render.c) turns a handful of motes over inside each
  fixture's cone — brightest near the lamp and in the middle of the beam, gone at
  its edges, faded in and out at the ends of their fall so the wrap never pops —
  and stops them where the beam meets the floor, so no mote is ever drawn over
  the slab. They are keyed to the fixture's own tile hash, sink on the render
  clock (a pause sheet leaves the air behind it alive), and hold still under
  reduced motion: the beam keeps its volume and loses only the drift.
- **An interior's back wall belongs to a room, not to the screen.** Every
  interior backdrop used to be one picture the size of the frame, pinned to the
  frame: it slid sideways with the camera and not at all up or down, so climbing
  a ladder moved the building past a wall that stood still, and every storey
  showed whatever slice of that one picture was behind it — a rack, a stack of
  shelving or a window bay cut in half by the slab above it and carrying on in
  the storey above. `level_backdrop_plan` in [level.c](../src/level.c) reads the
  map into rooms (a ladder hole or a pair of falling panels belongs to the slab,
  a door does not cut a strip out of the wall it is in, and a platform standing
  in a hall is in front of the hall's wall), and `level_art_backdrop` draws each
  room's wall clipped to it and anchored to the building, so it moves with the
  slabs on both axes. A theme lays its wall out against `ArtRoom` — things stand
  on its floor, hang from its ceiling and are sized to its height, which runs
  from two tiles to a dozen — while what is far away, a city through glazing or
  the sky over the roof, stays near the screen inside its window and so moves
  much less than the frame it is seen through. The dust, the room's own air
  gradient and the haze on its floor are drawn per room around the theme, which
  is why every storey now has haze on its floor rather than only the bottom of
  the frame. `test_the_back_wall_is_laid_out_per_room` and
  `test_every_shipped_interior_reads_into_rooms` hold the reading.
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
- **A backdrop layer sinks as the climb rises, and never wraps.** This bullet
  used to open by calling the climbs the only place the camera travels on the
  vertical, which was never true — most interiors are taller than the frame —
  and is the sentence the screen-pinned interior backdrops were resting on. On
  a climb the camera travels *only* on the vertical — a facade map is
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
- **And a cycle driven by a clock skates anyway.** That bullet was true and was
  not enough, which is what Chuck's run showed for as long as he had one: the
  cycle tracked the ankle back under him at a constant rate, and the rate was
  three and a half pixels either side of the hip while the man travelled
  twenty-two between footfalls. A foot moving back a sixth as fast as the body
  moves forward over it is a foot sliding, and the corridor between sectors —
  where the film eases him across the screen on a smoothstep at every speed from
  nought to two hundred and sixty pixels a second — was that slide at one and a
  half times the size. So his gait lives in [chuck_pose.c](../src/chuck_pose.c)
  and is driven by **distance**: `chuck_gait_cycle` turns how far he has
  travelled into a place in the stride, and `chuck_gait_stride` is not a tuning
  number but the length the planted foot sweeps divided by the share of the
  cycle it is planted for, which is the one stride at which the foot stays where
  it was put. The suite holds that as a property
  (`test_a_planted_foot_stays_where_it_was_put`), along with the bones keeping
  their length in every pose, no joint jumping anywhere in a cycle, the arms
  swinging against the legs, and the difference between a walk and a run: a walk
  vaults over a stiff leg and is highest above it, a run lands on a bending one,
  is lowest there, and has a flight between footfalls.
- **The film runs him plainly.** The sector's run is a run a player steers, and
  it acts: the heel kicks up behind him, he leans into it, the elbows drive and
  the headband streams. In the film that was the one figure on the pavement
  doing any of it — the captors walk on a flat two-beat step with their bodies
  still, Ellen barely lifts her feet — and he read as drawn for another film.
  `CHUCK_GAIT_PLAIN_RUN` is the run with the acting taken out: the run's flight
  and a stride at least as long, so the planted foot still stays put at every
  speed the film eases him through, and everything a viewer sees — how high a
  foot comes up, the lean, how far the hands travel and the elbows fold, the
  bounce, the headband — held under his own walk, on soles that never tip
  because the cast's shoe is a block that does not.
  `test_the_films_run_is_the_run_with_the_acting_taken_out` holds both halves
  against the other two gaits rather than against numbers. The film's standing
  and armed poses were already still, and are unchanged.
  **The kerb is the exception, and it is one on purpose.** Plain is right for
  a man walking into a building; it was wrong for a man watching his wife
  taken, where it read as not caring. There the same legs are pitched forward
  with the fists carried and driving (`agent_hurry` in
  [cutscene.c](../src/cutscene.c)) — the spine and the arms only, so the
  planted foot is still the plain run's — and the face shows what he saw. See
  [The prologue](screens.md#the-prologue-three-beats-one-shot) for the beat.
- **Chuck is a skeleton, and one drawing of it.** The sector and the film used
  to draw him twice, by hand, with every knee placed rather than solved, so a
  shin could be any length a pose wanted. He is
  joints now ([chuck_pose.h](../src/chuck_pose.h)) — knees and elbows solved by
  a two-bone reach, so no pose lengthens a limb, and a hand sent further than the
  arm goes stops at the end of it — and one renderer
  ([render_chuck.c](../src/render_chuck.c)) draws them at one pixel to the unit
  for the sector and 1.4 for the film.
  **The film's man has shorter legs, on purpose.** Drawn at 1.48 on the
  sector's legs he stood half a head over the crew he was chasing, on legs a
  fifth longer than theirs under a jacket two rows shorter, and read as a
  different build. `chuck_pose_fit_legs` scales every leg of a pose about its
  hip and brings the pelvis down so a planted foot stays planted; the film fits
  him at 0.82 and draws him at 1.4, a hair over the crew's 1.32 to 1.35. The
  sector keeps its legs, because a jump and a stride are measured on them.
  **What the skeleton wears is the cast's, and that was learned the hard way.**
  The first drawing of him from it shaped the parts as profiles — a smaller head
  with a chin, a jacket with a chest and a waist, a lot more leg — and it was a
  better drawing of a man and the wrong drawing of this one: every guard,
  civilian and captor in the game is chamfered blocks, and next to them he read
  as a figure pasted in from another game. So the head is the eight-by-seven
  block every head in the building is and the limbs are three pixels of garment
  as theirs are. What he keeps from the first drawing is a little more leg —
  the hip joint at 20.7 rather than 21.5 — so the stride has room to be a
  stride. **The proportions a player reads first are the cast's, not the
  figure's**: change one figure's and it is the one they notice.
  **And a chamfered block is not automatically a torso.** Going back to the
  cast's thirteen-by-twelve with two rows off the top corners put him straight
  back into the shape he was rebuilt to leave: shoulders sloping away from the
  neck, straight sides, and the widest, brightest row — the belt — at the foot,
  over legs narrower than it. That is a bell, and with hands that hung exactly
  at the hem the silhouette flared at the bottom. The jacket is its own row
  table now (`JACKET_BACK_IN` / `JACKET_FRONT_IN` in
  [render_chuck.c](../src/render_chuck.c)): eleven wide, square at the shoulders
  with only the corners off, straight down, and a unit in at the belt and the
  hem; and the arms are long enough that the hands hang beside the thigh rather
  than at the hem. The three views that are not side on — the ladder
  and the console from behind, and the crawl — are drawn by hand in
  [render_figures.c](../src/render_figures.c) to the same proportions, and the
  crawl borrows the skeleton's head, so his face does not change when he gets
  down.
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
- **The roof the night ends on is built the way the title screen is.** The outro
  was the last screen in the game still drawn as flat rectangles: one row of
  towers two or three units off the sky, so its lit windows floated like dirt,
  and a helipad drawn as a face-on circle whose top arc hung in the air above the
  parapet. `render_outro_skyline` in [cutscene.c](../src/cutscene.c) is the title
  screen's recipe — the city's teal haze behind a far row, a near row a value
  darker with the moon on one flank and the roofline, floors laid in as faint
  spandrels so a window sits in a building, setback crowns, water tanks and
  aviation lights — and the deck under the figures is a wet membrane in
  perspective: lapped seams closing up with distance, the moon broken into
  ripples down it, puddles that reflect the haze on their far edge, and the
  helipad painted flat and foreshortened with everything else.
- **Every roof in the drive is lit by one moon.** The blocks either side of the
  road are the only surfaces in the game seen from straight above, and they
  were flat slabs. `draw_rooftop_block` in
  [chase_render.c](../src/chase_render.c) gives each a parapet lit along its lip
  and shading the membrane inside it, the membrane rolled in strips with ballast
  and drains, and hardware in a grid of bays — condensers, vent stacks, a wooden
  tank, skylights, a stair head with a lamp on its lee side, a dish — every piece
  throwing its shadow down and to the right, the side away from the moon the
  title screen and the outro hang up and to the left. The only other light is
  the street's: the lip over the road takes the sodium, and a sign board stands
  on it facing the traffic. All of it comes off the block's world seed, never off
  where the block is on screen.
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
