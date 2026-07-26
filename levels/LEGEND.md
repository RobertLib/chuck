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
- `O` : Rotating ceiling fan (lethal on contact with the blades).
- `B` : Pushable crate (can be shoved or destroyed by shots/explosions).
- `L` : Small gas canister. A standing shot passes over it; crawl and shoot it to cause an explosion that can kill nearby enemies.
- `T` : Access terminal. One randomly selected terminal is active; the rest are decorative.
- `A` : Wall-mounted alarm switch. A guard may run to it after spotting the player.
- `c` : Decorative office chair (non-solid).
- `d` : Decorative office desk with a computer (non-solid).
- `i` : Decorative office equipment; its visual variant is selected from a filing cabinet, printer, or server rack (non-solid).
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

- Office decorations are loaded only when placed directly above a static `#`
  wall tile; unsupported decorations are ignored so they cannot float in air.
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
| `LOBBY` | polished marble, brass reveals | glazed street front, reception |
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
  put one directly above a `B` the player can stand on.
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
