# Level character legend

This file describes the meaning of characters used in the level text files.

- `#` : Wall (solid, impassable tile).
- `H` : Ladder (can climb up/down).
- (space) : Empty space / air.
- `C` : Card item (`ITEM_CARD`).
- `G` : Gun item (`ITEM_GUN`).
- `N` : Grenade item (`ITEM_GRENADE`).
- `K` : Medkit item (`ITEM_MEDKIT`).
- `M` : Enemy spawn (enemy is placed here).
- `W` : Enemy spawn with a guard dog.
- `X` : Mine (places an explosive mine).
- `^` : Spike / hazard (instant damage when stepped on).
- `B` : Pushable crate (can be shoved or destroyed by shots/explosions).
- `T` : Access terminal. One randomly selected terminal is active; the rest are decorative.
- `S` : Player start position.
- `E` : Exit / level end.
- `D` : Door tile (`TILE_DOOR`).
- `V` : Elevator shaft (vertical elevator track).
- `F` : Falling platform (falls after triggered).
- `P` : Moving platform (moves horizontally).

Notes:

- Hold `E` near the visibly active terminal to hack it and unlock the exit.
- A `SPAWNS n0 n1 ...` line may appear after the grid. When present, it must
  contain exactly one spawn count for every door, in the order the doors
  appear in the file.
