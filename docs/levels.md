# The maps, and the rules they have to keep

## Levels

`Level` ([level.h](../src/level.h)) separates `LevelMap` (immutable parsed data),
`LevelRuntime` (mutable per-run: items, crates, elevators, unlock flags, which
weak walls have been blown open), and
`LevelReveal` (the tile-by-tile reveal animation). `level_load_data` parses the
text grid; it also makes the seeded choices — which key card is the real one and
which terminal is active (kept at least `TERMINAL_MIN_START_TILES` from the
player start).

Maps live as text in `levels/level*.txt` (campaign, natural-sorted) and
`levels/sublevels/*.txt`. [tools/embed_levels.py](../tools/embed_levels.py) turns
them into `build/embedded_levels.c` on every build. **Adding
`levels/level16.txt` is all the *build* needs for a new campaign level** — the
Makefile wildcards it in and progression is driven by `EMBEDDED_LEVEL_COUNT`.
A level is scored by its theme, not by its index, so the new sector's music
comes with the `THEME` line. Maps are text and can be edited as text, but
`make editor` is the tool that knows the rules — see
[The level editor](tooling.md#the-level-editor).

What the *campaign* needs of it is a longer list, and `make test` is where it
is written down: a size no other sector already has, a storey rhythm no other
sector already has, a hazard budget above the sector before it, a theme
different from its neighbour's, and a route the conservative model can walk. A
sixteenth sector is therefore an interior — sector 15 has no `Y`, so the
alternation puts an interior next, and `test_all_embedded_levels_parse` pins
`facade_levels == 4` outright.

**A fifth climb is not a map away, it is a constant away.** The test also
requires each climb to be taller than the last, and the four run 40, 44, 46 and
48 rows — level 13 is standing on `MAX_LEVEL_HEIGHT` itself. Another facade
sector means raising that cap first (it sizes `LevelMap`, so it is a memory
decision as much as an authoring one) and then relaxing the two rules above.
The editor already says so, as a note on level 13.

## Walls that open

A `%` tile is a weak wall: a blocked-up opening that is solid in every way a `#`
is until a blast takes it out. Three decisions carry the whole feature, and each
is tested.

**One solidity rule.** `level_is_solid` is the only thing that knows a weak wall
can stop being one, and everything that collides, shades, blocks a bullet or
breaks a line of sight already went through it. So opening a wall opens it for
the player, the guards, the crates, the ambient NPCs, the lighting pass and the
ambient occlusion in the same frame, and there is no second list of places to
update. Where a module had its own copy of the rule it now calls
`level_is_solid` instead ([gameplay_climb.c](../src/gameplay_climb.c)'s facade
collision, the janitor and receptionist probes in
[gameplay_ai.c](../src/gameplay_ai.c)) — a tile that is solid to the player and air
to a guard is a bug however it is drawn.

**The hole is runtime, not map.** `LevelMap` stays exactly what the file said, so
the editor, the parser and the tests all keep one answer for what a sector is;
the opened tiles live in `LevelRuntime.wall_broken` beside the fallen panels and
the broken crates. A lost life therefore keeps the hole and reloading the sector
restores the wall, which is the same bargain `F` panels make.

**Only a blast opens one**, and gameplay code never plays the sound itself:
`gameplay_break_walls_in_radius` ([gameplay_world.c](../src/gameplay_world.c)) is
called from `apply_blast` in [gameplay_combat.c](../src/gameplay_combat.c) — see
[One blast, one rule](gameplay.md#one-blast-one-rule) — and reports one `SFX_WALL_BREAK`
per blast plus `GAME_EVENT_DUST` per tile.
Dust is a new event rather than the existing spark burst because masonry is not
blood: sparks arcing away from a broken wall read as the wrong material however
many of them there are.

The route model in [level_route.c](../src/level_route.c) counts a weak wall as wall
in both directions — impassable, because opening one costs an explosive the
model knows nothing about, and floor, because a patch set into a slab must not
cut the storey in two. That is what keeps a `%` a shortcut and never the way
out, and it means placing one where a wall already stood cannot change whether a
sector is solvable. The editor adds the two rules the model cannot see: a sector
with a patch needs a grenade or a bazooka in it, and a patch on a climb never
opens at all, because nothing out there can set off a blast.

## One plan per sector

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
([tests/test_main.c](../tests/test_main.c)) pins three things the parser cannot
see: no two sectors share a size or a storey rhythm, the hazard budget rises
strictly from sector to sector (and from climb to climb, along with the climb's
height), and a conservative model of the player can reach the way out, every
key card, every terminal and the restroom door without ever being stranded by a
one-way drop. That model never stands on a falling panel, so a sector has to
work once every `F` has gone, and it never walks through a weak wall, so a
sector has to work before any `%` has been opened. [levels/LEGEND.md](../levels/LEGEND.md)
tabulates the plans, the budgets and what the model will and will not do.

**"Rises strictly" is a floor, not a shape, and the finale is where that
mattered.** The rule only asks each interior to beat the one before it, and
sector 15 was clearing it by three — 67 against sector 14's 64 — which passed
the test and made the last sector of the campaign the flattest step in the back
half, a shade easier than the floor below it and the one the whole night is
climbing toward. It is 79 now, and the difference is spent where the story
needs it rather than sprinkled: the roof of the last plant room is mined under
its own guard, the run out of the service level past the second breach is mined
at both ends under a fan, and the helicopter pad is held by a man standing on
it. Voss himself is never fought — the ending is his ride being taken away, not
a duel — so what the finale has to say is *how many of them are left between
Chuck and the roof*, and it has to say it in the map, because nothing else on
that screen can.

That route model lives in [level_route.c](../src/level_route.c) rather than in the
test file, because the editor asks it the same question about a map that is
still being drawn. Two copies would drift, and a sector the editor calls
solvable that `make test` then rejects is worse than no check at all.

**The model has to survive the landing too.** It was conservative about
everything except the one thing that kills outright: `route_landing` walked a
column down with no limit, so a fall of any depth was a move the player could
make, and a sector whose only way to a card or the door was a drop nobody
walks away from would have been certified by the suite *and* the editor.
`route_survivable_fall` caps it at `PLAYER_FATAL_FALL_HEIGHT` — derived in
[game_config.h](../src/game_config.h) from `PLAYER_FATAL_FALL_SPEED` and
`GRAVITY` rather than written down as a number of tiles, so retuning either
moves the model with it. The cap belongs to the **step off a ledge alone**:
`route_landing` itself stays unbounded, because its other two callers are not
falls the player takes — a card hanging in mid-air resolving to the floor it is
collected from, and the map's own `S` settling onto the floor beneath wherever
it was drawn. Hearts do not enter into it; a fatal fall calls
`gameplay_hit_player` directly. `test_the_route_model_will_not_take_a_fatal_fall`
pins the refusal, the survivable drop either side of it, and the fact that the
cap is not `route_landing`'s.

## Level themes

A `THEME <name>` metadata line picks the level's art direction and its score;
the palettes, wall materials and parallax backdrops all live in
[level_art.c](../src/level_art.c), which reads nothing but the immutable
`LevelMap` and so can never change how a level plays. Fifteen sectors of one
building would otherwise be fifteen runs down the same corridor, so every
campaign level names a different theme — a lobby, an office floor, a server
hall, an archive, a plenum, and four exterior climbs that differ by **weather
and height, never by hour**. Every theme name, what it draws and what it sounds
like is tabulated in [levels/LEGEND.md](../levels/LEGEND.md).

That last constraint is the clock's, and it is the one an art pass will break
without noticing. The night is thirty-eight minutes long and the wall clock `w`
hangs in every interior sector reading it out, so a climb is pinned to the
minute by the sectors on both sides of it. `FACADE_MOON` was `FACADE_DAWN` for
a long time and drew an actual sunrise — sun off the corner of the building,
warm stone, birds — in sector 11, which sits at 00:47 between two dials reading
a few minutes either side of it: the player looked up in sector 10, climbed
through a dawn, and looked up again in sector 12 to be told the sun had risen
and set inside five minutes. The composition survived the fix intact, because
what the beat was for is *one climb lit from the side by a single round source*
against three that are not; only the light changed, from warm to cold and from
haze to a disc with a hard edge. A fifth climb owes the same check. A server aisle and a rooftop are not the
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
`U` into the restroom, and every **even-numbered sector** — 2, 4, 6, 8, 10, 12
and 14, which is the odd-numbered *index* the test counts from zero — carries
exactly one bazooka. Say it as the sector number wherever a player will read
it: the manual spent a while telling them to look in the odd ones, which is the
half of the campaign that has no `Z` in it at all.
`test_all_embedded_levels_parse` pins that shape, so a new level has
to keep it: the alternation, the campaign ending inside the building, and no
rocket left out on a wall where nothing can be fired.

Every map character is documented in [levels/LEGEND.md](../levels/LEGEND.md),
along with the authoring rules the geometry has to respect (jump reach, spike
and fan clearance, gap widths); keep both in sync when touching the parser. An
optional trailing `SPAWNS n0 n1 ...` line gives per-door spawn counts and must
list exactly one number per door.
