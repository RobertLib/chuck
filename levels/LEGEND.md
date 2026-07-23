# Level character legend

This file describes the meaning of characters used in the level text files.

- `#` : Wall (solid, impassable tile).
- `H` : Ladder (can climb up/down).
- (space) : Empty space / air.
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
- `E` : Exit / level end.
- `D` : Door tile (`TILE_DOOR`).
- `V` : Elevator shaft (vertical elevator track).
- `F` : Falling platform (falls after triggered).
- `P` : Moving platform (moves horizontally).

Notes:

- Office decorations are loaded only when placed directly above a static `#`
  wall tile; unsupported decorations are ignored so they cannot float in air.
- Hold `E` near the visibly active terminal to hack it and unlock the exit.
- Alarm switches are operated by guards. An active alarm alerts every guard
  and dog, then shuts itself off after nobody has seen the player for a short time.
- A `SPAWNS n0 n1 ...` line may appear after the grid. When present, it must
  contain exactly one spawn count for every door, in the order the doors
  appear in the file.
