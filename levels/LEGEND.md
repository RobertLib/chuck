# Level character legend

This file describes the meaning of characters used in the level text files.

- `#` : Wall (solid, impassable tile). In a `MODE FACADE` level it is exterior
  masonry: a stone cornice or plant the climber must route around, and cover
  that shatters thrown objects and turns birds away.
- `H` : Ladder (can climb up/down).
- (space) : Empty space / air.
- `.` : Empty padding / air (useful before a compact sublevel room).
- `C` : Card item (`ITEM_CARD`).
- `G` : Gun item (`ITEM_GUN`).
- `N` : Grenade item (`ITEM_GRENADE`).
- `K` : Medkit item (`ITEM_MEDKIT`).
- `Z` : Bazooka item (`ITEM_BAZOOKA`). Contains one explosive rocket and does not respawn.
- `M` : Enemy spawn (enemy is placed here).
- `W` : Enemy spawn with a guard dog.
- `J` : Ambient janitor with a cleaning cart and mop (visual-only NPC).
- `X` : Mine (places an explosive mine).
- `^` : Spike / hazard (instant damage when stepped on).
- `O` : Rotating ceiling fan (lethal on contact with the blades). It is drawn
  hanging on a drop rod from the first solid tile above it, so it never floats
  however far below the ceiling it is placed.
- `B` : Pushable crate (can be shoved or destroyed by shots/explosions).
- `L` : Small gas canister. A standing shot passes over it; crawl and shoot it to cause an explosion that can kill nearby enemies.
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
  unsupported ones are ignored so they cannot float in air.
- The office set (`c` `d` `i`) and the front-of-house set (`n` `s` `t` `g`) are
  not interchangeable dressing. A public floor furnished out of the office set
  reads as an office floor whatever its walls are made of, so keep desks and
  server racks to the staff side of a lobby and counters, seating and planting
  to the visitor side.
- Hold `E` near the visibly active terminal to hack it and unlock the exit.
- Alarm switches are operated by guards. An active alarm alerts every guard
  and dog, then shuts itself off after nobody has seen the player for a short time.
- A `SPAWNS n0 n1 ...` line may appear after the grid. When present, it must
  contain exactly one spawn count for every door, in the order the doors
  appear in the file.
- `MODE FACADE` on a metadata line after the grid selects the separate exterior
  climbing mode. It uses direct four-way wall movement: no gravity, ladders,
  doors, guards, or ordinary platform simulation. Pickups do work, so items
  placed on a facade are optional detours that carry into the next sector.
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

`THEME <name>` after the grid selects the wall material, backdrop and palette
the level is drawn with (see [level_art.c](../src/level_art.c)). It is
presentation only: no theme changes collision, spawning, lighting reach or any
other simulated behaviour, so the same map plays identically under any of them.
A map with no `THEME` line keeps the default for its mode — `PLANT` inside,
`FACADE_NIGHT` on a wall — so a new sector drops in without one. A misspelt
name is a parse error rather than a silent fall back.

| Name | Walls | Interior |
| --- | --- | --- |
| `PLANT` | riveted steel plate | machine hall, the original look |
| `LOBBY` | polished marble, brass reveals | glazed street front with the main entrance off the street, reception |
| `OFFICE` | painted partition board | cubicle farm, blinds, ceiling grid |
| `SERVER` | dark plate | rack rows and status LEDs |
| `CANTEEN` | glazed tile | servery counter under heat lamps |
| `LAB` | pale tile | clean-room bays, fume cabinets |
| `ARCHIVE` | brick | shelving stacks under bare bulbs |
| `SECURITY` | dark plate | monitor wall and console desks |
| `DUCTS` | dark plate | galvanised trunking; the darkest sector |
| `PENTHOUSE` | hardwood panelling | panelled hall, sconces, cabinets |
| `ROOF` | raw concrete | curtain wall over the night city |
| `RESTROOM` | tile | implied by restroom fittings; sublevel only |
| `FACADE_NIGHT` | — | clear night, city lights below |
| `FACADE_STORM` | — | rain, lightning, wet stone |
| `FACADE_DAWN` | — | sunrise, warm stone, distant birds |
| `FACADE_HIGH` | — | above the cloud layer, neon signage |

Two rules the campaign keeps, both pinned by
`test_campaign_themes_keep_changing`: no two consecutive levels share a theme,
and a facade level uses a `FACADE_*` theme while an interior never does.

## Sector plans

No two sectors are laid out the same way. A stack of full-width corridor bands
joined by ladder columns is the one plan the campaign does not use twice: it is
what every sector used to be, and fifteen of them played as one level repeated.
Each sector now has a floor plan that belongs to its theme, and
`test_campaign_levels_are_distinct_and_solvable` pins that no two share a size
or a storey rhythm.

| # | Theme | Plan |
| --- | --- | --- |
| 1 | `LOBBY` | the glazed entrance hall: a grand stair out of the atrium to a mezzanine gallery, a service ladder to the security wing, a short lift to the staff corridor behind the stair |
| 2 | `OFFICE` | three open-plan floors cut into blocks by floor-to-ceiling partitions, the ladders staggered so every partition is passed by changing floor; a welded stair core at the far end, the executive gallery back across the top, and a service crawl underneath reached by one ladder |
| 3 | `FACADE_NIGHT` | one wide breach per course, walking slowly from side to side |
| 4 | `SERVER` | cold aisles blocked at alternating ends: one long serpentine, plus two fenced pockets joined by a cable tunnel |
| 5 | `PLANT` | catwalk towers either side of a solid plant block, goods lift onto its roof, crane platform across the hall |
| 6 | `CANTEEN` | a double-height dining hall against a tight galley stack; the way up is the kitchen and its dumbwaiter |
| 7 | `FACADE_STORM` | short balconies instead of courses, so cover for the wind is sparse |
| 8 | `LAB` | a spine corridor with sealed clean-room bays combed off it above and below |
| 9 | `ARCHIVE` | a grid of canyons between blocks of shelving, linked only where an aisle was cut through |
| 10 | `SECURITY` | a patrol ring around the control bunker; both corridors are bricked up, so the bunker is the only way across |
| 11 | `FACADE_DAWN` | two breaches that braid, swapping sides as the climb rises |
| 12 | `DUCTS` | six crawl levels chopped into runs, one riser each; climb, drop through a missing panel, climb again |
| 13 | `FACADE_HIGH` | offset stubs laid like brickwork: every band is a lateral detour |
| 14 | `PENTHOUSE` | the only symmetrical plan: panelled rooms with single doorways around a double-height reception hall |
| 15 | `ROOF` | a skyline, not a floor plan: plant rooms of five heights under open sky, service level beneath the deck |

Two campaign-wide rules go with it, both pinned by the same test:

- **Pressure only rises.** A sector's hazard budget —
  `3·guards + 2·dogs + 2·mines + spikes + fans` — must exceed the previous
  interior's; a climb's budget (`3·throwers + 2·birds`) and its height must
  exceed the previous climb's. The campaign runs 6, 14, 20, 24, 30, 37, 48, 53,
  59, 64, 67 inside and 20, 25, 30, 35 on the walls. Sector 1 spends its whole
  budget on two guards and carries no hazard at all: it is where the player
  finds out what the controls do.
- **Every interior is finishable.** The test walks a conservative model of the
  player — walk, fall, step up one tile, jump a one-tile hole (two with a
  second open row overhead), hop one spike with that clearance, ladders, lift
  shafts, moving platforms, paired doors — and requires that it reaches the way
  out, every key card, every terminal and the restroom door, and that no tile
  it can reach strands it away from the exit.

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
  the standing row kills anyone who walks into it, crawling included.
- **Never hang a fan over a gap that has to be jumped.** The blades make the
  jump lethal while the route model, which knows nothing about fans, still
  reports the gap as crossed — the one way to build a sector the test calls
  finishable and the player cannot finish. Keep `O` at least two columns clear
  of any hole in the floor below it.
- **Ladders need not run the full height.** A run from the destination floor's
  headroom down to the source floor's standing row can be mounted and left at
  both ends; staggering short runs is what turns a floor plan into a route.
- **`F` panels are a shortcut, never a lifeline.** They fall away for the rest
  of the run, so the level must still be finishable once every one of them has
  gone, and no ledge they serve may become a place the player cannot leave.
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
