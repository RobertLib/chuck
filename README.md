# Chuck

Chuck is a 2D action platformer about pursuing the kidnappers who took Chuck's
fiancée and bringing her home alive.

The player explores a side-scrolling building made of platforms, ladders,
doors, elevators, hazards, and pickups. Each level is a small infiltration
route through the kidnappers' stronghold: follow their trail, find the key card
that opens the way deeper inside, and decide when to fight, when to sneak past,
and when to use the environment to your advantage.

## Story

Chuck arrives at the building just as a group of kidnappers drags his fiancée
inside. He tries to stop them, but cannot take a clear shot without putting her
in danger, so he follows them into the building.

The kidnappers keep moving deeper into the complex while Chuck fights and
sneaks his way through its guarded sectors. Each level brings him closer to
rescuing his fiancée, but every route is locked down: he must search the area,
identify the correct access key card, and reach the exit before he can continue
the pursuit.

The mission is simple: follow the kidnappers through every level, overcome the
building's security, and bring his fiancée home.

## Gameplay

Your goal is to keep the kidnappers in reach. In each level, survive the guards,
collect the right key card, and pass through the secured exit before the trail
goes cold. Guards patrol the building, climb between floors, and shoot when
they spot you. They can be eliminated with weapons, avoided by taking another
route, or bypassed by crawling, timing movement, and using cover.

The game rewards careful movement as much as combat. Ammunition is limited,
grenades are powerful but risky, and hazards such as mines, spikes, falling
platforms, and moving platforms make the building itself part of the challenge.
Some doors connect distant parts of a level, creating shortcuts, ambushes, or
escape routes depending on how you use them.

## Controls

- Move: arrow keys or `WASD`
- Jump: `Up` or `W` when on the ground
- Climb: `Up` / `Down` or `W` / `S` on ladders
- Crawl: hold `Down` or `S` while on the ground
- Shoot or throw a grenade: `Space`
- Use a door: `Down` or `S`
- Mute or unmute sound and music: `M`
- Toggle fullscreen: `F` or `Alt+Enter`
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

To remove build outputs:

```sh
make clean
```

SDL3 development files must be available through `pkg-config`.

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
