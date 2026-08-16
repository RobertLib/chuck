# Level character legend

This file describes the meaning of characters used in the level text files.

`make editor` builds a tool that paints with every one of them, draws the map
the way the game draws it, and checks the rules on this page while the map is
being edited. Its palette is the same list in code
([editor/editor_legend.c](../editor/editor_legend.c)); a character added here
has to be added there as well, or it is a character the editor cannot paint.

- `#` : Wall (solid, impassable tile). In a `MODE FACADE` level it is exterior
  masonry: a stone cornice or plant the climber must route around, and cover
  that shatters thrown objects and turns birds away.
- `%` : Weak wall — a blocked-up opening, drawn as coarse blockwork let into the
  sector's own material and cracked. It is solid in every way a `#` is until an
  explosion takes it out: bullets stop on it, guards cannot see through it, and
  props will not stand on it. Any blast within reach opens it (grenade, rocket,
  mine, gas canister) and the hole is permanent for the rest of the run, so it
  survives a lost life and is back the next time the sector loads. Interiors
  only, and never the way out — see the authoring rule below.
- `H` : Ladder (can climb up/down).
- (space) : Empty space / air.
- `.` : Empty padding / air (useful before a compact sublevel room).
- `C` : Card item (`ITEM_CARD`). The seed picks one of a sector's cards as the
  live one and the rest buzz and change nothing, so **a sector with a single
  `C` has no decoys at all** — the wrong-card sound, the sweep the sector opens
  on and the manual's "cards lie" are all spent on a card that cannot be wrong.
  Two or three is the shape that means anything; sector 1 keeps one on purpose,
  because it is the sector that teaches what a card is for. Every card has to
  be reachable, live or not, and the editor calls an unreachable one an error
  rather than a note for exactly that reason.
- `G` : Gun item (`ITEM_GUN`). Fills the sidearm, and **the only pickup that
  comes back** — `ITEM_RESPAWN_TIME` after it is taken it is there again,
  because the sidearm is what a sector is played with and a player who has
  spent it must not be left playing the rest of the floor with a knife.
- `N` : Grenade item (`ITEM_GRENADE`). One grenade, and it does not respawn:
  a one-shot explosive that regrows is not a decision about when to spend it,
  and a single `N` on a respawn timer opened every `%` in the campaign without
  the bazooka the patches were placed for.
- `K` : Medkit item (`ITEM_MEDKIT`). Refills the hearts, or adds a life if they
  are already full. Does not respawn.
- `Z` : Bazooka item (`ITEM_BAZOOKA`). Contains one explosive rocket and does not respawn.
- `M` : Enemy spawn (enemy is placed here).
- `W` : Enemy spawn with a guard dog.
- `J` : Ambient janitor with a cleaning cart and mop (visual-only NPC).
- `f` : Fleeing civilian (visual-only NPC). The level starts with him frozen
  facing the way the player came in; after a staggered beat he shouts and runs
  for it, dissolving as he reaches it, and the part is over. He falls off
  ledges rather than turning at them, so a route down a stair or off a
  mezzanine is walked without help; see the authoring rule below.
- `k` : Receptionist working the front desk (visual-only NPC). The tile is a
  post, not a patrol: they stand on it facing whichever side has room to stand
  in, leave every ten seconds or so on an errand two to five tiles out, deal
  with whatever it was, and walk back to the same tile. Place it on the staff
  side of an `n` run — the counter renders over the staff, which is what makes
  the post read as being behind it.
- `X` : Mine (places an explosive mine). Only the player's weight arms it, but
  the blast that follows a beat later is an ordinary blast: it is lethal to a
  guard or a dog caught in it, breaks crates and opens a `%` patch. Counted in
  the hazard budget below as a threat to the player, because that is who
  triggers it.
- `^` : Spike / hazard. Costs one heart on contact and pops the boots back out
  of the bed, so one misstep is one heart rather than a loop.
- `O` : Rotating ceiling fan (one heart on contact with the blades). It is drawn
  hanging on a drop rod from the first solid tile above it, so it never floats
  however far below the ceiling it is placed.
- `B` : Pushable crate (can be shoved or destroyed by shots/explosions).
- `L` : Small gas canister. A standing shot passes over it; crawl and shoot it
  to cause an explosion that can kill nearby enemies. Any other blast in range
  sets it off too — a grenade, a rocket, a mine or another canister — so a pair
  placed within `GAS_CANISTER_RADIUS` of each other will chain.
- `T` : Access terminal. One randomly selected terminal is active; the rest are decorative.
- `A` : Wall-mounted alarm switch. A guard may run to it after spotting the player.
- `c` : Decorative office chair (non-solid).
- `d` : Decorative office desk with a computer (non-solid).
- `i` : Decorative office equipment; its visual variant is selected from a filing cabinet, printer, or server rack (non-solid).
- `n` : Decorative reception counter in stone and brass; one tile of a run also
  carries the visitor terminal (non-solid).
- `s` : Decorative waiting-area bench seat (non-solid).
- `t` : Decorative palm in a stone planter (non-solid).
- `g` : Decorative optical security gate; the fitting that explains why the
  stair door upstairs wants a card (non-solid).
- `m` : Decorative flight case, stencilled *Meridian Facility Services* — one of
  the ones the crew wheeled in through the goods entrance in March and nobody
  inspected (non-solid). Half of them stand shut and half lie open with a
  rifle-shaped hole in the foam, chosen from the tile position, so a run of
  them is not a run of the same box. Interiors only, and worth putting near an
  explosive: the case the bazooka came out of, two tiles from the bazooka, is
  the whole plot said without a line of text.
- `w` : Decorative wall clock (non-solid). **The one prop that hangs rather
  than stands**: it needs a solid tile directly *above* it, and the loader
  drops it if there is none, exactly as it drops a desk with no floor. The
  dial reads the campaign sector it is standing in — the night runs from the
  broadcast to the bonds leaving the roof at 01:00, so the minute hand climbs
  toward the top of the face across the fifteen sectors. Presentation only:
  nothing in the simulation reads the time.
- `S` : Player start position.
- `E` : Normal security-door exit. When the map also contains `Y`, this door
  is physically blocked and cannot be unlocked by a card or terminal.
- `Y` : Open traversable window. In an interior it is the alternative route
  out; in a `MODE FACADE` level it is the window back into the building.
- `D` : Door tile (`TILE_DOOR`).
- `U` : Entrance to a separate sublevel (currently the WC/restroom).
- `R` : Return door from a sublevel to its paused parent level.
- `q` : Decorative restroom toilet (non-solid).
- `b` : Decorative restroom washbasin (non-solid). A mirror is drawn in the
  tile above it, and reflects Chuck when he stands in front of it.
- `u` : Decorative restroom urinal (non-solid).
- `p` : Decorative restroom stall partition (non-solid).
- `o` : Open restroom stall with a visible toilet (non-solid).
- `z` : Closed restroom stall door (non-solid).
- `V` : Elevator shaft (vertical elevator track).
- `F` : Falling platform (falls after triggered).
- `P` : Moving platform (moves horizontally).
- `r` : Facade-mode window that periodically throws an object toward the player.
- `v` : Facade-mode bird entry point. Birds periodically cross toward the player.

Notes:

- Decorations are loaded only when placed directly above a static `#` wall tile;
  unsupported ones are ignored so they cannot float in air. The one exception is
  the wall clock `w`, which hangs and therefore asks the tile *above* it the
  same question; the editor reports either mistake.
- The office set (`c` `d` `i`) and the front-of-house set (`n` `s` `t` `g`) are
  not interchangeable dressing. A public floor furnished out of the office set
  reads as an office floor whatever its walls are made of, so keep desks and
  server racks to the staff side of a lobby and counters, seating and planting
  to the visitor side.
- Fleeing civilians (`f`) run for the player's own start tile — in the lobby
  that is the street entrance he came in through — and dissolve about four
  tiles short of it, so plant them further into the room than that or they fade
  before they have run anywhere; `test_all_embedded_levels_parse` pins it. They
  walk and fall only: a stair, a step or a drop off a mezzanine is route enough,
  a ladder or a door is not. One walled in by the geometry gives up and fades
  out rather than jogging against the wall for the rest of the level, but that
  is a safety net, not a placement.
- A receptionist (`k`) needs floor either side of the counter to do anything
  with: an errand walks out along the same storey and stops at the first ledge
  or wall, so a post with two tiles of room in neither direction just stands
  there. Being boxed in is not a fault, only a waste of the part.
- Hold `E` near the visibly active terminal to hack it and unlock the exit.
- Alarm switches are operated by guards. An active alarm alerts every guard
  and dog, then shuts itself off after nobody has seen the player for a short time.
- A `SPAWNS n0 n1 ...` line may appear after the grid. When present, it must
  contain exactly one spawn count for every door, in the order the doors
  appear in the file. `F9` in the editor rewrites the line for the doors the
  map currently has.
- `MODE FACADE` on a metadata line after the grid selects the separate exterior
  climbing mode. It uses direct four-way wall movement: no gravity, ladders,
  doors, guards, or ordinary platform simulation. Pickups do work, so items
  placed on a facade are optional detours that carry into the next sector.
  Nothing can be *fired* out there — `update_facade_playing` never runs the
  attack — so a climb's pickups are entirely a bet on the sector above it, and
  every climb carries the same three: a `G`, an `N` and a `K`. Sector 7 spent a
  while without the `G`, which made the one climb that hands straight into the
  LAB the one you arrived at with whatever ammunition was left.
- `THEME <name>` on a metadata line picks the level's art direction — see
  [Themes](#themes) below.
- Facade masonry authoring: the climber box is exactly one tile tall, so a
  single `#` inside a two-row band seals that band off horizontally. Keep
  cornices to full rows with gaps of three tiles or more, and leave lone
  blocks out; plant is painted on top of the cornices instead.
- An interior campaign level contains one `E` and may additionally contain one
  `Y`. A facade level contains one `Y` and no `E`; a sublevel contains one `R`.
  When a map has both, the `Y` is the route and the `E` is dead scenery: the
  exit stays locked whatever the player does with cards or terminals.

## Themes

`THEME <name>` after the grid selects the wall material, backdrop, palette and
score the level gets (see [level_art.c](../src/level_art.c)). It is
presentation only: no theme changes collision, spawning, lighting reach or any
other simulated behaviour, so the same map plays identically under any of them.
A map with no `THEME` line keeps the default for its mode — `PLANT` inside,
`FACADE_NIGHT` on a wall — so a new sector drops in without one. A misspelt
name is a parse error rather than a silent fall back.

The Floor column is the finish on the walkable top of a slab: a sliver a few
pixels deep, and the one surface the player looks at for the whole level. The
bright arris line above it is the theme's trim in every sector, because where
Chuck can stand has to read identically throughout; the finish under it is what
says which floor of the building he is standing on.

| Name | Walls | Floor | Interior | Score |
| --- | --- | --- | --- | --- |
| `PLANT` | riveted steel plate | screed | machine hall, the original look | 104 bpm A minor; saw bass on the beats, half-time snare, struck metal |
| `LOBBY` | polished marble, brass reveals | polished stone | glazed street front with the main entrance off the street, reception | 88 bpm A minor; no snare at all, a pad holding the room, brass glints |
| `OFFICE` | painted partition board | carpet tile | cubicle farm, blinds, ceiling grid | 96 bpm E minor; the workaday groove the campaign shipped with |
| `SERVER` | dark plate | raised access panel | rack rows and status LEDs | 126 bpm B minor; two-bar chords, machine pulse, sixteenths where a tune would be |
| `CANTEEN` | glazed tile | treadplate | servery counter under heat lamps | 108 bpm G major; the only shuffle, and the only major key indoors |
| `LAB` | pale tile | ceramic | clean-room bays, fume cabinets | 92 bpm C minor; a chromatic wobble that never settles, tritone lead |
| `ARCHIVE` | brick | timber boards | shelving stacks under bare bulbs | 76 bpm D minor; one soft pulse a bar is the whole rhythm section |
| `SECURITY` | dark plate | carpet tile | monitor wall and console desks | 128 bpm F minor; a march — four on the floor and a snare into the bar line |
| `DUCTS` | dark plate | treadplate | galvanised trunking; the darkest sector | 84 bpm B♭ minor; a fan pedal that never stops and air past the plating |
| `PENTHOUSE` | hardwood panelling | parquet | panelled hall, sconces, cabinets | 100 bpm B♭ minor; the widest chords in the building, and punched |
| `ROOF` | raw concrete | screed | curtain wall over the night city | 96 bpm F♯ minor; the lead finally carries the loop, over open wind |
| `RESTROOM` | tile | ceramic | implied by restroom fittings; sublevel only | 72 bpm E♭ minor; tiled and dripping, no kit — the sector's score waits outside |
| `FACADE_NIGHT` | — | — | clear night, city lights below | 90 bpm E minor; wide, slow and exposed, the city glittering under it |
| `FACADE_STORM` | — | — | rain, lightning, wet stone | 116 bpm C minor; the only climb in a hurry, rain on the cornices |
| `FACADE_DAWN` | — | — | sunrise, warm stone, distant birds | 82 bpm A major; the one climb with the light coming |
| `FACADE_HIGH` | — | — | above the cloud layer, neon signage | 70 bpm B minor; thin air — barely a bass note, a long way between phrases |

Two rules the campaign keeps, both pinned by
`test_campaign_themes_keep_changing`: no two consecutive levels share a theme,
and a facade level uses a `FACADE_*` theme while an interior never does. The
second rule is what keeps the scores changing too, since the theme-to-track
mapping is one to one.

## Sector plans

No two sectors are laid out the same way. A stack of full-width corridor bands
joined by ladder columns is the one plan the campaign does not use twice: it is
what every sector used to be, and fifteen of them played as one level repeated.
Each sector now has a floor plan that belongs to its theme, and
`test_campaign_levels_are_distinct_and_solvable` pins that no two share a size
or a storey rhythm.

| # | Theme | Plan |
| --- | --- | --- |
| 1 | `LOBBY` | the glazed entrance hall: a grand stair out of the double-height atrium to a gallery, a service ladder up to the security wing where the exit is, and a second service ladder down behind the reception line to the staff corridor with the restroom. The hall empties past Chuck as he walks in — see `f` — and the front desk stays staffed after it has, see `k` |
| 2 | `OFFICE` | three open-plan floors cut into blocks by floor-to-ceiling partitions, the ladders staggered so every partition is passed by changing floor; a welded stair core at the far end, the executive gallery back across the top, and a service crawl underneath reached by one ladder, where the bazooka lies beside a blocked-up opening (`%`) that saves the teleport across |
| 3 | `FACADE_NIGHT` | one wide breach per course, walking slowly from side to side |
| 4 | `SERVER` | a serpentine of four aisles walked in alternating directions, plus two fenced pockets — the vault above, the terminal room below — joined by a service ladder, entered by an airlock door pair or by dropping through the cable gap in the hot-aisle floor |
| 5 | `PLANT` | catwalk towers either side of a solid plant block, goods lift onto its roof, crane platform across the hall to the exit ledge; spiked pit under the crane gap |
| 6 | `CANTEEN` | a double-height dining hall against a tight galley stack; the way up is the kitchen and its dumbwaiter, and a blast through the galley wall is the way back down |
| 7 | `FACADE_STORM` | short balconies instead of courses, so cover for the wind is sparse |
| 8 | `LAB` | a spine corridor with sealed clean-room bays combed off it above — each an airlock reached by its own ladder — and basement chambers and a deep-storage crawl below; the rocket vault opens only through a paired door |
| 9 | `ARCHIVE` | a grid of canyons between blocks of shelving, linked only where an aisle was cut through; the route weaves up, across the reading room, and back down before the exit stair |
| 10 | `SECURITY` | a patrol ring around the control bunker; both corridors are bricked up, so the bunker's lower floor is the only way across, and the airlock doors flanking it are where the reinforcements come out |
| 11 | `FACADE_DAWN` | two breaches that braid, swapping sides as the climb rises and merging where the lines cross |
| 12 | `DUCTS` | six crawl levels chopped into runs, one riser each; climb, drop through a missing panel, climb again. The rocket pocket hides behind a blocked-up bulkhead |
| 13 | `FACADE_HIGH` | offset stubs laid like brickwork: every band is a lateral detour |
| 14 | `PENTHOUSE` | the only symmetrical plan: panelled rooms with single doorways around a double-height reception hall crossed by balcony stubs. The far bay keeps the medkit and the rocket behind a paired door whose other end is the one guards come out of; the `%` in the bay's end wall is a second way in, for anyone who can spare a blast |
| 15 | `ROOF` | a skyline, not a floor plan: plant rooms of five heights under open sky, a gondola strung between the two towers, and a fan-choked service level beneath the deck that is the only way past the two breaches in it |

Two campaign-wide rules go with it, both pinned by the same test:

- **Pressure only rises.** A sector's hazard budget —
  `3·guards + 2·dogs + 2·mines + spikes + fans` — must exceed the previous
  interior's; a climb's budget (`3·throwers + 2·birds`) and its height must
  exceed the previous climb's. The campaign runs 6, 14, 20, 24, 29, 37, 48, 53,
  55, 64, 67 inside and 20, 25, 30, 35 on the walls. Sector 1 spends its whole
  budget on two guards and carries no hazard at all: it is where the player
  finds out what the controls do — including, since it gained a medkit on the
  lobby stair, what a pickup looks like before anything is shooting.
- **Every interior is finishable.** The test walks a conservative model of the
  player — walk, fall, step up one tile, jump a one-tile hole (two with a
  second open row overhead), hop one spike with that clearance, ladders, lift
  shafts, moving platforms, paired doors — and requires that it reaches the way
  out, every key card, every terminal and the restroom door, and that no tile
  it can reach strands it away from the exit. **The restroom behind that door
  is held to the same model**, on both its pickups and the way back out: it is
  not a campaign sector, so it was for a while the one room the model never
  ran, and its medkit sat across a two-tile gap under a two-row ceiling — the
  exact jump the reach rule below says is not on.

Three things that model deliberately will not do, so do not require them:

- **Stand on a falling panel.** `F` is never counted as floor, so the sector
  has to work once every panel has gone.
- **Stand on a door that hangs over a ladder.** A `D` needs a wall under it;
  put doors on the floor row, not on top of a riser.
- **Land two tiles up, or cross two spikes.** Anything the rules below say is
  out of reach really is out of reach.

## Authoring rules for interiors

A storey is a solid `#` slab with an open band above it. Entities stand in the
bottom row of that band, so a two-row band is a corridor and a three-row band is
a hall. These rules come from the tuning in
[game_config.h](../src/game_config.h) and every campaign map obeys them.

- **Jump reach.** A 365px/s jump under 980px/s² gravity peaks at 68px. In a
  two-row band the ceiling caps it at one tile and the player only covers about
  48px of ground — enough to clear a one-tile hole in the floor, not a two-tile
  one. Give him a second open row (~87px) before asking for a two-tile jump;
  anything wider needs a ladder, a lift shaft or a moving platform.
- **Spikes are area denial, not an obstacle course.** Clearing a single 32px
  spike means covering 58px of ground while the whole 26px-wide player box is
  above floor level, and even an unobstructed jump only offers about 73px of
  that. Two spikes side by side cannot be jumped at all. Use `^` to split a
  floor into halves that are each reached some other way.
- **Ceiling fans** hit a 46px-wide, 8px-tall band across the middle of their
  tile. Placed in an air row they only catch a jumping player, which is the
  intent — but the blades overhang the neighbouring columns, so keep `O` at
  least two columns clear of any `H` or `V` a climber passes through, and never
  put one directly above a `B` the player can stand on. A fan belongs in the
  top row of its band, hard under the slab: the drop rod is then a rod rather
  than a mast, and the band's own ceiling is what the fan hangs from. One in
  the standing row catches anyone who walks into it, crawling included.
- **Never hang a fan over a gap that has to be jumped.** The blades take a
  heart off every attempt while the route model, which knows nothing about
  fans, still reports the gap as crossed — three tries and the life is gone,
  which is the one way to build a sector the test calls finishable and the
  player cannot finish. Keep `O` at least two columns clear of any hole in the
  floor below it.
- **Ladders need not run the full height.** A run from the destination floor's
  headroom down to the source floor's standing row can be mounted and left at
  both ends; staggering short runs is what turns a floor plan into a route.
  The campaign's usual shape is shorter than that: the top tile of the run is
  the hole cut in the slab, and the storey above is left by climbing the last
  tile clear of it. Both the player and a guard finish that climb standing on
  the slab, but only because the guard is told to — see the ladder top in
  [enemy.c](../src/enemy.c), which is where that geometry was once a trap.
- **`F` panels are a shortcut, never a lifeline.** They fall away for the rest
  of the run, so the level must still be finishable once every one of them has
  gone, and no ledge they serve may become a place the player cannot leave.
  The parser makes the *map* tile air and hangs the panel off the runtime, so
  the route model already judges the sector in its fallen state — which makes
  **laying `F` into a hole a sector already has the safest placement there
  is**: the model's answer cannot change, and the player gets one crossing of a
  gap that was never crossable. That is where the campaign's panels are, in
  sectors 2, 4, 9, 12 and 15.
- **`%` patches are the same bargain from the other side.** The route model
  counts one as wall, so a sector is judged in the state it is authored in and a
  blocked-up opening can never be the way out, a key card's only approach or a
  terminal's. It also counts as floor, so a patch may be set into a slab without
  cutting the storey in two. Two things follow: put a `%` where opening it saves
  a detour or opens a cache, and give the sector a grenade `N` or a bazooka `Z`,
  because nothing else in the game opens one. Keep it one tile thick in the
  direction being crossed — a blast opens the face it goes off against, not a
  tunnel — and leave the row above a floor-level patch solid so the hole reads
  as a doorway.
- **`P` platforms** patrol the contiguous non-solid run of their row, bounded by
  `#`, `D` and `V`. Keep ladders and other gaps out of that row or the platform
  will wander further than the void it is meant to bridge.

## Authoring rules for facades

- The climb is inset by `FACADE_BUILDING_SIDE_INSET` (80px) on both sides, so
  the outer two and a half columns are out of reach — pad them and start the
  masonry at column 2.
- Closed windows are painted on an architectural grid: rows that are multiples
  of three, columns that are multiples of four. Put `S`, `Y`, `r` and `v` on
  that grid so each one replaces a painted window instead of covering one.
- Cornices belong on the rows between those window rows, so a full-width run
  leaves two open rows above it. Leave the cornice directly above `S` open at
  the start column, or the climb is sealed in before it begins.
- A row of short balconies instead of a full cornice makes a good breather: it
  still gives cover and a wind shelter without a lateral detour.
