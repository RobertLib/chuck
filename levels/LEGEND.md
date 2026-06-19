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
  mine, gas canister) and the hole lasts as long as the visit does: a lost life
  keeps it, because a death respawns at a checkpoint rather than reloading, and
  reloading the sector — which is what a continue does — puts the wall back.
  That is the same bargain `F` panels make, and it is written up in
  [The hole is runtime, not map](../docs/levels.md#walls-that-open). Interiors
  only, and never the way out — see the authoring rule below.
  **And "any blast opens it" is not the same question as "the player has a
  blast".** All four go through `apply_blast`, which is why the list above is
  four items long; what an author has to provide is one the player can actually
  bring to bear. A grenade or a rocket can be carried to any patch on the plan,
  so either settles the whole floor. A gas canister cannot be carried, so it only
  answers for a patch inside its own radius — and it is the *better* answer where
  it fits, because shooting one spends none of the sector's explosive. A mine
  answers for nothing: it takes a patch out when it goes off, but nothing sets
  one off except the player's own weight or somebody else's blast, so a floor
  whose only opener is a mine offers one way through the wall and it costs two of
  three hearts. `check_weak_walls` in
  [editor_validate.c](../editor/editor_validate.c) asks it in exactly those
  terms; it used to ask for an `N` or a `Z` and report "nothing to open them
  with", which is a claim about the floor and was false on any floor with a
  canister beside the patch — and because the suite holds the campaign to nought
  warnings, being wrong in that direction forbade a design the game supports.
  **And it has to save a walk, which is the rule this entry did not have.** A
  patch is a shortcut: that is the whole of what the route model counting it as
  wall in both directions buys, and it is stated as settled fact in
  [docs/levels.md](../docs/levels.md#walls-that-open). Nobody measured it, and on
  four of the seven interiors that carried one — sector 10 holding six of the
  campaign's seventeen patch tiles — opening every patch on the floor shortened
  the route to the way out and to every pickup by **nothing at all**.
  Nothing was wrong with any of those *tiles*. It was the floor around them, which
  is the one thing no rule about a single tile can see, so the editor measures it
  and says so as a note (`check_weak_wall_shortcut` in
  [editor_validate.c](../editor/editor_validate.c)): open every patch, flood
  again, and ask whether the walk to the way out or to any pickup got shorter.
  Nought is the threshold and it is not a number anybody chose — a shortcut that
  shortens nothing is not a shortcut in any degree. It stays a note rather than a
  warning because whether a bolt-hole worth no steps is worth a rocket *under an
  alarm* is a judgement about risk, which this model does not measure and the
  author does.
  **Then the four were fixed, and the two ways they had to be fixed are the thing
  worth knowing before you draw one.** Sector 2's patch moved: it now sits at the
  foot of the partition beside the start, where blowing it drops Chuck through the
  slab into the basement instead of walking the long way round through the paired
  `D`, and it is worth 43 steps of a 56-step floor. Sectors 4, 6 and 10 lost
  theirs, because every position on all three floor plans was tried — vertical and
  horizontal, two tiles and three — and the best saving available anywhere on any
  of them is two or three steps. Those floors have ladders at four or five columns
  each, which is a good way for a floor to be built and exactly why a patch on one
  promises something the plan cannot deliver. **So the answer to a patch that
  measures nought is either a different place for it or no patch**, and which of
  the two it is is a question about the whole floor rather than about the tile.
  The campaign carries four now — sectors 2, 9, 12 and 14, worth 43, 7, 15 and 25
  steps — and every one of them is real:
  `test_a_weak_wall_is_a_shortcut_somewhere_in_the_campaign` requires every sector
  with a patch on it to save something, which it can only do because none of them
  is scenery any more.
  **And a patch with a slab tile in it is a fall, not a doorway.** Sector 2's is
  the campaign's only one, and it is safe because the door pair is the way back
  up. Nothing had ever asked that question: every route rule above is asked of the
  map *as authored*, and the floor a run actually walks for most of a sector is the
  one with the holes in it. `check_opened_walls_leave_a_way_out` asks it now, as an
  **error** — a hole that drops the player somewhere they cannot climb out of is a
  run ended by using the mechanic correctly.
  This is the mirror of the docket sheet's rule three entries down. That one has
  to *cost* a detour and seven of twelve were on a shortest path to the door; this
  one has to *save* one. Both were prose for as long as they went unmeasured.
- `=` : Ventilation duct — galvanised trunking let into the wall, louvred.
  **Masonry to the whole building and a way through to a man on his elbows**,
  which is the only tile in the game those two sentences disagree about. What
  the shaft is *for* is that disagreement: a guard cannot see into one, a round
  cannot be put through one, a blast does not open one and nothing standing on
  the floor can follow anybody in — all of which is a single answer
  (`level_is_solid`) rather than a special case in the AI, the ballistics or the
  lighting. What it costs is the other half of the same fact: the louvres are
  opaque both ways, so a player in a duct cannot see the room they are about to
  come out in, cannot stand up in one (`level_blocks_stance`) and cannot haul a
  body through one (`player_can_drag` refuses a crawler outright). The
  shaft is a bet rather than a shortcut.
  **Firing is the one item that used to be on that list and does not belong on
  it.** The trigger is not refused inside a shaft: the round leaves the clip, and
  because trunking still answers `level_is_solid` it reaches nothing — and the
  launcher spends the sector's rocket and puts its own blast on the man holding
  it, which is two of three hearts. That is the ordinary point-blank rule
  (a rocket fired with the muzzle in masonry does the same anywhere, see
  [One blast, one rule](../docs/gameplay.md#one-blast-one-rule)) rather than
  anything the duct does, and it is worth knowing precisely because a shaft is
  the one place the muzzle is *always* in masonry — there is no stepping back
  from it. Measured rather than reasoned about, and pinned by
  `test_a_shot_from_inside_a_shaft_reaches_nothing`.
  **The list it was on is the same sentence whose JUMP clause was prose for as
  long as it existed**, written up under the crawl below: a claim about what a
  shaft is *for* reads as a claim about what the simulation refuses, and only
  some of it was ever refused. Two of the four held, one was fixed in the code,
  and this one was fixed in the sentence.
  The drawing agrees: a man inside one is
  behind its louvres and shows through the slots between them, and that takes a
  pass of its own (`render_duct_fronts`) because the tile layer goes down before
  the figures do.
  Interiors only, like `%` and `I`: nothing on a climbed wall has a plenum
  behind it. It carries no decoration — a prop needs a `#` under it, and a
  grille is not one.
  **A duct needs a floor under every tile of its run**, because a crawl is
  horizontal: the tile the player is inside stops blocking them, so what holds
  them up is whatever the map put underneath. Trunking over a hole is trunking
  the player falls out of, and the editor says so.
  **And it needs somewhere to stand at both mouths**, or it is a hole that is
  crawled into and not out of. The route model reaches a duct only from a tile
  the player can stand on beside its mouth, and leaves it only onto another one
  — the crawl is the *only* move it allows from inside a shaft, so no jump, no
  step up and no hole hop starts in one. A duct with one mouth is not a route
  and nothing past it counts as reachable.
  **And the simulation keeps that sentence now, which it did not.** It was
  written about the route model and read as though it were about the game. One
  press of JUMP from the middle of a run put Chuck standing on the lid, because
  a rise is resolved in the posture he is in and trunking is open to a crawler
  upwards exactly as it is sideways — so a shaft could be left anywhere along
  its length, "a duct with one mouth is not a route" described nothing, and the
  opaque louvres the whole bet is made against could be lifted at any tile. The
  other direction had no sentence at all and was worse: crouching on the **lid**
  took the floor out from under the man doing it and stood him back up on the
  next step, 240 times a second, on every run in the campaign. **The lid is a
  walkway, not somewhere to lie down**, and a shaft is entered and left at its
  mouths — one rule, two directions, held by
  `test_the_lid_of_a_shaft_is_not_somewhere_to_lie_down` and
  `test_a_shaft_is_left_by_its_mouths`.
  Sector 12 carries the campaign's ducts, and it is the floor named after them:
  four runs of trunking, 61 tiles of it, on the storey that leaves by the window,
  the middle storey, the one below it and through the pillar that divides the
  storey under that. Two of them are the length of most of a floor, so a crawl
  across one is a decision about the whole storey rather than a step around a
  wall.
  **Placing a run moves whatever stood in it**, and the two things that matter
  are a ceiling fan `O` and the mouths. A fan reads a duct as its own floor —
  `band_floor_row` in the editor stops at the first thing that blocks, and
  trunking blocks — so a fan over a run needs every column within two of it to be
  duct as well, or the editor calls it a hole in the floor. And both mouths want
  two tiles of standing floor beside them, or the crawl is into somewhere nobody
  can walk out of.
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
- `!` : Flash charge (`ITEM_FLASHBANG`). One at a time, spent on the throw, and
  it crosses a sector boundary with the grenade and the rocket — **and a death,
  and the restroom door**, which for six sectors it did not: the shell kept its
  own copy of the loadout rule and the charge had been added to the other one.
  So the item placed for the moment a floor goes wrong was destroyed by the
  moment a floor goes wrong, and one `!` a floor does not respawn. That is worth
  knowing here because it is what the placement advice below rests on. **It is not a
  weapon**: it kills nobody, breaks nothing, opens no `%` and chains with no
  other explosive. What it does is take a room's attention away for
  `FLASH_BLIND_TIME` — everything in the room with eyes. Every guard inside
  `FLASH_RADIUS` stops seeing, stops aiming, stops walking and stops on his way
  to an alarm switch; **every dog** stops seeing, stops where it stands and loses
  the bite it was winding up; and every camera in reach forgets what it had. Then
  everyone comes back **exactly as they were**: still provoked, still hunting,
  still remembering where Chuck was.
  The charge buys seconds and never the encounter, which is what makes it the
  one answer in the game to having already been seen. Place one where a sector
  can go wrong rather than beside the door — it is the escape, not the entry.
  **It also has to be somewhere the player can stand**, which is the rule the
  docket sheet already states two entries down and this one was quietly failing:
  sector 14's charge sat on top of a partition five rows above the floor of the
  storey it belonged to, so the busiest floor in the building — twelve men, two
  cameras, three consoles — laid out the one answer to being seen and no player
  could ever take it. The editor's route check calls an unreachable pickup a
  **warning** now rather than a note, and `make test` requires nought of those.
  Optional to take is never optional to reach.
  And what a blast opens, a charge does not: the flash is stopped by a slab, a
  crate or an unopened `%` exactly as a guard's own eyes are, so a wall between
  the throw and a man is a man who keeps looking. Place it in the room it is
  meant to blind.
- `*` : A sheet off Meridian's own docket (`ITEM_EVIDENCE`) — the one pickup in
  the game that is worth nothing to the man carrying it. It pays
  `EVIDENCE_SCORE`, counts on the **run** rather than on the sector
  (`CampaignState.evidence_collected`, banked into `Progress.best_evidence` on
  every way a run ends), and does nothing else: no checkpoint, no counter the
  simulation reads, no respawn. **Exactly one belongs in every interior sector
  and none on a climb**, which `test_every_interior_lays_out_exactly_one_docket_sheet`
  pins — a collection the player can complete is the whole point, and one sheet
  short of twelve has to mean a floor they can name. Keep it inside what the
  route model can walk: it is optional, so nothing in an ordinary run would ever
  reveal one placed where the player cannot stand.
  **And put it somewhere that costs a detour**, which is a sentence this file
  carried for a long time with nothing behind it. Measured through the route
  model — the flood from the spawn, and a second one from the sheet, because a
  step off a ledge is a one-way edge and a flood from the door read backwards
  would lie about it — seven of the twelve sheets sat on a *shortest* path to the
  way out and sector 12's cost one step. So the collectable the whole docket is
  built out of was, on eight floors of twelve, picked up by walking to the door,
  and a collection that completes itself is not a collection. It is
  `test_the_docket_sheet_costs_a_detour` now, and the bar is a tenth of the
  sector's own walk rather than a number of steps: a floor plan runs from 19
  steps to 136, so eight steps is a decision on one and a rounding error on
  another. What the campaign ships is +11 to +92 steps, and the one closest to
  the bar is sector 4 at 22 against a 136-step walk.

**None of the three above is spent on a counter that is already full**, and
that rule is what makes it safe to put two of one kind on a floor. Walking over
a second `N` while carrying a grenade, a second `Z` with the tube loaded, or a
`K` with the hearts and the spare lives both at their cap leaves the pickup
exactly where it is, to be collected on the way back once it is worth
something; the pickup sound only ever plays for a pickup that did something.
`G` is deliberately the exception, because it is the one that comes back:
taking a magazine on a full clip costs nothing and the box is there again ten
seconds later. `test_a_pickup_that_would_be_wasted_is_left_alone` pins both
halves.

**A grenade, a rocket and a flash charge also survive the way out of a
sector.** Nothing else does — the sidearm is refilled anyway and the weapon in
the hand is reset to it — and the rule is `player_begin_sector` in
[../src/player.c](../src/player.c).
The climbs are why it exists: nothing on a facade can be thrown or fired at
all, so the `N` on each of the five is a pickup whose entire value is in the
sector above it.
- `M` : Enemy spawn (enemy is placed here).
- `W` : Enemy spawn with a guard dog.
- `Q` : Heavy guard. The same man in a plate carrier and a full helmet:
  `ENEMY_HEAVY_HP` rounds from the front instead of `ENEMY_HP`, `ENEMY_HEAVY_SPEED`
  of the ordinary pace, and — the reason he exists — **he cannot be stomped**.
  The stomp costs no ammunition and no position, and it is what a player
  reaches for the moment a floor gets busy; a heavy is where a sector takes it
  away. It was the *free* answer until a guard learnt to fire up the column the
  boot came down, which now charges a heart for the kill — so a heavy asks the
  same heart and gives nothing back for it. The blade behind him is deliberately
  unchanged, because a takedown is a knife across a throat rather than damage,
  which makes him a floor's clearest argument for the quiet route. Worth
  `ENEMY_HEAVY_HAZARD_WEIGHT` of the budget below rather than a guard's three.
  Reinforcements out of a door are never heavies: he is a fixture of a plan,
  placed where the author wanted the stomp denied, and one arriving at random
  would be a spike nobody drew.
- `J` : Ambient janitor with a cleaning cart and mop (visual-only NPC). Unlike
  the receptionist below, he is **not** confined to the lobby, and the
  difference is worth writing down because the two look like the same
  objection. A staffed counter is a post: somebody standing at it at 00:51 is
  being *served by the building*, which is why `k` belongs to sector 1 and to
  nothing above it. A janitor is one man alone on a floor with a cart, and the
  building never told him anything — the crew took the lobby, not the whole
  night shift, and nothing about a mopped corridor two floors up claims he has
  heard a shot. He may stand in any interior sector; he still owes the ordinary
  rule that he never fights, blocks or is seen.
  **He is also the one body that keeps off every moving surface in the
  building.** `janitor_has_floor_ahead` asks about masonry and ladders and
  nothing else, so a plate, a panel and a deck are all the end of his run — which
  is deliberate, because he is the one body with no ride of any kind and giving
  him three would be three mechanics nobody will ever watch. A lift shaft used to
  slip through that, since the tile is passable and the lowest one in a run has
  the storey's own floor underneath: on sector 5 he walked into the bottom of the
  goods lift and the descending deck crossed his chest for up to 0.70s, twelve
  times a minute of play. Drawn every run, counted as covered, and seen by nobody
  until somebody read the frame. `janitor_side_blocked` refuses a `V` outright
  now.
  **A crate is the other one that slipped through, and for a different reason:
  it is not a tile at all**, so a rule written in tiles could not see it whatever
  it asked. He walked clean through a `B` — measured, up to 19% of a two-minute
  visit spent standing inside a box at the full 26px of his own width, with
  nobody pressing anything, on four of the five floors carrying both a `J` and a
  `B` (sectors 2, 5, 6, 9 and 10) and in the lobby's washroom, which carries
  both as well. A
  *guard* doing the same thing is deliberate and is written down beside
  `gameplay_resolve_enemy_crates`: he is drawn after the crates, so the overlap
  reads as the foreground route past one. The janitor is drawn *before* them, on
  the ambient-staff layer, so the box is drawn over him and a full overlap leaves
  the top four pixels of his head. He turns at one now the way he turns at
  masonry, which keeps the rule above intact — the crate is untouched, so he
  still blocks nothing. **A box that lands on him does not stop him**, and that
  arm is what makes the rule safe rather than a trap: nothing prevents a shove or
  a fall putting one there, and without it he turned on the spot for the whole of
  a sixty-second probe.
  So placing a `J` needs no rule of its own about crates, which is the point of
  fixing it here rather than in the validator.
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
  **It is the most expensive tile on this page — two hearts of three — and for a
  long time it was the only hazard with no rule at all**: five checks for a fan
  that costs one heart, one for a spike bed that costs one, and nothing
  whatsoever for the charge. It is invisible to the route model too, which knows
  a spike and has never heard of a mine, so a certified route runs straight over
  every one of them.
  None of what the editor now says forbids a mine anywhere. A mine is a tile
  that makes the player look at the floor and the answers are all still there —
  see it and walk round it, hop it, or set it off from a distance with anything
  that explodes. What it warns about is a charge whose answers something *else*
  on the plan has taken away: blades in its own column, where ducking the fan
  lands on the charge and hopping the charge lands in the fan; a mine right
  against a ladder or a shaft, which is stepped on coming off the rungs rather
  than walked into; and two abreast, which cannot be hopped at all and so cuts
  the floor in half at twice a spike bed's price. It notes a charge with nothing
  under it, since weight is what arms one.
  **One column off a riser and not two, which is worth knowing because the fan's
  rule says two.** Blades reach: `CEILING_FAN_BLADE_LENGTH` hangs better than a
  tile either side of the fan's own column, so a fan two columns off a ladder
  genuinely overhangs the arrival. A mine is exactly its tile and reaches
  nothing, so borrowing the fan's number would have been borrowing a span whose
  justification does not come with it. Measured on the shipped maps, two columns
  flags six tiles across four of the seven mined floors and one column flags the
  two where the step really has nowhere to land.
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
- `T` : Access terminal. One randomly selected terminal is active; the rest are
  decorative. **A console is also a way of putting more men on the floor**: one
  hacked while the alarm is up sends for up to
  `TERMINAL_REINFORCEMENT_MAX_COUNT` guards out of a `D`, once per console, and
  they need somewhere to stand alongside the men already drawn and the corpses of
  the ones that have gone down. So a floor's real guard count is the `M`, `W` and
  `Q` on it plus two per `T` — and only if it has a door at all, since that is
  where an arrival comes from. Over `MAX_ENEMIES` nothing fails, which is why the
  editor warns rather than refuses: `find_enemy_slot` hands the arrival the
  corpse furthest from Chuck, so the floor deletes a body and with it the thing
  the next calm guard was walking over to look at. The quiet route rests on
  bodies being readable, so that is the one thing a floor plan must not spend.
  `test_every_sector_can_seat_the_reinforcements_it_can_call` holds the shipped
  maps to it.
  **And `SPAWNS` is the other way men come out of a door**, which that seating sum
  omitted for as long as it existed and the editor's warning could not see at all.
  It is the *deterministic* path — the line is copied into `door_spawns` at level
  start and dripped out on a timer with no alarm and no console involved — and it
  lands in the same `spawn_enemy_from_door`, taking the same slots and rolling the
  same handler. So a floor's real guard count is the `M`, `W` and `Q` on it, plus
  two per `T` if there is a door, plus every value on its `SPAWNS` line. Measured
  that way sector 14 reaches **21 of 24** where the check reported 18, and the
  editor's warning was gated on the console count, so a map whose whole
  over-subscription came out of `SPAWNS` was never warned about — nothing bounds
  the value either. Both hold the whole sum now, dogs included.
  **The threat only exists where the floor has a door**, and that is worth
  knowing before you place a console rather than after: the reinforcement penalty
  for hacking under an alarm is live on sectors 4, 8 and 14 and on no others,
  because the rest of the floors carrying a `T` have no `D` for anybody to come
  out of. On those the console is a lock to pick and nothing else. It is a
  deliberate shape — the vault and the roof get their pressure from fourteen and
  fifteen men rather than from arrivals — but it is a shape nobody chose in one
  place, so it is written here where it can be checked.
- `A` : Wall-mounted alarm switch. A guard may run to it after spotting the player.
- `I` : Ceiling security camera. It sweeps a beam across the floor below and
  raises the building alarm after `CAMERA_NOTICE_TIME` of holding the player in
  it — pointed at the *player*, not at the camera, so being seen by one costs
  exactly what being seen by a guard costs. **It is the one watcher in the
  building that none of the quiet answers work on**: it has no facing to get
  behind, no ears for a bolt, and crawling does nothing because it is looking
  down at the floor the crawl is on. What it has is a sweep, so the answer is
  timing — or a round, which destroys it permanently for the visit, scores
  `CAMERA_SCORE`, and is the loudest thing in the game. Any blast takes one out
  as well. **It hangs**, so like the wall clock `w` it needs a solid tile
  directly *above* it and the loader drops it if there is none. Interiors only,
  and worth two of the hazard budget below.
  Place one where the beam crosses ground the player has to walk, not over a
  dead end: a camera nobody has to pass is a fitting rather than an obstacle.
  `CAMERA_RANGE` is five and a half tiles, so a mounting more than five storeys
  above the floor it is meant to watch is watching nothing.
  **Both of those sentences were prose for as long as the mechanic existed, and
  the second one is not the way a camera actually goes blind.** The failure is
  the opposite of a mounting too far above its floor: it is a mounting with no
  air under it. `camera_sees_player` refuses anything at or above the lens, and
  a standing player is exactly one tile tall, so in a **one-tile-high space a
  camera is level with his chest and sees nothing at all** — only a crawler, and
  only within about a tile, on the fitting whose own manual sheet promises "it
  looks down - crawling is no help". Sector 12 hung both of its cameras in the
  service gap over a duct run, where the tile below the lens is trunking and
  therefore masonry: measured, each watched **nought** cells a standing player
  could occupy, against five to fourteen for every other camera in the game, and
  reaching either beam at all cost a detour of +78 and +101 steps on a 133-step
  floor. They cost two of the hazard budget each, so that floor's stated
  pressure was four higher than the floor it described. Both moved — one to the
  corridor west of the row-8 duct's mouth, one to the corridor west of the
  row-14 duct's — which is the placement the original was reaching for and had
  upside down, because **a duct is cover from a camera** (`level_is_solid` stops
  the beam) and a lens watching the floor beside a mouth is what makes the shaft
  worth crawling.
  `check_cameras` in [editor_validate.c](../editor/editor_validate.c) asks both
  halves now: a camera with no slab above it, and a camera whose beam reaches no
  tile a standing player can occupy. The first of those was already the loader's
  rule and the comment beside it in [level.c](../src/level.c) claimed the editor
  said so while the map was being drawn — it did not, because
  `editor_symbol_hangs` answers for the clock `w` and the camera `I` and its
  only caller sits inside `check_decorations`, which filters to the four
  decoration groups. An `I` is `ED_GROUP_FITTINGS`, so the clock was caught and
  the camera was never asked.
  **Reachability is deliberately not the bar and neither is the shortest
  route.** Seven of the campaign's eleven cameras watch only ground that costs a
  detour of +10 to +61 steps, and every one of those is a lens guarding a pickup
  or a side route, which is what the fitting is *for*. The question that
  separates an obstacle from an ornament is not how far the ground is but
  whether there is any.
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
  **That was advice nothing acted on, and two floors were not taking it.** Nine
  interiors put their case within two tiles of an `N` or a `Z`; sector 8 had its
  nineteen tiles away and sector 14 eleven, so on the two busiest floors below
  the roof the box was just a box. Both have been moved and the editor notes a
  sector whose nearest case is more than six tiles from an explosive.
  The note is asked of **the sector, not of each case**, which is the same
  distinction the sentence above already draws: a run is sanctioned, so the roof's
  four cases along the service deck are right to be a run and only one of them has
  to be making the point. One case saying it is the point; four saying it is the
  same sentence four times. A flash charge does not count — `!` is not a weapon
  and a case it came out of is a case with nothing in it worth the sentence — and
  a sector carrying no explosive at all is not asked, which is what keeps the note
  off sectors 1 and 5.
- `a` : Decorative pallet, loaded with drums or with sacks chosen from the tile
  position (non-solid). The first of the **plant set** — `a` `e` `j` `l` — which
  exists because the campaign had two vocabularies and five rooms neither of them
  described; see the note on sparsely dressed sectors below. A pallet is the
  stacked thing a machine hall, a duct run or a strongroom is actually full of,
  and it is the one to reach for when a floor needs to look worked in rather than
  furnished.
- `e` : Decorative cable reel, stood on its edge with the tail run off to one
  side (non-solid). Plant rooms, the roof deck, anywhere cable was pulled and the
  drum was left where it emptied.
- `j` : Decorative pipe rail: two risers, a run across them and a hand wheel on
  the valve (non-solid). The plumbing a machine hall is made of, and the only
  prop in the set with a moving part — the wheel sits at a different angle on
  every one of them.
- `l` : Decorative bollard with a reflective band (non-solid). What keeps
  something with a pallet on it off a wall, so it belongs on a service route
  rather than in a room: a goods deck, a loading bay, the strip of roof a
  helicopter is loaded from.
- `w` : Decorative wall clock (non-solid). **The one prop that hangs rather
  than stands**: it needs a solid tile directly *above* it, and the loader
  drops it if there is none, exactly as it drops a desk with no floor. The
  dial reads the campaign sector it is standing in — the night runs from the
  broadcast to the bonds leaving the roof at 01:00, so the minute hand climbs
  toward the top of the face across the seventeen sectors. Presentation only:
  nothing in the simulation reads the time.
- `S` : Player start position.
- `E` : Normal security-door exit. When the map also contains `Y`, this door
  is physically blocked and cannot be unlocked by a card or terminal.
- `Y` : Open traversable window. In an interior it is the alternative route
  out; in a `MODE FACADE` level it is the window back into the building.
- `D` : Door tile (`TILE_DOOR`).
- `U` : Entrance to a separate sublevel — the restroom off this floor. **Which
  room it opens on is decided by the sector's `THEME`**, exactly as the score
  is (`level_theme_sublevel` in [level.c](../src/level.c)), so a sector names
  its room by being what it already is and a theme with no room of its own
  falls back to the lobby's. The four rooms are tabulated under
  [Restrooms](#restrooms) below.
- `R` : Return door from a sublevel to its paused parent level.
- `q` : Decorative restroom toilet (non-solid).
- `b` : Decorative restroom washbasin (non-solid). A mirror is drawn in the
  tile above it, and reflects Chuck when he stands in front of it.
- `u` : Decorative restroom urinal (non-solid).
- `p` : Decorative restroom stall partition (non-solid).
- `o` : Open restroom stall with a visible toilet (non-solid).
- `z` : Closed restroom stall door (non-solid).
- `V` : Elevator shaft (vertical elevator track). A deck carries its rider to the
  top of its own run and no further, which is the rule the route model has always
  been certified against (`route_in_shaft` only ever answers for tiles that *are*
  shaft) and which the simulation had to be made to agree with — see the note in
  AGENTS.md on the guard who stood in sector 6's ceiling. Guards ride one; dogs
  and the janitor do not, and neither of their floor tests counts a deck as
  floor, which is what keeps them out of a shaft rather than standing in one.
- `F` : Falling panel. **Only Chuck's weight arms it**, like a mine `X`, and
  once armed it goes for the rest of the visit — a lost life keeps the hole,
  reloading the sector restores the panel. Everything else in the building
  walks over it: guards, dogs, the ambient NPCs and every dropped body pass
  `triggers_falling` as false. A dog used to be the exception, which spent
  sector 12's panel about a second into every run and left a hole that boxed
  the guard beside it into one tile.
- `P` : Moving platform (moves horizontally). It patrols between the ends of its
  own row's clear run, and **it carries everything it holds up** — Chuck, a
  guard, a dog. The support was never the interesting half: `level_move` has
  always settled every body onto a plate the same way it settles one onto a
  falling panel, so a guard walking across one has always worked. The *carry* was
  the player's alone, so a guard who stopped on a plate was left behind by it and
  dropped off the trailing edge 0.44s later — and that is reachable by design
  rather than by luck, because `enemy_floor_in_col` counts a plate as floor on
  purpose so a pursuing guard does not mistake one for a gap. Measured, the AI
  leaves bodies resting on plates for 716 frames on sector 14 and 1512 on
  sector 17 over twenty-four minutes of play a sector, and sector 17's plate is
  eleven tiles long.
  What it will *not* do is push a rider into masonry: a plate's limits are the
  ends of its row, so the plate never reaches a wall and the rider does — a tile
  is 32 and a body is 26. The carry is refused for that frame instead, which is
  the same rule and the same function for every body on one.
  A crate is the deliberate exception and keeps its own note above: a box rests
  on a plate and is not carried, so the plate slides out from under it. And the
  janitor is the other, for the reason under `J`.
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
- **The plant set is what this page used to be asking for, and it is the answer
  to five sectors that were bare for want of a vocabulary rather than for want of
  work.** Counted by prop, the campaign used to split clean in two: the office and
  front-of-house themes carried sixteen to twenty-three apiece and PLANT, LAB,
  DUCTS, VAULT and ROOF carried 5, 9, 3, 6 and 6. It read at a glance like the top
  of the building running out of attention, and it was not: PLANT is sector 5, and
  was as bare as DUCTS at 12. What those five have in common is a *room*, not a
  position — a machine hall, a clean room, a duct run, a strongroom and an open
  roof deck — and the decoration vocabulary had no set for any of them. `c` `d`
  `i` is an office and `n` `s` `t` `g` is a foyer.
  The wrong answer, and the one a reader is most likely to reach for while
  believing they are tidying up, is the nearest set: a desk and a filing cabinet
  in a strongroom, a swivel chair on a roof. That is the rule directly above. Nor
  is it another `m` — the roof already runs the sanctioned four, and a fifth is
  the same sentence a fifth time. The right answer was a third vocabulary, and
  `a` `e` `j` `l` are it: a pallet, a cable reel, a pipe rail and a bollard, in
  [level.h](../src/level.h), [level.c](../src/level.c),
  [game_render.c](../src/game_render.c) and
  [editor_legend.c](../editor/editor_legend.c), which is a change to the
  vocabulary rather than to a map. All twelve interiors now carry fifteen to
  twenty-three props apiece, and `check_docs.py` derives that range from the maps
  so a sixth bare room cannot open without this page saying so.
  **What the set is not is a licence to dress every floor out of it.** A pallet on
  a lobby carpet is the same mistake as a swivel chair on a roof, one set over: the
  three vocabularies answer to the room, and a floor that is an office is furnished
  as one.
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
- **A sector's checkpoints are the things it lays out, so lay one on the route.**
  A death costs a life and puts Chuck back on the last banked checkpoint, and
  only four things bank one: a card, a finished hack, a step through a door pair
  and a medkit. A floor that puts all four somewhere the player has no reason to
  walk is a floor that replays whole, however carefully its hazards were spent —
  and the sectors that leave by a window are the ones this catches out, because a
  welded stair door means they carry no card and no terminal at all.
  `test_no_sector_asks_for_a_long_walk_with_nothing_banked` floods the route from
  both ends and requires at least one bank on the line, no further than
  `CHECKPOINT_MAX_STRETCH` steps from the last. A bank more than a few steps off
  the way out does not count: a detour nobody takes banks nothing.
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
  attack — so a climb's pickups are entirely a bet on the sector above it.
  **Which means only what actually crosses the doorway is worth placing**, and
  that is the grenade, the rocket and the flash charge: `player_begin_sector`
  carries those three and nothing else, because `player_reset` hands the next
  sector a full clip either way. So every climb carries an `N` and a `K`, and
  none of them carries a `G`.
  All four of the climbs there were then did, on the stated grounds that sector
  7's climb otherwise handed into the LAB "with whatever ammunition was left" —
  which was never true of any sector, since the clip is refilled at every
  doorway. A magazine on a wall
  cannot be spent up there, changes no counter when it is walked over, pays no
  score, and plays the sound of a pickup that worked.
  `test_no_climb_lays_out_a_pickup_it_cannot_use` is what keeps one from coming
  back.
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

**With one exception, and it is how the four restrooms get their look.**
Placing any restroom fitting — `q`, `b`, `u`, `p`, `o` or `z` — sets the theme
to `RESTROOM` as the grid is read, which is why none of the four sublevel maps
carries a `THEME` line and why `RESTROOM` is the one name in the table below
that no map ever spells. A tiled room is a tiled room whatever it hangs off, so
the fittings are the sentence and the metadata line would only be a second copy
of it. The line still wins where a map has both — it is read after the grid —
so an interior that wants a washbasin in the corner without becoming a
washroom says so by naming its own theme, which every campaign map already
does.

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
| `VAULT` | riveted steel plate | raised access panel | strongroom core, open grilles, empty boxes | 66 bpm C minor; no kit at all — the only score in the building with no pulse, because every other floor has somebody working on it |
| `ROOF` | raw concrete | screed | curtain wall over the night city | 96 bpm F♯ minor; the lead finally carries the loop, over open wind |
| `RESTROOM` | tile | ceramic | implied by restroom fittings; sublevel only | 72 bpm E♭ minor; tiled and dripping, no kit — the sector's score waits outside |
| `FACADE_NIGHT` | — | — | clear night, city lights below | 90 bpm E minor; wide, slow and exposed, the city glittering under it |
| `FACADE_STORM` | — | — | rain, lightning, wet stone | 116 bpm C minor; the only climb in a hurry, rain on the cornices |
| `FACADE_MOON` | — | — | a low moon off the corner, silver stone, haze bands, distant birds | 82 bpm A major; the storm has blown through and the sky is clear — a breath, not a sunrise |
| `FACADE_SLEET` | — | — | the storm's weather at a later hour, a step colder and paler | 96 bpm A minor; the hat carries the whole of the time, because that is what sleet on stone sounds like |
| `FACADE_HIGH` | — | — | the thinnest air of the climb, the city a violet glow between torn cloud far below, neon signage | 70 bpm B minor; thin air — barely a bass note, a long way between phrases |

Two rules the campaign keeps, both pinned by
`test_campaign_themes_keep_changing`: no two consecutive levels share a theme,
and a facade level uses a `FACADE_*` theme while an interior never does. The
second rule is what keeps the scores changing too, since the theme-to-track
mapping is one to one.

**The five climbs differ by weather and height, never by hour**, and that is a
constraint rather than a preference. The night is thirty-eight minutes long —
00:22 to 01:00, `NIGHT_CLOCK_*` in [../src/game_config.h](../src/game_config.h)
— and the wall clock `w` hanging in every interior sector reads it out, so a
climb between two of them is pinned to the minute on both sides. `FACADE_MOON`
was a sunrise until it was measured against the sectors either side of it,
whose dials read 00:42 and 00:46: the player looked up in
sector 10, climbed through a dawn, and looked up again in sector 12 to be told
the sun had risen and set inside five minutes. A fifth climb owes the same
check before it is painted.

**And the hour is not the only thing a climb can contradict; the weather is the
other, and the fifth climb was contradicting it.** `FACADE_HIGH` at sector 13
described itself as *above the weather, a sea of cloud below* — and sector 15,
which is higher, is sleet. So the player climbed out of the weather two floors
under the roof and back into it on the way to the roof, which is the `FACADE_DAWN`
mistake with the clock swapped for the sky. The picture did not have to move: the
backdrop draws seven drifting puffs and a violet city glow coming up between
them, which reads as **broken cloud a long way below** rather than as a floor of
it, and that is what it says now. What makes the beat is one purple sky and a
city that has stopped being a street; a continuous deck was never load-bearing.
**A climb owes the sector above it a look as well as the two dials either side.**

**Those two minutes are derived rather than remembered**, and they were `00:47`
on both sides of this sentence until somebody divided the same thirty-eight
minutes seventeen ways instead of fifteen. The dial moved and the paragraph
describing it did not, which is the whole reason
[tools/check_docs.py](../tools/check_docs.py) computes every reading it quotes
from `NIGHT_CLOCK_*` now.

## Sector plans

No two sectors are laid out the same way. A stack of full-width corridor bands
joined by ladder columns is the one plan the campaign does not use twice: it is
what every sector used to be, and fifteen of them played as one level repeated.
Each sector now has a floor plan that belongs to its theme, and
`test_campaign_levels_are_distinct_and_solvable` pins that no two share a size
or a storey rhythm.

**Several of the mechanics on this page belong to one floor each, and that is a
decision rather than an oversight.** It is worth writing down because counting
the maps makes it look like the opposite: trunking (`=`) is on sector 12 alone,
lift shafts (`V`) on sectors 5, 6 and 10 and never again, moving platforms (`P`)
on sectors 5, 14 and 17 with a single panel each. Read as a running mechanic, a
duct that costs a split solidity rule, a renderer pass of its own, a route-model
edge and two editor checks for one floor of twelve looks like a thing that was
started and dropped. It is not: it is what makes sector 12 the floor called
`DUCTS`, the same way the goods lift is what sector 5 is and the gondola is what
the roof is. A sector's identity is a mechanic the rest of the building does not
have.

The cost of that is real and is the other half of the decision: a mechanic met
once is met without practice, so the floor that introduces it must also be the
floor that teaches it, and none of them may be the last one before something the
campaign turns on. What would be an actual defect is a mechanic on *two* floors
with the second assuming fluency the first did not build.

**The teaching load is not spread evenly either, and sector 2 carries most of
it.** Sector 1 introduces the basics a player cannot refuse — ladder, guard,
console, card, medkit, alarm, the restroom door, the docket sheet — and sector 2
then introduces ten more: the crate, the canister, the grenade, the bazooka, the
fan, the falling panel, the weak wall, the door pair, the window and the janitor.
Everything after it arrives one at a time and slowly: the mine on 8, the heavy on
10, the duct on 12. That front-loading is on purpose — the second floor is where
a player is still willing to be taught, and a grenade withheld until sector 6 is
a grenade the player has already decided the game does not have — but it means
**sector 2 is the one floor where a new tile is a real cost**, and anything added
to it is the eleventh thing in one room.

| # | Theme | Plan |
| --- | --- | --- |
| 1 | `LOBBY` | the glazed entrance hall: a grand stair out of the double-height atrium to a gallery, a service ladder up to the security wing where the exit is, and a second service ladder down behind the reception line to the staff corridor with the restroom. The hall empties past Chuck as he walks in — see `f` — and the front desk stays staffed after it has, see `k` |
| 2 | `OFFICE` | three open-plan floors cut into blocks by floor-to-ceiling partitions, the ladders staggered so every partition is passed by changing floor; a welded stair core at the far end, the executive gallery back across the top, and a service crawl underneath reached by one ladder, where the bazooka lies beside a blocked-up opening (`%`) that saves the teleport across |
| 3 | `FACADE_NIGHT` | one wide breach per course, walking slowly from side to side |
| 4 | `SERVER` | a serpentine of four aisles walked in alternating directions, plus two fenced pockets — the vault above, the terminal room below — joined by a service ladder, entered by an airlock door pair or by dropping through the cable gap in the hot-aisle floor |
| 5 | `PLANT` | catwalk towers either side of a solid plant block, goods lift onto its roof, crane platform across the hall to the exit ledge; spiked pit under the crane gap. **It lays out no grenade and no rocket, and that is the washroom's job**: it is the only interior above a hazard budget of 20 with neither on its plan — sectors 2 and 4 under it carry both — so the floor's blast is behind the `U`, which is what makes that detour worth the walk here rather than a free medkit. Written down because a supply outlier nobody has explained is indistinguishable from an oversight, and the obvious "fix" is to drop an `N` in the plant hall and delete the reason. `test_every_interior_can_reach_a_blast` holds the rule rather than the list: a floor with no explosive on its plan has a door to one |
| 6 | `CANTEEN` | a double-height dining hall against a tight galley stack; the way up is the kitchen and its dumbwaiter, and a blast through the galley wall is the way back down |
| 7 | `FACADE_STORM` | short balconies instead of courses, so cover for the wind is sparse |
| 8 | `LAB` | a spine corridor with sealed clean-room bays combed off it above — each an airlock reached by its own ladder — and basement chambers and a deep-storage crawl below; the rocket vault opens only through a paired door |
| 9 | `ARCHIVE` | a grid of canyons between blocks of shelving, linked only where an aisle was cut through; the route weaves up, across the reading room, and back down before the exit stair |
| 10 | `SECURITY` | a patrol ring around the control bunker; both corridors are bricked up, so the bunker's lower floor is the only way across, and the airlock doors flanking it are where the reinforcements come out. A single lens has been overhead since sector 5, but **sector 10 is the first with a pair of them**, which is the floor the monitor wall belongs to |
| 11 | `FACADE_MOON` | two breaches that braid, swapping sides as the climb rises and merging where the lines cross |
| 12 | `DUCTS` | six crawl levels chopped into runs, one riser each; climb, drop through a missing panel, climb again. The rocket pocket hides behind a blocked-up bulkhead. Two cameras, and the crawl that gets a player past a guard's cone does nothing at all about them |
| 13 | `FACADE_HIGH` | offset stubs laid like brickwork: every band is a lateral detour |
| 14 | `PENTHOUSE` | the only symmetrical plan: panelled rooms with single doorways around a double-height reception hall crossed by balcony stubs. The far bay keeps a medkit and the rocket behind a paired door whose other end is the one guards come out of; the `%` in the bay's end wall is a second way in, for anyone who can spare a blast. **The second medkit is on the middle storey between the first heavy and a mine**, and it is there because this floor was the campaign's one outlier on supply: the highest pressure below the roof, and the only late interior with no crate and no canister on it, so the two floors under it were carrying more of both on less of a budget Two cameras over the middle storey. **It leaves by the window and its stair core is therefore welded** — sector 15 is a climb and a climb is entered from a window — which makes it the fifth of the welded sectors and the only one of them that still lays out cards and a terminal, since both score and bank a checkpoint whatever the door is doing. It is also the reason it shows no report: see [intel.c](../src/intel.c) |
| 15 | `FACADE_SLEET` | paired courses with the breach swapping side every band, so no two are left by the column they were entered on — the tallest climb in the game and the only one that ends at a roof rather than a window |
| 16 | `VAULT` | four storeys of unequal depth around the strongroom core: the trolley run they emptied it onto at the bottom, the boxes themselves above it, the handling floor where they were still working, and the way out over the top. The risers are staggered end to end, so every floor is crossed the full width. Two cameras, and the report after it is the one that answers sector 8 |
| 17 | `ROOF` | a skyline, not a floor plan: plant rooms of five heights under open sky, a gondola strung between the two towers, and a fan-choked service level beneath the deck that is the only way past the two breaches in it. **The last stretch is where the crew is**: the roof of the final plant room is mined under its own guard, the run out of the service level past the second breach is mined at both ends under a fan and three more times along its length, and the helicopter pad is held by a **heavy** standing on it — the last man in the campaign is the one a boot bounces off, so the free answer is gone exactly where the player most wants it. Voss's remaining crew between Chuck and the ride, which is the one thing the finale has to say |

### Restrooms

Four sectors carry a `U`, and each opens on a room of its own — picked by the
sector's `THEME`, so the room belongs to the place it hangs off. There used to
be one room behind all four doors, which meant the marble washroom off the
lobby and the executive suite two floors under the roof were the same
twenty-nine tiles with the same guard standing in the same place.

**What they pay is deliberately identical**: one magazine on the floor, one
grenade and one medkit up top. The campaign is balanced on four grenades coming
out of these doors on top of the sixteen it lays out itself, and a room that
paid differently would move that line. What changes is the plan, the climb and
what is waiting — which is what was actually repeating.

| Sector | Room | Plan | Waiting |
| --- | --- | --- | --- |
| 1 `LOBBY` | `restroom_lobby` | the room the game teaches with: one ladder straight up to a service walkway broken once | a guard, a janitor, a crate and a canister |
| 5 `PLANT` | `restroom_plant` | a works toilet under a long mezzanine broken twice, and the only ladder is at the far end — so both pickups are walked back over both breaks | a guard, and a pair of canisters close enough to chain |
| 9 `ARCHIVE` | `restroom_archive` | three short levels behind the stacks, the risers at opposite ends so the climb doubles back on itself each time; the top shelf has a break of its own | two guards |
| 14 `PENTHOUSE` | `restroom_penthouse` | the only symmetrical one, like the floor it hangs off: a centre riser with a wing either side and a break in each, the grenade in one wing and the medkit in the other | two guards and a dog — and the riser is the way out of the dog's reach, since dogs do not climb |

`test_embedded_restroom_sublevels` walks the route model through every one of
them to both pickups and back to the door, holds the three-pickup payout, and
requires that no two share a footprint;
`test_every_restroom_theme_names_a_room_that_exists` holds the theme table
against the files actually embedded, because a renamed map would otherwise be a
`U` that silently falls back to the lobby's washroom.

**And no two may share a storey rhythm either**, which is the campaign's own
distinctness rule (`test_campaign_levels_are_distinct_and_solvable`) asked of the
rooms — and it had never been asked, so two of the four were the same room. A
footprint is the outline; the rhythm is the run of open rows between slabs, which
is the part the player actually walks. The plant's washroom and the penthouse's
both ran a two-row gallery over a slab broken twice over a three-row floor, so
what told them apart was the tiling, the fittings and nothing else. The plant's
gallery is three rows deep now and its lower room two, which is the same plan the
table above describes at a different pitch.
`test_the_restrooms_are_four_rooms_rather_than_one` measures both halves — the
shape, and the payout being identical on purpose.

Two campaign-wide rules go with it, both pinned by the same test:

- **Pressure only rises.** A sector's hazard budget —
  `3·guards + 4·heavies + 2·dogs + 2·mines + 2·cameras + spikes + fans` — must exceed the
  previous interior's; a climb's budget (`3·throwers + 2·birds`) and its height
  must exceed the previous climb's. The campaign runs 6, 14, 20, 26, 31, 39, 48,
  58, 65, 70, 77, 89 inside and 20, 25, 30, 35, 43 on the walls, which are steps of
  8, 6, 6, 5, 8, 9, 10, 7, 5, 7 and 12.
  **These are measured rather than written down, and the paragraph they replace is
  the argument for measuring.** It quoted 57, 63, 68 and 79 for the back four, all
  four of them wrong, while claiming the last number was the largest step in the
  run — and by then it was the *smallest*, tied at two. What happened is worth
  knowing because nothing in the suite could have said so: the `pressure only
  rises` test only asks that each interior beat the one below it, so inserting the
  vault at 77 between sector 14 and the roof satisfied every check while squeezing
  the finale's step from eleven down to two. The rule held and the intent behind it
  did not.
  **The last number is the largest step in the run on purpose**, and getting it
  back cost the finale a heavy and three charges: the man holding the helicopter
  pad is now the kind a boot bounces off, which is the sector taking away the free
  answer on the last tiles of the campaign, and the service run beneath the deck
  carries three more mines along its length. 89 against the vault's 77 is a step of
  twelve, one clear of sector 9's nine. Left at 82 the sector the whole night
  climbs toward played a shade easier than the floor below it.
  **And the height half of the rule was held by a proxy, which is the same
  failure one clause to the left.** The check measured `map.height` — the row
  count of the file — where the rule says a *climb's* height. The two agree only
  while the spawn sits on the bottom row, and sector 13's did not: two blank
  rows under its `S` made its map 48 against sector 11's 46, so the assertion
  passed while both walls were 45 tiles from spawn to window. The player is
  shown that number, and the facade HUD read `WINDOW 45M` on the third climb and
  again on the fourth. The `S` moved to the bottom row — the campaign runs 39,
  42, 45, 48, 51 now — and the test computes the rise the way `draw_facade_hud`
  computes it, so the assertion and the readout cannot come to disagree. Nothing
  in the maps was wrong; what was wrong was that the thing being measured was
  not the thing being ruled on.
  **And then the condition that fix rests on went unheld for a release, on one
  wall.** "The `S` moved to the bottom row" was the repair and it was never made
  a rule, so nothing stopped a climb keeping a blank row under its spawn — and
  sector 7 still had one, the only wall of the five that did, which made
  `map.height` rise + 2 there and rise + 1 everywhere else. Nothing was broken by
  it: the rise was right, the order was right, and the camera clamp never showed
  the row. What was wrong is that the campaign carried one wall where the cheap
  reading and the true one disagreed, on the exact pair of quantities that had
  already been confused once — so the next reader of `map.height` would have been
  right about four walls. **A repair that a check depends on is a rule, and wants
  holding like one.** The spawn is on the bottom row of every climb now (heights
  40, 43, 46, 49, 52 against rises 39, 42, 45, 48, 51) and the same test asserts
  it, so a spare row arrives as a failure rather than as a proxy quietly going
  stale again.
  **And that move landed the fourth spawn off the window grid, which nothing but
  the editor noticed.** A facade's painted windows sit on rows of three and
  columns of four, and an `S`, `Y`, `r` or `v` that misses an intersection covers
  a window instead of replacing one. The rise it was moved to was 47, and 47 is
  the one figure in that sequence not divisible by three — so sector 13 spent a
  release as the only shipped map with a finding against it, the single note in
  all seventeen. Two rules pulling on one tile: the rise has to beat sector 11's
  45 and stay under sector 15's 51, and it has to be a multiple of three, which
  leaves exactly 48. The wall gained a row at its foot and the sequence is
  divisible by three all the way up, which is the property to keep rather than
  the numbers — a fifth climb inserted anywhere in it inherits both rules at
  once.
  **Sector 12 was the same complaint one floor lower**, and clearing the rule is
  not the same as keeping it: it once beat the floor below it by two, the
  smallest step anywhere in the campaign at the time, while dropping from ten
  guards to seven. Fewer men in a crawl duct is the right instinct and the
  arithmetic still has to hold, so what it was short went where the sector
  already speaks — a charge on the approach to the blocked-up bulkhead, so
  opening it with the rocket is not free, and one out on the bottom service run
  between the last guard and the exit ladder. It stands at 65 against sector
  10's 58 now, a step of seven. **The figures in this paragraph used to be
  absolute and are deliberately not any more**: quoted as `55 against 53` they
  went on describing a campaign two edits old, and a reader checking them
  against the maps found neither number. The sequence above is the one place
  the numbers live, and it is measured. Sector 1
  spends its whole
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
- **Land two tiles up, or cross two spikes.** The model refuses both, and the
  two refusals are not the same kind of claim — which matters, because this
  sentence used to say "anything the rules below say is out of reach really is
  out of reach" and one half of that was false.
  The spike is a claim about the *body*: crossing two of them costs a heart, and
  `test_the_route_model_promises_only_moves_the_player_can_make` measures it. The
  two-tile step up is a claim about the *model only*. Measured, the body can do
  it — the apex is 68.7px against 64px of rise, with about 4.7px in hand — so a
  ledge two tiles up is somewhere the player can get and the model will never
  route them. That is the conservative side and it stays: a floor certified
  finishable is finishable. But **do not draw a pocket two tiles up and call it
  unreachable**, because it is not, and nothing will tell you. Measured across
  every shipped sector the wider move opens *nought* extra tiles, so the campaign
  as it stands does not depend on either reading;
  `test_a_two_tile_step_up_is_the_bodys_move_not_the_models` is what keeps the
  paragraph honest if the jump is ever retuned.

## Authoring rules for interiors

A storey is a solid `#` slab with an open band above it. Entities stand in the
bottom row of that band, so a two-row band is a corridor and a three-row band is
a hall. These rules come from the tuning in
[game_config.h](../src/game_config.h) and every campaign map obeys them.

- **Jump reach.** A 365px/s jump under 980px/s² gravity peaks at **68.7px**, and
  that is a measured number rather than the continuous arithmetic's 68: the
  impulse is written into `vy` after the step's gravity, so the launch step
  carries the full launch speed and the apex lands half a step's travel above
  `v0²/2g`. It is the same 68.7 on every machine because the simulation is
  stepped at a fixed `SIM_STEP_DT` — before that it was 71px at 60Hz and 77px on
  a stuttering one, which made a ceiling placed to cap a jump clearable on a
  slow machine and not on a quick one. `test_the_jump_apex_does_not_depend_on_the_frame_rate`
  is what holds it there, so an author may draw against this figure.
  In a two-row band the ceiling caps the rise at one tile and the player covers
  about 48px of ground; a second open row buys the full apex and about 87px.
  **What that is worth in tiles is one more than it reads**, and the correction
  is the player's own box: a hole is landed *past* rather than cleared, and
  `level_move` holds a body up while any part of it is over solid tile, so he
  leaves the near lip with his left edge on it and lands the moment his right
  edge reaches the far one. The ground a hole asks for is `width - PLAYER_W` —
  38px for two tiles, 70px for three. So **a two-row corridor jumps a two-tile
  hole and a three-row hall jumps a three-tile one**; four tiles is where a
  ladder, a lift shaft or a moving platform becomes the answer. This bullet
  said one and two for as long as it existed, and the spike bullet below it has
  had the same 26px right the whole time — because a spike *does* have to be
  cleared entirely, so there the box is added rather than subtracted. Two
  adjacent rules about one body, and only one of them had ever been measured
  against the simulation;
  `test_a_jump_clears_a_wider_hole_than_the_model_will_route` measures both
  widths in both directions now.
  The route model refuses a tile earlier in each band, which is the same
  conservative gap the two-tile step up has and the same reason it stays. But
  **do not draw a two-tile gap in a corridor and call it a barrier**, because
  it is not one, and nothing will tell you. Measured across the twelve
  interiors nothing rests on the old reading: seventy-three one-tile gaps, two
  two-tile gaps that both have three open rows, and four six-tile ones that are
  storey breaks.
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
  sectors 2, 4, 12 and 17.
  **This list said 15 for a while, and 15 is a facade** — a climb has no slab to
  cut a panel into, so the claim was not merely stale but impossible, in the same
  edit that turned the old sector 15 into a wall and moved the last interior to
  17. It is derived from the maps by
  [tools/check_docs.py](../tools/check_docs.py) now, which is what
  `docs/gameplay.md`'s sector lists already had and this page did not.
  **And every rule above is about the fallen state of the *map*, which left the
  player out.** Nothing asked what is *under* a panel, and two of the five
  panelled floors were paying for it. The archive's pair was an **outright
  death**: a man standing on a panel has his feet on its top edge, so the drop
  is measured from the row *above* it — 192px into the hall below against a
  `PLAYER_FATAL_FALL_HEIGHT` of 160, arriving at 592px/s against a fatal 560.
  Three hearts, which is the life rather than a hit, on a dead-end mined pocket
  whose only other exit was walking back out. Every column of that slab drops
  the same 192px, so there was no better place for it and those two tiles are
  masonry now, which is why the list above is one shorter than it was.
  **The route model already knew** — the fall it implies is one
  `route_survivable_fall` returns false for, so the model refused the hole and
  certified the floor by another path while the player could walk straight into
  it: *a model that refuses a move is not a warning to whoever drew it.*
  The roof's panel dropped through a ceiling fan instead, one heart every time
  the mechanic was used, which `check_fans` cannot see because all five of its
  questions look **down** from the blades — a hole in the slab directly above
  one is the half of that pair nothing asked about. The fan moved two columns,
  which is what clears `CEILING_FAN_BLADE_LENGTH` of the falling player's box.
  `check_panels` in [editor_validate.c](../editor/editor_validate.c) asks all
  three now: a drop the player cannot survive as an **error**, because that is a
  floor that can eat a run rather than one that plays differently from how it
  reads, and blades or a spike bed on the way down as a warning, because those
  cost a heart and an author may well mean it.
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
- **Every interior reaches a blast, and the washroom counts.** Ten of the twelve
  lay out an `N` or a `Z`; sectors 1 and 5 do not, and both have a `U`, and every
  washroom hands out a grenade. So the rule is a reachability one rather than a
  list of floors with an exception on the end: **a floor with no explosive on its
  plan has a door to one.** On sector 5 that is what the detour is *for* — see
  its row in the plan table above, and
  `test_every_interior_can_reach_a_blast`, which asks the property so a new
  interior drawn with neither fails on the change rather than on a player.
- **A crate shoved at a dog is a shove, not a doorway.** `move_crate_x` stops for
  a man and not for an animal, on purpose, because a dog that stopped a box would
  stop the shove the mechanic is played for. What that means for a plan is that
  the animal is put out from under the crate, on the far side where there is room
  and behind it where there is not — so a `B` and a `W` on the same run against a
  wall is a legal arrangement rather than a trap. It was not always: until
  `eject_from_crate` asked the building anything, that arrangement put the dog
  inside the masonry and then a tile beyond the map, permanently. See
  `test_a_crate_never_shoves_a_body_into_the_building`.
- **`P` platforms** patrol the contiguous non-solid run of their row, bounded by
  `#`, `D` and `V`. Keep ladders and other gaps out of that row or the platform
  will wander further than the void it is meant to bridge.
  **And keep `B` out of that run.** A crate now rests on a platform the way it
  rests on masonry, which is the fix for a box falling through the one thing on
  a floor that is drawn as floor — but the platform's own pass carries nobody,
  so it slides out from under a crate and drops it, and one travelling into a
  crate passes through it sideways. Neither is reachable by pushing on any
  shipped map and `test_a_crate_rests_on_what_the_player_does` is what keeps it
  that way; the rule is here so an author does not have to discover it. A lift
  shaft `V` refuses a crate outright for the opposite reason — a deck travels
  *up* into things, so there is no position inside a shaft that would be safe to
  allow. See `crate_position_clear` in
  [gameplay_physics.c](../src/gameplay_physics.c) for why the three surfaces get
  three different answers.

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
- **Keep the lowest `r` or `v` six rows above `S`.** A source wakes on
  *vertical* distance alone (`FACADE_HAZARD_WAKE_RANGE`, ten tiles), and its
  first delay is only a quarter to a little over half of the ordinary one, so a
  bird three rows up can be in the air 0.6s after the player arrives. A thrown
  object shouts and leans out for `THROWN_OBJECT_WINDUP` first; **a bird does
  not announce itself at all**, which makes it the one hazard on the wall that
  cannot be answered by having seen it coming, and therefore the one that must
  not be within reach of a climber who has not moved yet.
  Sector 15's lowest `v` was three rows above the spawn and four columns
  across, and it cost a heart 1.25s into the climb on 94 of 256 seeds with
  nothing pressed. Sectors 11 and 13 keep theirs six rows up and are clean at
  256; 15 is six rows up now too. This is the facade half of the interiors'
  "a sector has to give the player a moment to read it", which is
  `SPAWN_GRACE_SECONDS` in
  [tests/test_main.c](../tests/test_main.c) — and the rule was checked by a
  loop that counted to seventeen while asking twelve, because a wall has no men
  on it and the stepper drove the interior frame. Both modes are driven in their
  own order now.
  **And then it was measured a source at a time, which says the rule is exactly
  right and was about the wrong hazard.** One `r` or one `v` at a time on a real
  wall's geometry, 128 seeds of thirty seconds with nothing pressed: a **bird**
  costs nothing from six rows up, and at three rows up it is worth 79 to 106
  seeds of 128 anywhere within four columns of the spawn. So this clause is a
  bird rule, it is a good one, and the number in it is the number the
  measurement gives.
  **The clause used to close "at *any* column", and that half was a reading
  rather than a rule.** 128 seeds cannot see a one-in-three-thousand event, and
  re-measured at 4096 seeds across four independent seed families sector 13 —
  six rows up and four columns across, obeying every word above — cost a
  passive climber a heart at 8.06s, inside the 11.05s window
  `test_a_climb_does_not_bite_before_its_first_gust_blows_out` gives a wall.
  The mechanism is the one written in the bullet below for the thrower,
  arriving at the hazard that bullet exempts: the first gust carries a still climber
  five to six tiles sideways, out from under the cornice that was sheltering
  him, and a bird descending on his column has no windup to answer. So the
  column matters for a `v` too — what it has to clear is not the spawn but the
  band the drift can reach — and how far that is depends on which cornice is
  over it, which is why the rule stays behavioural. Sector 13's lowest `v`
  moved four columns and is nought at 4096 on all four families; the wall's
  thirty-second profile is unchanged, which is what says it was a grace fix and
  not a difficulty one. The check now samples 2048 seeds rather than 64, which
  is the order of magnitude the rate is, and it is still a sample.
  It never covered the thrower. A thrown object is aimed, so it stays dangerous
  at **six and nine rows up as well**, and what decides whether it lands is how
  many columns lie between the source and the spawn — with a cornice in between
  worth more than either. Sectors 3 and 7 were both sitting at three rows and
  both measured clean, which is what a rule written on one axis buys you: they
  were saved by their cornices rather than by their placement, and nobody had
  looked. Both are six rows up now and it bought nought at 512 seeds of sixty
  seconds, which is worth writing down rather than dressing up — the maps obey
  the campaign's own rule now, and that is the whole of what changed.
- **And keep an `r` out of the spawn's own column**, which is the clause that
  was missing and the one that was costing a run. Sector 7's sat directly above
  `S`, six rows up and therefore obeying every word written above it, and it
  took a heart off a climber who pressed nothing at **7.18s on 45 of 256
  seeds** — the second wall of the campaign, and the harshest in it: 13 and 15
  both wait about fifteen seconds and 3 and 11 never bite at all, so the curve
  ran backwards for three climbs with every gate green. Moving that one
  character eight columns across takes it to nought at 256.
  The mechanism is the one thing on this wall that moves a player who is not
  moving. A gust carries a passive climber three to five tiles sideways; a
  thrown object's horizontal reach is `THROWN_OBJECT_SPEED` over the 1.35s its
  travel time is clamped to, about 8.6 tiles, so a source the drift walks him
  under is in range and one four columns further out is not.
  **The check is behavioural rather than positional, and that is deliberate**:
  how far is far enough depends on which cornice is between the two, which is a
  fact about the floor plan and not about the tile — the same reason the weak
  wall's rule and the docket sheet's live in the route model rather than in the
  grid parser. `test_a_climb_does_not_bite_before_its_first_gust_blows_out`
  gives a wall its own window instead of the floor's three seconds, derived
  from the wind cycle rather than picked: `FACADE_WIND_CALM_MAX` plus the
  warning plus the gust, 11.05s, by which point the wall has shown the player
  the one thing it does on every seed rather than on the lucky ones. Measured at
  15s the figures are identical, so the boundary is the map's and not the
  constant's.
