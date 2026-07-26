# Chuck

Chuck is a 2D action platformer about pursuing the kidnappers who took Chuck's
fiancée and bringing her home alive.

The game opens with a top-down night car chase: Chuck tails the kidnappers' SUV
through city traffic until it reaches the building where they take her. From
there the game becomes a platformer.

The player explores a side-scrolling building made of platforms, ladders,
doors, elevators, alarm switches, hazards, and pickups. Each level is a small
infiltration route through the kidnappers' stronghold: follow their trail, find the key card
or breach the security terminal that opens the way deeper inside, and decide
when to fight, when to sneak past, and when to use the environment to your
advantage.

Ambient janitors patrol some corridors with cleaning carts, pause to mop, and
leave short-lived wet streaks. They are visual-only NPCs: attacks, collisions,
alarms, pickups, and scoring ignore them completely.

## Story

Kidnappers take Chuck's fiancée off the street and drive away with her. Chuck
gets to his own car seconds later and follows them across the city, and they lead
him to the building they are holding her in.

He arrives just as they drag her inside. He tries to stop them, but cannot take a
clear shot without putting her in danger, so he follows them into the building.

The kidnappers keep moving deeper into the complex while Chuck fights and
sneaks his way through its guarded sectors. Each level brings him closer to
rescuing his fiancée, but every route is locked down: he must search the area,
identify the correct access key card, and reach the exit before he can continue
the pursuit.

The mission is simple: follow the kidnappers through every level, overcome the
building's security, and bring his fiancée home. The pursuit ends in a
cinematic rooftop rescue after the final sector.

## The pursuit drive

The prologue is played from above, driving forward along a four-lane city street.
Only two things matter: stay on the SUV, and keep the car in one piece. Slower
traffic runs with you, oncoming traffic comes down the two left-hand lanes with
its headlights in your face, and every block ends at a signalled junction where
cross traffic cuts straight across your path — the lights and the painted warning
on the road tell you whether it is about to. The readout shows how far ahead the
SUV is; clipping another car, or scraping a kerb, costs the speed you need to
stay with it. Three crashes wreck the car, and letting the SUV get too far ahead
loses the trail, but neither ends the game: a failed attempt only restarts the
drive, so the story always continues. Reaching the building starts the campaign.

## Gameplay

Your goal is to keep the kidnappers in reach. In each level, survive the guards,
collect the right key card, and pass through the secured exit before the trail
goes cold. Guards patrol the building, climb between floors, and react to first
contact by attacking or running for one of the wall-mounted alarm switches.
They see through a forward vision cone that walls, floors, and crates block, so
they can spot Chuck diagonally or up a ladder shaft and fire straight up or down
that shaft; a crawling Chuck is noticed only from closer in. Guards also have
ears and memory: gunfire and explosions draw nearby guards over to investigate,
a guard who finds a fallen comrade often sprints for the alarm, and a suspicious
guard walks to the disturbance, scans, and returns to its patrol if the coast is
clear. A triggered alarm bathes the sector in red warning light and mobilizes
every guard and dog. Guards move, aim, and fire faster, lead a running target,
hop small gaps to keep up, and fan out to sweep around Chuck's last known
position; the alarm shuts itself off after he stays out of sight long enough.
Guards can be eliminated with weapons, avoided by taking another route, or
bypassed by crawling, timing movement, and using cover.

The game rewards careful movement as much as combat. Each sector contains
several terminals, but only the brightly lit active terminal can be hacked.
Holding position while breaching it takes several seconds and leaves Chuck
exposed; the noise alerts guards and dogs, who pursue Chuck during the hack
and for a short time afterward. It can also bring one or two reinforcements
through randomly selected doors after varied delays. This makes breaching a
risky alternative to finding the correct key card.
The active terminal is always selected away from the level entrance.
Ammunition is limited; once the pistol is empty, the attack button switches to
a short-range knife strike. A one-shot bazooka appears in every second level;
its rocket detonates on impact and can clear a tightly grouped patrol. Grenades
are powerful but risky, and hazards such as
mines, spikes, ceiling fans, falling platforms, and moving platforms make the
building itself part of the challenge. Some doors connect distant parts of a level,
creating shortcuts, ambushes, or escape routes depending on how you use them.
Small gas canisters can also be used against nearby guards, but their low profile
means Chuck must crawl before firing to hit one.

The first sector also contains a marked WC door. It enters a small, separate
restroom sublevel with useful supplies. The parent level is paused while Chuck
is inside, and both areas retain their state when he leaves or returns; this WC
entrance is independent of the paired shortcut-door system.

## Controls

### Gamepad

Gamepads can be connected or disconnected while the game is running. Button
names below use the Xbox layout; the same physical face-button positions are
used on PlayStation and Nintendo controllers.

- Move, climb, crawl, steer, accelerate, and brake: left stick or D-pad
- Jump, start, continue, skip a cinematic, or replay after the ending: `A`
  (south face button)
- Launch a rocket, throw a grenade, shoot, or use the knife: `X` (west face
  button)
- Use a door or hold to hack the active terminal: `Y` (north face button)
- Return to the title screen: `B` (east face button) or `Back`
- Quit from the title screen: `B` or `Back`
- Mute or unmute sound and music: left shoulder
- Toggle fullscreen: right shoulder
- `Start` also starts, continues, skips, or replays wherever `A` does

### Pursuit drive

- Steer: `Left` / `Right` or `A` / `D`
- Accelerate: `Up` or `W`
- Brake: `Down` or `S`
- Skip the opening beat: `Space` or `Enter`

### Platformer

- Move: arrow keys or `WASD`
- Jump: `Up` or `W` when on the ground
- Climb: `Up` / `Down` or `W` / `S` on ladders
- Shoot vertically from a ladder: hold `Up` / `W` or `Down` / `S` and press `Space`
- Use the knife sideways from a ladder: press `Space` without holding `Up` or `Down`
- Crawl: hold `Down` or `S` while on the ground
- Launch a carried rocket, throw a grenade, shoot, or use the knife: `Space`
- Hack the active terminal: hold `E` while standing nearby
- Use a door: press `E` while standing in the doorway
- Mute or unmute sound and music: `M`
- Toggle fullscreen: `F` or `Alt+Enter`
- Skip a cinematic to its next screen: `Space` or `Enter`
- Restart after win or game over: `R`

## Running

Chuck is written in C and uses SDL3.

Build and run it with:

```sh
make run
```

To build without starting the game:

```sh
make
```

Run the deterministic core test suite with:

```sh
make test
```

Build the complete game and run the core tests with AddressSanitizer and
UndefinedBehaviorSanitizer enabled:

```sh
make sanitize
```

To remove build outputs:

```sh
make clean
```

SDL3 development files must be available through `pkg-config`.

Campaign maps remain editable as `levels/level*.txt`, while separate interiors
live under `levels/sublevels/`. During every build they are converted into C
arrays and linked into the `chuck` executable. The executable therefore does
not need the `levels` directory (or any other runtime asset files) when it is
distributed.

## Architecture

The SDL-facing application shell lives in `game.c`, `game_input.c`,
`game_render.c`, and `chase_render.c`. `Game` composes four explicit areas of
state: platform resources, campaign progress, current gameplay simulation, and
presentation. Scene changes go through one state-transition function and loading
a level uses one simulation reset path.

Gameplay rules are split by responsibility under `src/gameplay_*.c`: AI,
combat, interactions, physics, and shared world operations. The prologue drive
is a separate simulation of its own in `chase.c`. These modules do not depend on
SDL or on the top-level `Game`, which keeps them deterministic and directly
testable. They report sounds, particles, and camera shake through
a small event buffer that the application shell translates into presentation
effects.

`Level` separates immutable parsed map data (`LevelMap`), mutable per-run data
(`LevelRuntime`), and reveal-animation state (`LevelReveal`). Random gameplay
choices use an explicitly seeded `Rng`; the SDL random generator is reserved
for visual effects. The core tests cover seeded behavior, parsing every
embedded level, collision, resets, interactions, AI spawning, combat feedback,
and the prologue drive from its opening beat to the arrival at the building.

## Assets

All visuals are generated procedurally at runtime with SDL draw calls. Sound
effects use a matching lo-fi arcade synthesizer: every effect is generated
once during audio startup, cached as PCM in memory, and replayed through a
16-voice pool. The title screen and each level have their own procedurally
composed, seamless industrial scores. The longer level arrangements move
through sparse, full and breakdown sections before repeating. Music
automatically ducks under busy action so gameplay cues stay readable.
Repeated playback never reruns the synthesis. The game uses no external asset
files.

## Project Direction

The project is intentionally small and direct: a classic pixel-art platformer
with readable rules, compact levels, and arcade-style tension. New levels,
hazards, pickups, and enemy behaviors should keep the same core loop intact:
follow the trail, read the situation, breach the locked route, and close the
distance to Chuck's fiancée.
