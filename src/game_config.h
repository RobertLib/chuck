#ifndef CHUCK_GAME_CONFIG_H
#define CHUCK_GAME_CONFIG_H

/* Tile / world geometry */
#define TILE_SIZE 32
#define HUD_HEIGHT 40

/*
 * The longest step the simulation is ever asked to take. `SDL_AppIterate`
 * clamps the frame to this before anything sees it, which is what keeps a
 * dropped frame or a dragged window from teleporting the world.
 *
 * It is also load-bearing for collision. A projectile tests the tile under its
 * leading edge *after* it has moved rather than sweeping the path it crossed,
 * so it can only be trusted while one step is shorter than one tile — the
 * static assertions beside the projectile speeds below are what enforce that,
 * and they turn "raise BULLET_SPEED and shots quietly start passing through
 * one-tile walls" into a build failure.
 *
 * Written as a whole number of steps per second rather than as a decimal, so
 * those assertions can be integer constant expressions: a float comparison in
 * a _Static_assert is a GNU extension and this tree is built -Wpedantic.
 */
#define MIN_FRAME_RATE 20
#define MAX_FRAME_DT (1.0f / (float)MIN_FRAME_RATE)

/*
 * The step the simulation actually takes, and why it is not the frame.
 *
 * Feeding the real frame time straight to `game_update` made every number the
 * physics produces a property of the display it was drawn on, because the jump
 * impulse is written into `vy` *after* that frame's gravity (see
 * `player_update`): the launch step therefore carried the undecayed
 * `PLAYER_JUMP_SPEED` and the apex came out at `v0^2/2g + v0*dt/2`. Measured,
 * that is 68.7px at 240Hz, 71.0px at 60Hz and 77.4px at the MIN_FRAME_RATE
 * floor — nearly a third of a tile of difference between a fast machine and a
 * stuttering one, in the one quantity every map is drawn against. A ceiling
 * placed to cap a jump could be cleared on a slow machine and not on a quick
 * one, which is the only thing in this tree that ever answered differently
 * depending on the hardware under it.
 *
 * So `SDL_AppIterate` accumulates real time and spends it in steps of exactly
 * this length. The apex is now one number everywhere — 68.7px, which is also
 * the closest the discrete integrator gets to the 68px the level legend has
 * always quoted from the continuous arithmetic.
 *
 * 240 rather than 60 for two reasons. It divides every common refresh rate the
 * game is likely to meet, so a 60, 120 or 240Hz display spends a whole number
 * of steps per frame and nothing judders; and where it does not divide (144Hz)
 * the un-rendered remainder is at most one step of travel, which at walking
 * speed is half a pixel. Raising it costs update time and buys nothing;
 * lowering it puts visible judder back on high-refresh displays.
 */
#define SIM_STEPS_PER_SECOND 240
#define SIM_STEP_DT (1.0f / (float)SIM_STEPS_PER_SECOND)

/*
 * The two collision proofs below are written against MAX_FRAME_DT, and they
 * stay written against it: MIN_FRAME_RATE is still the longest step anything
 * downstream may see, and this assertion is what keeps the fixed step inside
 * that promise rather than becoming a second, unproved one.
 */
_Static_assert(SIM_STEPS_PER_SECOND >= MIN_FRAME_RATE,
               "the simulation step must be no longer than MAX_FRAME_DT");

/* Ignore the loose center of an analogue stick so a resting gamepad cannot
 * make Chuck or his car creep. D-pad input bypasses this threshold. */
#define GAMEPAD_AXIS_DEAD_ZONE 8000

#define MAX_LEVEL_WIDTH 128
/*
 * 56 rather than the 48 it stood at for four climbs.
 *
 * The five facades run 40, 44, 46, 48 and 52 rows and each has to be taller
 * than the last, so level 13 was standing on the old cap: a fifth climb was
 * never a map away, it was this number away. Raising it is a memory decision as
 * much as an authoring one — `LevelMap` holds the grid inline and `Game` holds
 * two of them (the sector and the paused restroom) — and it comes to about
 * eight kilobytes across both, which is nothing against what it buys. There is
 * room for one more climb above 52 before this number is the constraint again.
 */
#define MAX_LEVEL_HEIGHT 56
#define MAX_ITEMS 128
/*
 * The guard ceiling is a seating limit, not a design statement, and it has to
 * cover the men a floor can *call* as well as the ones drawn on it. A terminal
 * hacked under the alarm sends for up to `TERMINAL_REINFORCEMENT_MAX_COUNT`
 * out of a door, once per console, and sector 14 is twelve men with three
 * consoles and two doors — eighteen, against a ceiling of sixteen. What that
 * cost was not a crash: `find_enemy_slot` hands the furthest corpse to the
 * arrival instead, so the floor quietly deleted a body in front of the player
 * and with it the thing `update_body_discovery` sends the next calm guard to
 * look at. The one rule the quiet route rests on switched itself off on the
 * busiest floor that has doors.
 *
 * `test_every_sector_can_seat_the_reinforcements_it_can_call` derives the
 * requirement from the maps rather than trusting this number, so a fourth
 * console on a floor fails the build instead of eating a corpse.
 *
 * Sixteen to twenty-four is 1408 bytes of `GameplayState` and `Game` holds two
 * of them, so under three kilobytes for the whole change — the same trade
 * `MAX_LEVEL_HEIGHT` above makes, and worth as little argument. The ceiling on
 * it is `enemy_body_bit`: one bit per corpse in `Enemy.bodies_investigated`,
 * asserted in [enemy.h](enemy.h), which is why that field is 64 bits wide.
 */
#define MAX_ENEMIES 24
#define MAX_DOGS 12
#define ITEM_RESPAWN_TIME 10.0f

#define MAX_DECORATIONS 192

/* Ambient janitors are visual-only NPCs. They collide with the static level
 * geometry so their patrols stay grounded, but never participate in combat,
 * player collision, pickups, alarms, or scoring. */
#define MAX_JANITORS 8
#define JANITOR_W 26
#define JANITOR_H 32
#define JANITOR_CART_SIDE_EXTENT 26.0f
#define JANITOR_WALK_SPEED 34.0f
#define JANITOR_WET_SPOTS 6
#define JANITOR_WET_LIFETIME 7.0f

/* Fleeing civilians are the same kind of visual-only NPC, but they play once:
 * the moment a level starts they bolt for the way the player came in and are
 * gone. Nothing in the simulation can see them, and they can neither block
 * anything nor be hurt by it, so the evacuation is staging and never a rule.
 * The startle beat is staggered per person so the room empties in ones and
 * twos rather than as one drilled column. */
#define MAX_CIVILIANS 8
#define CIVILIAN_W 24
#define CIVILIAN_H 32
#define CIVILIAN_VARIANTS 3
#define CIVILIAN_RUN_SPEED 118.0f
#define CIVILIAN_RUN_SPEED_SPREAD 46.0f
#define CIVILIAN_STARTLE_MIN 0.12f
#define CIVILIAN_STARTLE_SPREAD 2.40f
/* One trip each at most: a panic that keeps falling over reads as a bug. */
#define CIVILIAN_STUMBLE_CHANCE 45
#define CIVILIAN_STUMBLE_DELAY_MIN 0.30f
#define CIVILIAN_STUMBLE_DELAY_SPREAD 1.10f
#define CIVILIAN_STUMBLE_TIME 0.62f
/* They dissolve into the doorway instead of popping out of the room, and are
 * counted out for good just short of the tile the player entered on. */
#define CIVILIAN_FADE_DISTANCE 130.0f
#define CIVILIAN_EXIT_REACH 60.0f
#define CIVILIAN_HOP_SPEED 250.0f
#define CIVILIAN_STUCK_TIME 1.20f
#define CIVILIAN_STUCK_FADE_TIME 0.45f

/* The receptionist is the third visual-only NPC, and the only one with a place
 * to be: the post at the front counter is left for an errand and always
 * returned to, so the desk still reads as staffed however long the player
 * spends in the lobby. Like the other two this is scenery — no perception, no
 * collision against the player, nothing that can hit it. */
#define MAX_RECEPTIONISTS 4
#define RECEPTIONIST_W 24
#define RECEPTIONIST_H 32
#define RECEPTIONIST_WALK_SPEED 44.0f
/* Long enough on the desk that leaving it is an event rather than a pacing. */
#define RECEPTIONIST_DESK_TIME_MIN 7.0f
#define RECEPTIONIST_DESK_TIME_SPREAD 8.0f
#define RECEPTIONIST_ERRAND_TIME_MIN 2.6f
#define RECEPTIONIST_ERRAND_TIME_SPREAD 3.4f
/* Errands stay inside the room the counter is in; the walk out is measured
 * from the post so a round trip always lands back on the same tile. */
#define RECEPTIONIST_ERRAND_MIN_REACH (2.0f * TILE_SIZE)
#define RECEPTIONIST_ERRAND_REACH_SPREAD (3.0f * TILE_SIZE)
/* How far ahead a side is probed for walkable floor when deciding which way
 * the counter faces and which way an errand can go. */
#define RECEPTIONIST_OPEN_RUN_PROBE 6
/* On post the floor is watched and the work behind the counter is turned to
 * now and then; the glance is what stops the pose reading as a mannequin. */
#define RECEPTIONIST_GLANCE_MIN 2.2f
#define RECEPTIONIST_GLANCE_SPREAD 4.0f
#define RECEPTIONIST_GLANCE_TIME 1.3f

/* Mines */
#define MAX_MINES 32
#define MINE_W 16
#define MINE_H 10
#define MINE_TRIGGER_DELAY 0.45f
/* What a mine costs is not written here: it goes through `apply_blast` like
 * every other explosive and so costs EXPLOSION_DAMAGE, which is the whole point
 * of "one blast, one rule". A MINE_DAMAGE of its own sat here long after
 * nothing read it — a number that reads like a tuning knob and is not one is
 * exactly what the assertion beside MAX_FALL_SPEED exists to prevent. */
#define MINE_RADIUS 36.0f

/* Small floor-level gas canisters. Their low profile deliberately puts them
 * below a standing player's firing line, so Chuck must crawl to shoot them. */
#define MAX_GAS_CANISTERS 64
#define GAS_CANISTER_W 12
#define GAS_CANISTER_H 16
#define GAS_CANISTER_RADIUS 56.0f

/* Pushable/destructible crates */
#define MAX_CRATES 64
#define CRATE_W 28
#define CRATE_H 28
#define CRATE_PUSH_SPEED 95.0f
#define CRATE_FRICTION 8.0f
#define CRATE_LAND_SOUND_SPEED 90.0f
/* Breaking one pays the smallest number in the game on purpose: a crate is
 * cover and a step up, so shooting the floor's furniture away has to read as
 * spending ammunition rather than as farming it. */
#define CRATE_SCORE 20

/* Weak walls: a blocked-up opening that only an explosion reopens. Nothing
 * smaller than a blast touches one, so a route through a wall always costs an
 * explosive — which is what keeps it a shortcut rather than a corridor. The
 * hole is permanent for the rest of the run, like a fallen panel. */
#define WEAK_WALL_DUST 9
#define WEAK_WALL_SCORE 25
/*
 * What a sheet of the docket pays.
 *
 * Above a key card's `CARD_SCORE` and well below a sector's par bonus, which
 * is the band it belongs in: taking the detour has to be worth doing on a run
 * that is only chasing points, and it must not be worth *more* than clearing
 * the floor quickly — the collectable is a second reason to explore, not a
 * replacement for the first one.
 */
#define EVIDENCE_SCORE 200
/* And the card the sentence above compares itself to, which was a bare 100 in
 * `gameplay_collect_items` while the argument for the number sat up here. A
 * comparison between two values is a third place either of them is written
 * down; see `ENEMY_SCORE`. */
#define CARD_SCORE 100

/* Exit-access terminals */
#define MAX_TERMINALS 16
#define TERMINAL_HACK_TIME 4.0f
/* A finished hack pays what a takedown does, and the two are unrelated numbers
 * that happen to agree: four seconds stood still in the open is the same order
 * of risk as getting behind a man with a blade. Written separately so that
 * tuning one does not silently move the other. */
#define TERMINAL_SCORE 250
#define TERMINAL_INTERACT_RANGE 44.0f
#define TERMINAL_MIN_START_TILES 12
#define TERMINAL_REINFORCEMENT_MIN_COUNT 1
#define TERMINAL_REINFORCEMENT_MAX_COUNT 2
#define TERMINAL_REINFORCEMENT_FIRST_MIN 0.65f
#define TERMINAL_REINFORCEMENT_FIRST_MAX 2.40f
#define TERMINAL_REINFORCEMENT_GAP_MIN 1.40f
#define TERMINAL_REINFORCEMENT_GAP_MAX 3.50f

/* Building-wide security alarm. Guards that choose not to engage immediately
 * run to one of these wall switches. The countdown is refreshed whenever a
 * guard or dog can still see Chuck, so it only expires after the scene has
 * actually been quiet for a while. */
/*
 * Ceiling cameras, and the one hazard in the building that cannot be talked to.
 *
 * Every other thing that notices Chuck can be worked on. A guard has a facing,
 * so there is a side of him to be on; he has ears, so a bolt moves him; he can
 * be taken from behind, and the body can be carried away. A camera has none of
 * that — no back, no ears, and crawling under it does nothing, because it is
 * looking down at the floor the crawl is on. What it has instead is a sweep,
 * which is a *clock*: it is pointing somewhere else half the time, and the
 * answer to it is to be somewhere else when it is not.
 *
 * That is the whole reason it earns a place beside the three quiet mechanics
 * rather than duplicating them. It is the obstacle they do not solve, and the
 * sector plans that carry one are the ones where the player has just been
 * taught that everything can be solved.
 *
 * `CAMERA_NOTICE_TIME` is what makes the sweep readable: crossing the beam is
 * survivable, standing in it is not, and the lens flushes to the danger red for
 * the whole of that beat so the frame says which of the two is happening. It is
 * deliberately longer than a guard's `ENEMY_NOTICE_TIME` — a man who spots you
 * shoots you, and a camera only ever tells everybody else.
 */
#define MAX_CAMERAS 8
#define CAMERA_W 14
#define CAMERA_H 10
#define CAMERA_RANGE (5.5f * TILE_SIZE)
/* Half-width of the beam, in radians: a 60-degree cone. */
#define CAMERA_CONE_HALF_ANGLE 0.52f
/* How far either side of straight down the mounting sweeps, and how long one
 * full pass takes. A sweep that reached the horizontal would look through the
 * wall it is bolted to. */
#define CAMERA_SWEEP_ARC 0.85f
#define CAMERA_SWEEP_PERIOD 5.2f
#define CAMERA_NOTICE_TIME 0.85f
/* How long the lens keeps flashing after it has lost him, so the player can see
 * that it *had* him even when they got clear in time. */
#define CAMERA_SUSPICION_FADE 1.1f
/* What a camera contributes to a sector's hazard budget. The same weight as a
 * dog or a mine: it is a second thing that can raise the alarm, and unlike the
 * guard who runs for a switch it cannot be reached before it does. */
#define CAMERA_HAZARD_WEIGHT 2
/* What taking one down pays. Under a guard's 150 on purpose: this is furniture,
 * and a scoring route that ran on shooting fittings would be a worse game than
 * one that ran on the men. Level with a broken crate's 20 would say the
 * opposite — that it was not worth the round. */
#define CAMERA_SCORE 60

#define MAX_ALARM_SWITCHES 16
#define ALARM_CALM_TIME 9.0f
#define ALARM_SWITCH_USE_TIME 0.65f
#define ALARM_SWITCH_USE_RANGE 18.0f
#define ALARM_SWITCH_STAND_DISTANCE 14.0f
#define ALARM_SIREN_INTERVAL 1.15f
#define GUARD_ALARM_CHANCE 45
#define GUARD_ENCOUNTER_RESET_TIME 2.5f

/* Grenades */
#define MAX_GRENADES 8
#define GRENADE_W 10
#define GRENADE_H 10
/*
 * The underarm throw's arc, shared by the grenade, the flash charge and the bolt.
 *
 * `THROW_ARC_STRENGTH` is the distance the lob is solved for: the rise is
 * `strength * GRAVITY / (2 * |vx|)`, which is the speed needed to be coming back
 * down about that many pixels along. The two clamps keep a slow lob from stalling
 * overhead and a fast one from being flat enough to land at Chuck's own feet.
 *
 * Named because the formula used to be written out three times, once per
 * throwable, and only one of the three could ever have been found by somebody
 * tuning it. See `throw_arc_speed` in gameplay_combat.c.
 */
#define THROW_ARC_STRENGTH 160.0f
#define THROW_ARC_MIN_RISE 30.0f
#define THROW_ARC_MAX_RISE 220.0f

#define GRENADE_FUSE_TIME 1.4f
#define GRENADE_RADIUS 48.0f
#define GRENADE_THROW_SPEED 260.0f

/*
 * The flash charge, and the one situation nothing else in the game answers.
 *
 * Every quiet mechanic here is about *before*: a bolt moves attention somewhere
 * else, a blade removes one man who never saw you, a dragged body removes the
 * reason the next one looks. All of them stop being available the moment
 * somebody is actually shooting — and at that point the player's whole
 * repertoire is "shoot back" and "survive `ALARM_CALM_TIME`". This is the
 * answer to *after*: it does not move attention, it takes it away for a few
 * seconds, and what those seconds are for is leaving.
 *
 * **It is deliberately not a weapon.** No damage, no wall opened, no charge
 * chained, and it does not raise the alarm on its own. A guard caught by it
 * stops seeing, stops aiming and stops walking for `FLASH_BLIND_TIME`, and a
 * camera in reach forgets what it had; both come back exactly as they were.
 * Nothing about the sector is permanently different afterwards, which is what
 * separates it from the grenade it looks like.
 *
 * `FLASH_RADIUS` is wider than a grenade's blast and narrower than the noise a
 * shot makes: it has to catch the room the player is in, and it must not reach
 * the one next door, or "throw it and walk" would be the answer to every floor.
 * `FLASH_BLIND_TIME` is measured against `ALARM_CALM_TIME` — about a third of
 * it, so a flash buys a way out of one room and never waits the whole alarm
 * out.
 */
#define MAX_FLASHBANGS 4
#define FLASH_W 10
#define FLASH_H 10
#define FLASH_FUSE_TIME 1.1f
#define FLASH_THROW_SPEED 250.0f
#define FLASH_RADIUS (5.0f * TILE_SIZE)
#define FLASH_BLIND_TIME 3.2f
/* What a spent charge pays. It kills nobody, so this is the whole of what the
 * score says about using one — and it is small, because the escape is the
 * reward and paying well for it would make throwing one at an empty room worth
 * doing. */
#define FLASH_SCORE 40

/*
 * Bolts, and the noise they make where they land.
 *
 * The one thing thrown in this game that is not a weapon. Every perception rule
 * a guard has — `ENEMY_INVESTIGATE_TIME`, the walk to a heard disturbance, the
 * scan, the return to patrol — was reachable by the player through exactly one
 * act, which was firing a gun: the disturbance and the man who caused it were
 * always the same place, so the whole investigate branch could only ever be
 * used against the player. A bolt separates the two.
 *
 * **It is never picked up and never runs out.** A count would make it a
 * resource to hoard, and a resource the player is hoarding is one they do not
 * experiment with — which is fatal for the one mechanic in the game that has to
 * be discovered by trying it. The cooldown is what keeps it from being a
 * remote-controlled patrol route: one bolt in the air at a time, roughly a
 * second apart, so leading a guard somewhere is a plan rather than a joystick.
 *
 * `DECOY_NOISE_RADIUS` is deliberately under `ENEMY_HEAR_RADIUS_SHOT`. A shot
 * is louder than a bolt hitting a floor, and it has to stay louder or the
 * quietest option in the game would also be the one that reaches furthest.
 */
#define MAX_DECOYS 4
#define DECOY_W 6
#define DECOY_H 6
#define DECOY_THROW_SPEED 300.0f
/* Written as a whole number of tiles rather than as a decimal, for the same
 * reason MIN_FRAME_RATE is: it lets the assertion beside
 * `ENEMY_HEAR_RADIUS_SHOT` be an integer constant expression. */
#define DECOY_NOISE_TILES 6
#define DECOY_NOISE_RADIUS ((float)DECOY_NOISE_TILES * TILE_SIZE)
#define DECOY_COOLDOWN 0.9f

/* Bazooka: every pickup contains one rocket. The slow, large projectile
 * detonates on the first solid surface or target it reaches. */
#define MAX_ROCKETS 1
#define BAZOOKA_AMMO 1
#define ROCKET_W 16
#define ROCKET_H 6
#define ROCKET_SPEED 460.0f
#define ROCKET_RADIUS 72.0f
#define ROCKET_ACTION_TIME 0.24f

/* Player tuning */
#define PLAYER_W 26
#define PLAYER_H 32
#define PLAYER_WALK_SPEED 135.0f
#define PLAYER_CLIMB_SPEED 100.0f
/*
 * How far into the top rung the box is snapped when the climb is joined from
 * the tile above it. See the note in `player_update`: without it the grab and
 * the release fight each other for ever and the ladder becomes one-way.
 *
 * One pixel would do — `player_over_ladder` samples the box's bottom edge at
 * `y + height - 1` — and this is two, so the grab does not turn on where the
 * arithmetic rounds. It stays well under the 3px the horizontal snap on the
 * same line already moves a player, which nobody has ever seen.
 */
#define LADDER_TOP_GRAB_OVERLAP 2.0f
#define PLAYER_JUMP_SPEED 365.0f
#define PLAYER_LIVES 3
#define PLAYER_CONTINUES 3
#define CONTINUE_COUNTDOWN_TIME 10.0f
#define GAME_OVER_DISPLAY_TIME 3.0f
/*
 * How long B has to be held on the title screen to close the game.
 *
 * The keyboard has ESC and the mouse has the window's close box; a pad in
 * fullscreen has neither, and every letter on that screen is already spoken
 * for — A starts, X and Y open the two sheets, SELECT takes the resume. That
 * leaves B, which is the one button that must never end a session on a press:
 * it is what a thumb reaches for to back out of the manual, and a second
 * reflex press landing on the title screen would close the game. Holding is
 * the same "deliberate second step" the terminal already asks for, and the
 * chip fills while it is held so the rule teaches itself.
 */
#define TITLE_QUIT_HOLD_TIME 1.1f
#define MAX_LIVES 9
/* Hearts within one life. Ordinary contact damage costs hearts; only the
 * physics deaths (a fatal fall, a crushing elevator) skip them, so "what hits
 * you hurts, what crushes you kills" stays one legible rule. */
#define PLAYER_MAX_HP 3
#define PLAYER_ASSIST_MAX_HP 5
/* Mercy window and the pop away from whatever connected. Knockback is only
 * vertical because the walk speed is rewritten from input every frame. */
#define PLAYER_HIT_INVULN 1.2f
#define PLAYER_HIT_KNOCKBACK_Y 230.0f
#define EXPLOSION_DAMAGE 2
/* Forgiving jump input: a jump still works for a beat after the boots leave
 * the ledge, and a press just before landing is kept until the boots arrive.
 * Without both, the game discards inputs the player visibly made. */
#define PLAYER_COYOTE_TIME 0.10f
#define PLAYER_JUMP_BUFFER 0.12f
/* Releasing the button caps the rise, so a tap hops and a hold clears. */
#define PLAYER_JUMP_CUT_FACTOR 0.45f
/*
 * How long a ladder stays let go of after a jump off it — the same idea as
 * `ENEMY_STOMP_LADDER_LOCKOUT` below, and needed for exactly the same reason.
 *
 * The ladder branch of `player_update` sets `vy` from the climb input every
 * frame, and the grab only asks that the box is over a rung with up or down
 * held. So a jump taken *while climbing* was let go of and grabbed again on the
 * very next frame, and the climb speed overwrote the jump — which made holding
 * up and pressing jump do nothing whatever. That is the one case the keyboard's
 * `LSHIFT` was added for: `UP` cannot be the jump over a ladder because it is
 * the climb, so the player pressing the separate jump key is nearly always
 * already holding the climb key, and the pad has the same shape under A.
 *
 * Short on purpose. It is long enough to carry the boots clear of the rung they
 * left — about a tile and a half at `PLAYER_JUMP_SPEED` — and no longer, so a
 * jump up a shaft still catches the ladder again on the way and reads as a
 * boost rather than as the rungs going dead.
 */
#define PLAYER_LADDER_JUMP_LOCKOUT 0.18f
#define PLAYER_CRAWL_H 18
#define PLAYER_CRAWL_SPEED 75.0f
#define PLAYER_KNIFE_RANGE 18.0f
#define PLAYER_KNIFE_ACTION_TIME 0.18f
/*
 * How long the pose an attack puts Chuck in is held for, one number per pose.
 *
 * Two of the four used to be literals: every one of the three throw branches in
 * `gameplay_combat.c` wrote `0.18f` and the pistol wrote `0.12f`, next door to a
 * `PLAYER_KNIFE_ACTION_TIME` that is *also* 0.18 and a `ROCKET_ACTION_TIME` that
 * is not. So a reader could not tell whether the throw was meant to last as long
 * as the blade or merely happened to, and tuning either one meant finding out by
 * grep which of five numbers were the same on purpose. They are named here, and
 * the two relationships between them are held by
 * `test_the_attack_poses_agree_with_what_is_drawn_on_them` rather than by the
 * assertions next door: a float comparison in a `_Static_assert` is a GNU
 * extension and this tree is built `-Wpedantic`, so what cannot be asked here is
 * asked in the suite. If the throw is meant to stop lasting as long as the blade,
 * that test is the line that has to say so out loud.
 */
#define PLAYER_THROW_ACTION_TIME 0.18f
#define PLAYER_SHOT_ACTION_TIME 0.12f
/*
 * And how much of an action's pose has a muzzle flash on it.
 *
 * `render_figures.c` asks `action_timer > 0.055f` in four places for Chuck and
 * once more for a guard's recoil, and `game.c` states the same number in a
 * comment beside the pose the soak sweep stages — six copies of one number, on
 * the side of the SDL line no test can reach, deciding whether the frame that
 * proves a shot happened is drawn at all. The renderers are the last place a
 * number should be written twice, because nothing over there can compare them.
 * It has to stay under `PLAYER_SHOT_ACTION_TIME` or the flash is never reached;
 * that is the other half the suite holds.
 */
#define PLAYER_MUZZLE_FLASH_TIME 0.055f
/*
 * What a guard taken from behind is worth, against the `ENEMY_SCORE` every
 * other way of putting one down pays.
 *
 * The premium is the whole of how this mechanic is taught. Nothing on screen
 * announces that the knife does something different behind a man who has not
 * seen you — there is no prompt, no highlight and no second button — so the
 * only thing that can say it happened is the number that comes up, and a
 * takedown paying exactly what a bullet pays is a mechanic the player is not
 * told they used. It is deliberately smaller than a hostile is worth twice
 * over: this is the quiet answer to one man, not a scoring route to run a
 * sector on.
 */
#define PLAYER_TAKEDOWN_SCORE 250
/*
 * Dragging a body, and the four numbers it takes.
 *
 * A corpse is already a place on the map rather than scenery —
 * `update_body_discovery` sends the next calm guard who sees one over to look
 * at it, and often on to the nearest alarm switch. That rule is simulated,
 * documented and punishing, and up to now the player's only answer to it was to
 * kill somebody where nobody would walk past. Which is not an answer, it is a
 * hope: patrol routes are the one thing about a sector that cannot be read off
 * the map. Dragging is the answer, and it costs what it should — half speed,
 * both hands, and no ladder.
 *
 * `PLAYER_DRAG_SPEED` is deliberately under `PLAYER_CRAWL_SPEED`. Crawling is
 * the other way to be hard to notice and it has to stay the quicker of the two,
 * or hauling a dead man about would be the fastest careful way across a floor.
 *
 * `BODY_DRAG_BREAK` is the leash. It is longer than the offset the body is held
 * at, so an ordinary walk never trips it and a body wedged against a doorframe
 * lets go rather than stretching across the room.
 */
#define PLAYER_DRAG_SPEED 62.0f
_Static_assert((int)PLAYER_DRAG_SPEED < (int)PLAYER_CRAWL_SPEED,
               "hauling a dead man would be quicker than crawling");
#define BODY_DRAG_REACH 24.0f
#define BODY_DRAG_OFFSET 24.0f
#define BODY_DRAG_BREAK 52.0f

/* An extra life every this many points gives the score a mechanical meaning:
 * better play literally buys more attempts. */
#define EXTRA_LIFE_SCORE_STEP 10000
/* A landing becomes audible well before it becomes dangerous. The fatal
 * threshold is about five tiles of uninterrupted free fall. */
#define PLAYER_LAND_SOUND_SPEED 150.0f
#define PLAYER_FATAL_FALL_SPEED 560.0f
/*
 * The same threshold as a height, which is what the route model needs: from
 * v^2 = 2gh, the drop that arrives at PLAYER_FATAL_FALL_SPEED. Derived here
 * rather than written down as a number of tiles, so retuning the speed or the
 * gravity moves the model's idea of a survivable fall with it instead of
 * leaving it certifying sectors the player would die crossing.
 */
#define PLAYER_FATAL_FALL_HEIGHT \
    ((PLAYER_FATAL_FALL_SPEED * PLAYER_FATAL_FALL_SPEED) / (2.0f * GRAVITY))

/* Enemy tuning */
#define ENEMY_W 26
#define ENEMY_H 32
#define ENEMY_WALK_SPEED 62.0f
#define ENEMY_CLIMB_SPEED 60.0f
#define ENEMY_CLIMB_COOLDOWN 1.8f
#define ENEMY_CLIMB_CHANCE 3
#define ENEMY_OBSTACLE_AVOID_TIME 1.25f
#define ENEMY_HP 3
/*
 * What a guard is worth, and the reason it is a name rather than a number.
 *
 * A guard pays this however he goes down — a round, a stomp, a blast, a crate,
 * a mine — with the single exception of the blade behind him, which pays
 * `PLAYER_TAKEDOWN_SCORE` because that premium is the only thing that teaches
 * the mechanic. A heavy pays it too: the vest is bought with rounds and with
 * the stomp being refused, not with points, and paying more for one would make
 * the loud answer to him the profitable one.
 *
 * **It was a bare `150` in three functions and four sentences**, which is the
 * defect this repository keeps finding on the floor written out in its most
 * literal form. The three were `damage_enemy` and `apply_blast` in
 * [gameplay_combat.c](gameplay_combat.c) and `gameplay_kill_enemy_with_crate`
 * in [gameplay_world.c](gameplay_world.c); the four were the comment on
 * `PLAYER_TAKEDOWN_SCORE` just below, `docs/gameplay.md`, `docs/story.md` and
 * the arithmetic in `EVIDENCE_SCORE`'s own comment above. Nothing held any of
 * them to any other, and `check_docs.py` — the script whose whole job is to
 * hold a sentence to a constant — could not read a constant that did not
 * exist. Tuning the number meant finding seven places and hoping.
 *
 * `DOG_SCORE` is the same story one line down, in the same three functions.
 * Half a man, because a dog is half an encounter: it closes fast and it dies
 * to one of anything.
 */
#define ENEMY_SCORE 150
#define DOG_SCORE 75
/*
 * The heavy, and what a plate carrier is worth.
 *
 * The campaign had one kind of man in it for fifteen sectors, and every
 * mechanic the player has learned answers that one man: a stomp for the cheap
 * kill, three rounds otherwise, a blade behind him if he has not looked. A
 * second kind exists to take one of those answers away rather than to be
 * harder — **the stomp**, which is the free one, the one that costs no
 * ammunition and no position, and the one a player falls back on the moment a
 * floor gets busy.
 *
 * `ENEMY_HEAVY_HP` is what he takes from the front. The blade behind him is
 * deliberately *not* raised with it: a takedown is a knife across a throat
 * rather than damage, so the man in the vest goes down to it exactly as the
 * man in the shirt does — which makes him the sector's clearest argument for
 * the quiet route rather than a wall to unload into.
 *
 * `ENEMY_HEAVY_SPEED` is under one because carrying it has to cost him
 * something the player can see. A heavy who moved at the ordinary pace would
 * be an ordinary guard with a longer health bar, which is the version of this
 * idea worth avoiding.
 */
#define ENEMY_HEAVY_HP 6
#define ENEMY_HEAVY_SPEED 0.72f
/* Worth more of a sector's budget than a plain guard's 3, and less than a
 * guard-and-dog pair's 5: he denies an answer rather than covering ground. */
#define ENEMY_HEAVY_HAZARD_WEIGHT 4
/* Stomping a guard from above bounces Chuck off instead of killing him. */
#define ENEMY_STOMP_BOUNCE_SPEED 300.0f
/* Briefly blocks re-grabbing a ladder after a stomp, so climbing down onto a
 * guard below doesn't just overwrite the bounce with the climb speed. */
#define ENEMY_STOMP_LADDER_LOCKOUT 0.3f

/* Perception. Guards no longer see only along their exact floor row: they have
 * a forward vision cone (wide field of view, diagonal sight up and down) with a
 * ray-cast line of sight that walls, floors, and crates block. A short
 * peripheral radius lets a guard notice something right next to or behind it. */
#define ENEMY_VIEW_RANGE (7 * TILE_SIZE)
/* cos of the cone half-angle. 0.34 => ~70deg half-angle (~140deg total FOV). */
#define ENEMY_VIEW_CONE_COS 0.34f
#define ENEMY_PERIPHERAL_RANGE (1.6f * TILE_SIZE)
/* A crawling player is stealthier: spotted only at closer range. */
#define ENEMY_CRAWL_VIEW_FACTOR 0.55f
#define ENEMY_LOS_STEP 6.0f

/* Hearing: gunfire and explosions draw nearby guards to investigate. */
#define ENEMY_HEAR_TILES_SHOT 7
#define ENEMY_HEAR_RADIUS_SHOT ((float)ENEMY_HEAR_TILES_SHOT * TILE_SIZE)
#define ENEMY_HEAR_RADIUS_BLAST (11.0f * TILE_SIZE)

/* The bolt has to stay the quieter of the two. Both radii were written down as
 * a rule in two prose comments — the one beside `DECOY_NOISE_RADIUS` and the
 * one in docs/gameplay.md — and held by nothing, which is the arrangement this
 * file keeps turning into a build failure everywhere else. Swap the numbers and
 * the quietest option in the game becomes the one that reaches furthest. */
_Static_assert(DECOY_NOISE_TILES < ENEMY_HEAR_TILES_SHOT,
               "a thrown bolt would carry further than a gunshot");

/* Suspicion / investigation: a soft alert short of a full building alarm. A
 * guard walks warily to the disturbance, scans, then resumes its patrol. */
#define ENEMY_INVESTIGATE_TIME 5.0f
#define ENEMY_INVESTIGATE_LOOK_TIME 1.2f
#define ENEMY_INVESTIGATE_REACH 20.0f
#define ENEMY_INVESTIGATE_SCAN_FLIP 0.55f
/* Discovering a fallen comrade is a strong signal to raise the alarm. */
#define ENEMY_BODY_NOTICE_RANGE (3.0f * TILE_SIZE)
#define GUARD_BODY_ALARM_CHANCE 65

/* Combat tactics */
#define ENEMY_AIM_LEAD 0.26f /* seconds of target velocity to lead a shot */
#define ENEMY_VERTICAL_SHOOT_RANGE (4 * TILE_SIZE)
#define ENEMY_VERTICAL_SHOOT_HALF_W (TILE_SIZE * 0.55f)
/* Posted-up guards hold and fire from range instead of crowding into melee. */
#define ENEMY_KEEP_DISTANCE (3.2f * TILE_SIZE)

/* Pursuit movement: guards hop small gaps while chasing. At a grounded crate,
 * they slow the horizontal part of the jump so they land on its top before
 * continuing beyond it. */
#define ENEMY_JUMP_SPEED 245.0f
#define ENEMY_GAP_JUMP_SPEED 190.0f
#define ENEMY_JUMP_MIN_SPEED 190.0f
#define ENEMY_JUMP_MAX_GAP_TILES 2
#define ENEMY_STEP_DOWN_MAX_TILES 2
#define ENEMY_CRATE_JUMP_LOOKAHEAD 26.0f
#define ENEMY_CRATE_MOUNT_SPEED 90.0f
#define ENEMY_CRATE_JUMP_CLEARANCE 2.0f
#define ENEMY_CRATE_FLOOR_TOLERANCE 3.0f
/* Guards may either mount a safe crate or walk in front of it. Pursuers are
 * more likely to jump; patrols more often take the foreground route. */
#define ENEMY_CRATE_PURSUIT_JUMP_CHANCE 50
#define ENEMY_CRATE_PATROL_JUMP_CHANCE 35
#define ENEMY_ELEVATOR_FLOOR_TOLERANCE 8.0f
/* A search party fans out around the last sighting instead of clustering. */
#define ENEMY_SEARCH_FAN 1.5f
#define ENEMY_ALARM_SPEED_MULTIPLIER 1.28f
/* Alarm aggression is floored above human reaction time: 0.45s x 0.78 is
 * 0.35s of aim, and the first synchronized volley waits most of a second.
 * An alarm should raise pressure, not fire below what a player can answer. */
#define ENEMY_ALARM_AIM_MULTIPLIER 0.78f
#define ENEMY_ALARM_COOLDOWN_MULTIPLIER 0.55f
#define ENEMY_ALARM_INITIAL_SHOT_DELAY 0.8f
/* A guard that freshly spots Chuck spends this long noticing him before the
 * aim telegraph even starts. Provoked guards and an active alarm skip it:
 * they are already looking for him. */
#define ENEMY_NOTICE_TIME 0.35f
#define ENEMY_ALARM_SEARCH_RADIUS (2.0f * TILE_SIZE)
#define ENEMY_ALARM_SEARCH_NEAR_RADIUS (2.4f * TILE_SIZE)

/* Guard dog tuning */
#define DOG_W 24
#define DOG_H 16
#define DOG_HP 1
#define DOG_PATROL_SPEED 90.0f
#define DOG_RETURN_SPEED 135.0f
/* Faster than Chuck's 135, so a dog still wins a flat footrace, but slow
 * enough that a jump over it or a ladder is a real answer. */
#define DOG_CHASE_SPEED 165.0f
/* The crouch-and-growl beat before a bite connects. */
#define DOG_BITE_WINDUP 0.30f
#define DOG_JUMP_SPEED 285.0f
#define DOG_JUMP_MIN_SPEED 150.0f
#define DOG_JUMP_MAX_GAP_TILES 2
#define DOG_STEP_DOWN_MAX_TILES 2
#define DOG_TURN_COOLDOWN 0.4f
#define DOG_HANDLER_DISTANCE 34.0f
#define DOG_ROAM_RADIUS 90.0f
#define DOG_RETURN_RADIUS 130.0f
#define DOG_VIEW_RANGE (6 * TILE_SIZE)
#define DOG_BACK_SENSE_RANGE (2 * TILE_SIZE)
#define DOG_BITE_RANGE 16.0f
#define DOG_BITE_COOLDOWN 0.75f
#define DOG_LOST_TIME 2.0f
#define DOG_DOOR_HANDLER_CHANCE 30

#define ENEMY_RETALIATE_RADIUS (3 * TILE_SIZE)
#define ENEMY_RETALIATE_CHANCE 8
#define ENEMY_TALK_DURATION 4.0f
#define ENEMY_TALK_NOTICE_RADIUS (TILE_SIZE * 1.5f)
#define ENEMY_TALK_CHANCE 30
#define ENEMY_TALK_COOLDOWN 5.0f

/* A guard with nobody beside him still has somebody to talk to: twelve of
 * them badged into this building as one contractor, and they are working to
 * a clock. The radio check is the solo half of the chat — the same standing
 * beat, half as long, and only while the building is still quiet. */
#define ENEMY_RADIO_DURATION 2.2f
#define ENEMY_RADIO_GAP_MIN 16.0f
#define ENEMY_RADIO_GAP_MAX 38.0f

/*
 * How near Chuck has to be to make out the words, and how long the line he
 * made out stays on the strip.
 *
 * The reach is deliberately shorter than the 16 tiles a routine sound carries
 * (`audio_play_at`): a sound heard from off screen is a cue about somewhere
 * else, and a *sentence* printed from a man the player cannot see is the game
 * subtitling thin air. Eleven tiles is 352px, which is inside the 400px half
 * of the viewport, so whoever is speaking is on screen whenever the camera has
 * caught up.
 */
#define CHATTER_EARSHOT (TILE_SIZE * 11.0f)
#define CHATTER_HOLD_TIME 3.8f

/* Ammunition dropped by guards downed in direct combat (bullet, knife or
 * stomp). Explosions destroy the magazine along with its owner. */
#define MAX_AMMO_DROPS 16
#define AMMO_DROP_BULLETS 2
#define AMMO_DROP_W 10
#define AMMO_DROP_H 8

/* Assist options, chosen on the title screen. Multipliers of the defaults;
 * leaving them off changes nothing. */
#define ASSIST_ENEMY_SPEED 0.8f
/*
 * And the veteran run, which is the same lever pulled the other way.
 *
 * One number rather than a table of them, because a second difficulty *tuning*
 * would be a second campaign to balance: every map, every hazard budget and
 * every jump in this tree is drawn against the pace in `game_config.h`, and a
 * mode that moved several of those at once would be a set of sectors nobody had
 * played. What it moves is the crew's pace, the lives in hand and the continues
 * — three numbers the player already understands, all of them read at the same
 * places the assist switches are read.
 *
 * 1.18 rather than something rounder: `ENEMY_WALK_SPEED` at 62 becomes 73, which
 * is still under `PLAYER_WALK_SPEED`. A crew that outran Chuck on open floor
 * would make the whole quiet half of the game pointless — there would be no
 * such thing as breaking off — and that is a different game rather than a
 * harder one.
 */
#define VETERAN_ENEMY_SPEED 1.18f
#define VETERAN_LIVES 1
#define VETERAN_CONTINUES 0

/* Projectiles */
#define MAX_BULLETS 8
#define BULLET_SPEED 600.0f
#define BULLET_W 8
#define BULLET_H 4
#define MAX_AMMO 6
/*
 * One slot per man on the floor, which is what stops a guard aiming and firing
 * nothing.
 *
 * `fire_enemy_bullet` walks this array for a free slot and, finding none,
 * returns having done nothing at all: no round, no recoil, no `SFX_ENEMY_SHOT`
 * — and the aim that led to it has already been spent by the caller. So the
 * failure is not a dropped bullet, it is a guard who visibly levels his weapon
 * at Chuck and produces silence. The player's own dry trigger is answered with
 * `SFX_EMPTY_CLICK` in three separate places in `gameplay_combat.c`, for the
 * stated reason that a press which does nothing has to say so; there is no
 * equivalent here and there should not need to be one.
 *
 * It was 16 against a `MAX_ENEMIES` of 24. Sector 14 is the floor that reached
 * it: twelve men drawn on the map and three consoles, each of which sends for
 * `TERMINAL_REINFORCEMENT_MAX_COUNT` more under an alarm — eighteen guns, and
 * the alarm is exactly the state in which all of them are shooting at once.
 * The same arithmetic `test_every_sector_can_seat_the_reinforcements_it_can_call`
 * does for the seating, one array over.
 *
 * Tied to `MAX_ENEMIES` rather than raised to a round number, because the
 * relationship is the rule: a raised enemy ceiling is a raised bullet ceiling,
 * and the assertion below is what makes the two move together instead of one of
 * them being remembered.
 */
#define MAX_ENEMY_BULLETS MAX_ENEMIES
_Static_assert(MAX_ENEMY_BULLETS >= MAX_ENEMIES,
               "every guard on the floor needs a slot, or one of them aims "
               "and fires nothing");
#define ENEMY_BULLET_SPEED 380.0f
#define ENEMY_SHOOT_RANGE (7 * TILE_SIZE)
#define ENEMY_SHOOT_COOLDOWN 2.5f
#define ENEMY_AIM_TIME 0.45f
#define ENEMY_MUZZLE_MIN_Y_FACTOR 0.30f
#define ENEMY_MUZZLE_MAX_Y_FACTOR 0.70f

/* No projectile may cross a whole tile in one frame: each of them samples the
 * tile under its leading edge once per step, so a step longer than a tile can
 * step straight over a one-tile wall without ever being inside it. See
 * MAX_FRAME_DT. Raising a speed here is allowed; raising it past this line
 * means the sampling has to become a sweep first. */
_Static_assert((int)BULLET_SPEED < TILE_SIZE * MIN_FRAME_RATE,
               "a pistol round would cross a whole tile in one frame");
_Static_assert((int)ENEMY_BULLET_SPEED < TILE_SIZE * MIN_FRAME_RATE,
               "a guard's round would cross a whole tile in one frame");
_Static_assert((int)ROCKET_SPEED < TILE_SIZE * MIN_FRAME_RATE,
               "a rocket would cross a whole tile in one frame");

/*
 * And the one projectile that is *not* swept has to clear a second bar.
 *
 * Everything the player fires is tested against the ground it crossed
 * (`gameplay_combat_update_player_bullets`), because a round is four pixels by
 * eight and a dog is sixteen tall. A guard's round is tested where it ended up
 * instead, which is only honest while one step is shorter than the target it
 * would otherwise step over — and the smallest target in the game is Chuck
 * crawling, eighteen pixels of him under an eight-pixel round fired straight
 * down a shaft. There are seven pixels of margin at the moment, which is
 * exactly why this is written down rather than left to be rediscovered: raising
 * the speed past this line means the enemy round has to be swept first.
 */
_Static_assert((int)ENEMY_BULLET_SPEED <
                   (BULLET_W + PLAYER_CRAWL_H) * MIN_FRAME_RATE,
               "a guard's round could step over a crawling player");

/*
 * And the same round now has a second thing it can hit, which is smaller than
 * Chuck is.
 *
 * A gas canister is twelve pixels across, so a horizontal round crossing
 * twenty of them per step is the tightest case in the game: eight of bullet
 * plus twelve of cylinder against nineteen pixels of travel is one pixel of
 * margin, and the whole point of the canister being on this list is that the
 * player can shelter behind it. A round that steps over it is a round that
 * goes through it, which is the bug this replaced.
 */
_Static_assert((int)ENEMY_BULLET_SPEED <
                   (BULLET_W + GAS_CANISTER_W) * MIN_FRAME_RATE,
               "a guard's round could step over a gas canister");

#define ENEMY_SPEED_HP2 0.60f
#define ENEMY_SPEED_HP1 0.30f

/* Shared physics */
#define GRAVITY 980.0f
#define MAX_FALL_SPEED 620.0f
#define INVULN_TIME 1.5f

/*
 * And the same rule for everything that falls.
 *
 * `level_move` resolves the vertical axis by testing the single tile row under
 * the leading edge *after* the step, exactly as a projectile does, so a body
 * falling further than a tile in one frame drops straight through a one-tile
 * floor without ever having been inside it. Every falling thing in the game is
 * clamped to this one speed — the player, guards, dogs, crates, grenades,
 * magazines, settling bodies and the bricks thrown off the facade — so one
 * assertion covers all of them.
 *
 * It has a pixel of margin at the moment (620/20 is 31 against a 32px tile),
 * which is exactly why it is worth pinning: the number reads like a free
 * tuning knob and it is not one.
 */
_Static_assert((int)MAX_FALL_SPEED < TILE_SIZE * MIN_FRAME_RATE,
               "a falling body would cross a whole tile in one frame");

/* Doors */
#define MAX_DOORS 8
#define DOOR_SPAWN_INTERVAL 5.0f
#define TELEPORT_COOLDOWN 0.3f

/* Platforms */
#define MAX_ELEVATORS 8
#define ELEVATOR_SPEED 72.0f
#define ELEVATOR_PLAT_H 6
#define MAX_FALL_PLATFORMS 64
#define FALL_PLATFORM_H 6
#define FALL_PLATFORM_TRIGGER_DELAY 0.25f
#define FALL_PLATFORM_ACCEL 420.0f
#define MAX_MOVING_PLATFORMS 64
#define MOVING_PLATFORM_H 6
#define MOVING_PLATFORM_SPEED 72.0f

/* Hazards */
#define MAX_SPIKES 128
#define SPIKE_W TILE_SIZE
#define SPIKE_H TILE_SIZE
#define MAX_CEILING_FANS 64
#define CEILING_FAN_BLADE_LENGTH 23.0f
#define CEILING_FAN_HIT_HEIGHT 8.0f
#define CEILING_FAN_CENTER_Y 10.0f

/* Exterior facade climb hazards. Map-authored sources wake only while the
 * player is nearby, keeping tall levels active around the visible route. */
#define MAX_FACADE_HAZARD_SPAWNS 32
#define MAX_THROWN_OBJECTS 12
#define MAX_BIRDS 12
#define THROWN_OBJECT_SIZE 14.0f
#define THROWN_OBJECT_SPEED 205.0f
#define THROWN_OBJECT_GRAVITY 410.0f
#define THROWN_OBJECT_SPAWN_MIN 1.8f
#define THROWN_OBJECT_SPAWN_MAX 3.4f
/* A source shouts and leans out before it lets go, so every throw can be
 * answered by moving behind a ledge instead of by memorising the map. */
#define THROWN_OBJECT_WINDUP 0.60f
#define BIRD_W 26.0f
#define BIRD_H 12.0f
#define BIRD_SPEED 155.0f
#define BIRD_SPAWN_MIN 2.4f
#define BIRD_SPAWN_MAX 4.6f
/* Birds swoop rather than fly on rails; the wave is derived from their own
 * animation clock so the motion stays deterministic. */
#define BIRD_WAVE_SPEED 190.0f
#define BIRD_WAVE_RATE 3.6f
#define FACADE_HAZARD_WAKE_RANGE (10.0f * TILE_SIZE)
#define FACADE_CLIMB_SPEED 112.0f
#define FACADE_BUILDING_SIDE_INSET 80.0f
#define FACADE_CLIMB_SIDE_MARGIN FACADE_BUILDING_SIDE_INSET

/* Wind. Gusts announce themselves for a beat, then shove the climber sideways
 * along the wall unless a ledge or air-conditioning unit upwind of him breaks
 * the gust. Standing still in the open during a gust is never fatal, but it
 * costs the route, which is what makes the shelters worth using. */
#define FACADE_WIND_WARN_TIME 1.15f
#define FACADE_WIND_GUST_TIME 2.30f
#define FACADE_WIND_CALM_MIN 4.20f
#define FACADE_WIND_CALM_MAX 7.60f
#define FACADE_WIND_PUSH 82.0f
/* How far upwind a solid tile still counts as cover. */
#define FACADE_WIND_SHELTER_REACH 42.0f

/* Climb checkpoints. A tall wall would be miserable if one brick cost the
 * whole ascent, so height already earned is banked every few floors and a
 * lost life resumes from there. */
#define FACADE_CHECKPOINT_STEP (3.0f * TILE_SIZE)

/* Prologue car chase. A top-down, forward-only pursuit played once before the
 * campaign, and the middle of the prologue's three beats: Chuck tails the SUV
 * Ellen was put into through night traffic until it reaches Kessler Tower,
 * where the platformer starts. Road space is measured in
 * pixels, x across the road and y along the driving direction (y grows
 * forward), so the simulation needs no separate world scale. */
#define CHASE_ROAD_WIDTH 480.0f
#define CHASE_LANE_COUNT 4
#define CHASE_LANE_WIDTH (CHASE_ROAD_WIDTH / (float)CHASE_LANE_COUNT)
/* Lanes 0..1 carry oncoming traffic, 2..3 run with the pursuit. */
#define CHASE_FIRST_FORWARD_LANE 2
#define CHASE_KERB_MARGIN 5.0f
#define CHASE_CAR_LENGTH 84.0f
#define CHASE_CAR_WIDTH 46.0f
#define CHASE_SUV_LENGTH 98.0f
#define CHASE_SUV_WIDTH 54.0f
#define CHASE_MAX_CARS 24
#define CHASE_MAX_INTERSECTIONS 4
#define CHASE_BLOCK_LENGTH 1000.0f
#define CHASE_JUNCTION_HALF 92.0f
#define CHASE_SPAWN_MARGIN 760.0f
#define CHASE_CULL_MARGIN 280.0f
/* Never more than this many cars abreast, so at least two lanes stay open. */
#define CHASE_MAX_CARS_ABREAST 2

/* Player car handling. Coasting matches the SUV's own speed, so holding a lane
 * keeps the trail while every dodge and crash has to be paid back on the
 * accelerator. */
#define CHASE_CRUISE_SPEED 320.0f
#define CHASE_MAX_SPEED 470.0f
#define CHASE_MIN_SPEED 140.0f
#define CHASE_ACCEL 235.0f
#define CHASE_BRAKE 330.0f
#define CHASE_COAST 130.0f
#define CHASE_STEER_SPEED 250.0f
#define CHASE_CRASH_SPEED 150.0f
#define CHASE_SCRAPE_DRAG 150.0f
#define CHASE_SCRAPE_SOUND_INTERVAL 0.34f
#define CHASE_INTEGRITY 3
#define CHASE_HIT_INVULN 1.1f
#define CHASE_NEAR_MISS_SIDE 62.0f
#define CHASE_NEAR_MISS_AHEAD 130.0f
#define CHASE_HORN_INTERVAL 1.6f
#define CHASE_ENGINE_INTERVAL 1.05f

/* Traffic */
#define CHASE_TRAFFIC_SPEED_MIN 150.0f
#define CHASE_TRAFFIC_SPEED_MAX 215.0f
#define CHASE_ONCOMING_SPEED_MIN 150.0f
#define CHASE_ONCOMING_SPEED_MAX 205.0f
#define CHASE_CROSS_SPEED_MIN 195.0f
#define CHASE_CROSS_SPEED_MAX 265.0f
#define CHASE_CROSS_GAP_MIN 0.85f
#define CHASE_CROSS_GAP_MAX 1.40f
#define CHASE_CROSS_LANE_OFFSET 42.0f
#define CHASE_CROSS_ALERT_RANGE 900.0f
#define CHASE_SIGNAL_PERIOD 7.4f
#define CHASE_SIGNAL_CROSS_GREEN 3.1f
#define CHASE_WRECK_DRIFT 95.0f

/* The hunted SUV */
#define CHASE_TARGET_SPEED 300.0f
#define CHASE_TARGET_SPEED_SWING 26.0f
#define CHASE_TARGET_BOOST 65.0f
#define CHASE_TARGET_BOOST_TIME 1.6f
#define CHASE_TARGET_STEER_SPEED 205.0f
#define CHASE_TARGET_LANE_TIME_MIN 2.2f
#define CHASE_TARGET_LANE_TIME_MAX 4.8f
#define CHASE_TARGET_LOOKAHEAD 210.0f
#define CHASE_START_GAP 380.0f
#define CHASE_MIN_GAP 190.0f
#define CHASE_LOSE_GAP 1050.0f

/* The opening beat, cue by cue: the crew slam a door and pull away while Chuck
 * runs up the pavement, unlocks his car and pulls out after them. It resumes
 * the abduction cutscene's last frame, so the two read as one shot. The
 * renderer stages Chuck's run from the same timings the simulation uses. */
#define CHASE_KERB_X \
    (CHASE_ROAD_WIDTH - CHASE_CAR_WIDTH * 0.5f - CHASE_KERB_MARGIN)
#define CHASE_DEPARTURE_TARGET_OFFSET 260.0f
/* They leave at ordinary traffic speed: they are not being chased yet, which is
 * what lets Chuck close the distance after his late start. */
#define CHASE_DEPARTURE_TARGET_SPEED 230.0f
#define CHASE_DEPARTURE_TARGET_ACCEL 190.0f
#define CHASE_DEPARTURE_SUV_DOOR 1.20f
#define CHASE_DEPARTURE_SUV_START 1.40f
#define CHASE_DEPARTURE_CHUCK_RUN 1.60f
#define CHASE_DEPARTURE_CAR_DOOR 3.05f
#define CHASE_DEPARTURE_IGNITION 3.30f
#define CHASE_DEPARTURE_PULL_OUT 3.45f

/* Arrival geometry, measured back from the building's front face. */
#define CHASE_ARRIVAL_PLAYER_STOP 310.0f
#define CHASE_ARRIVAL_TARGET_STOP 130.0f
#define CHASE_ARRIVAL_CAMERA_LEAD 70.0f

/* Camera and pacing. The camera keeps the player's car near the bottom of the
 * view so most of the frame shows the road being driven into. */
#define CHASE_CAMERA_LEAD 135.0f
/* The opening and closing beats sit further back so the SUV can be watched
 * driving away, and so the destination fits in frame. */
#define CHASE_DEPARTURE_CAMERA_LEAD 110.0f
#define CHASE_PAVEMENT_WIDTH 26.0f
#define CHASE_DEPARTURE_DURATION 6.0f
#define CHASE_PURSUIT_DURATION 40.0f
/* How long the pedals stay named on the road at the head of every attempt.
 * A car is the one thing in the game nobody guesses the controls for — the
 * platformer never asks for a throttle — so the prompt is shown outright and
 * comes back after every crash, not only at the very start. */
#define CHASE_CONTROL_HINT_TIME 7.0f
/* A failed attempt rewinds the drive by a beat instead of to zero, and after
 * a couple of failures the whole drive can be skipped: the prologue must
 * never be the wall someone quits the game on. */
#define CHASE_FAIL_REWIND 12.0f
#define CHASE_SKIP_AFTER_ATTEMPTS 2
#define CHASE_FAILED_DURATION 2.4f
#define CHASE_ARRIVAL_DURATION 5.4f
/* Both cars are on their marks before the beat ends. */
#define CHASE_ARRIVAL_BRAKE_TIME 4.2f
#define CHASE_ARRIVAL_DISTANCE 1500.0f

/* The cordon the broadcast bought them, and the reason the drive is threaded
 * through it rather than raced down an empty street. A squad car stands on the
 * kerb at a junction with its bar lit, sealing the street: scenery on the
 * pavement, never in a lane, never something that can be hit.
 *
 * The ramp is spatial, not temporal — thin at the edge of the ring and thick
 * at the tower, because that is where the units converged. That is exactly why
 * the demand goes out at 00:04 and the pavement outside the coffee window is
 * taken at 00:12: the whole cordon has to already be standing when the drive
 * starts, or the player passes the consequence of a broadcast that has not
 * happened. It also buys the abduction its impunity — every unit in the city
 * is looking at this building, and nobody at all is looking three blocks out.
 *
 * Moving either clock means moving the other with it. */
#define CHASE_CORDON_FIRST_BLOCK 2
#define CHASE_CORDON_CHANCE_START 20
#define CHASE_CORDON_CHANCE_END 85
#define CHASE_CORDON_RAMP_BLOCKS 9
#define CHASE_CORDON_KERB_INSET 13.0f
#define CHASE_CORDON_STROBE_HZ 3.4f

/* ---- The clock on the wall -------------------------------------------- */

/*
 * The night has a deadline and the building states it out loud: at 01:00 the
 * overnight settlement — six hundred and forty million in bearer bonds — leaves
 * the roof on their helicopter, which is the only reason any of this is
 * happening tonight. A `w` clock reads the campaign sector it is standing in,
 * so the minute hand climbs toward the top of the dial across the campaign's
 * sectors and the player can watch the job close in without a line of text.
 * The dial itself is presentation and nothing reads its position — but the
 * *rate* is not, any more: `SECTOR_PAR_SECONDS` below is derived from it, so
 * the allowance the fiction gives a floor is the allowance the score gives it.
 *
 * The first sector opens at 00:22, which is when the prologue hands over: the
 * SUV reaches the tower at 00:22 and Chuck is through the door behind it, so
 * the lobby's dial reads the minute the cutscene before it was captioned. The
 * last is 00:57, three minutes short of the deadline.
 */
/*
 * The night, and the one number in it that moves when the campaign grows.
 *
 * Every time the game states is on one line and none of them may be moved
 * alone — 00:04 the broadcast, 00:12 the pavement, 00:22 the front door, 01:00
 * the roof — and two cutscenes, the manual's `THE NIGHT` sheet, the intel table
 * and both prose pages all say them out loud. So the night's *length* is the
 * fixed thing here and the per-sector step is what is derived from it: thirty
 * eight minutes divided by however many sectors there are.
 *
 * That is why adding the vault and the fifth climb cost one edit rather than
 * fifteen. The dial upstairs still reads 00:22 in the lobby and still reaches
 * 01:00 on the roof; each floor simply gets a shade under two and a half
 * minutes instead of exactly it, and `SECTOR_PAR_SECONDS` follows because it is
 * derived from the same figure.
 *
 * It is written out rather than computed from `EMBEDDED_LEVEL_COUNT` for one
 * reason: this header is included by the gameplay core, which links no level
 * data at all and must not start. `test_the_night_clock_fills_the_night` is
 * what holds the arithmetic instead — a sector added without touching this
 * number is a campaign that no longer ends at 01:00, and the suite says so.
 */
#define NIGHT_CLOCK_FIRST_MINUTE 22.0f
#define NIGHT_CLOCK_TOTAL_MINUTES 38.0f
#define NIGHT_CLOCK_SECTORS 17
#define NIGHT_CLOCK_MINUTES_PER_SECTOR \
    (NIGHT_CLOCK_TOTAL_MINUTES / (float)NIGHT_CLOCK_SECTORS)

/*
 * The same number under the name of the thing rather than of the dial.
 *
 * `test_the_night_clock_fills_the_night` already holds `NIGHT_CLOCK_SECTORS`
 * against `EMBEDDED_LEVEL_COUNT`, so this is the campaign's length, checked —
 * and it is an alias rather than a second literal precisely so it cannot come
 * to disagree. It exists because the clock is not the only thing that needs to
 * know how long the campaign is: the manual's `THE MISSION` sheet *draws* the
 * route as one tick a sector, and it drew fifteen of them for a while after
 * there were seventeen, because the loop counted to a number written into the
 * renderer. A count in a drawing is as wrong as a count in a sentence and
 * harder to notice, since no fit check measures a picture.
 */
#define CAMPAIGN_SECTORS NIGHT_CLOCK_SECTORS

/*
 * What a sector is worth for being finished, and why the clock decides it.
 *
 * The report between floors has printed TIME and DEATHS since it existed and
 * neither number was read by anything: of the four fields on it only SCORE
 * belonged to the run, so the game showed a player a stopwatch that could not
 * matter and a death count that cost nothing beyond the walk back. That is
 * worse than showing neither, because the fiction spends a great deal of
 * effort insisting the night is against a clock — 01:00 is when the bonds
 * leave the roof, the wall dials climb toward it, the intel line after sector
 * eleven says TEN MINUTES — and the one place the player could act on that
 * said nothing.
 *
 * **The par is the fiction's own allowance, not a number invented for a
 * bonus.** A sector gets NIGHT_CLOCK_MINUTES_PER_SECTOR on the dial upstairs,
 * so that is exactly what it gets down here: finish inside the slot the night
 * clock gives the floor and the seconds left over pay, overrun it and they do
 * not. Deriving it means the two can never disagree — moving the dial moves
 * the par with it, and `test_the_sector_par_is_the_night_clock_s_own` says so
 * out loud rather than leaving it to whoever edits the constant above.
 *
 * The rates are set so a fast, clean floor is worth about what its guards are:
 * the whole par is 2680 at `SECTOR_TIME_BONUS_PER_SECOND` and a sector holds
 * eight or so men at 150 apiece. Speed is therefore a real alternative to
 * clearing the floor rather than a rounding error on top of it, which is the
 * whole point — the two ways to play a sector should pay comparably. That
 * sentence used to quote 150 seconds and 3000 points, which were the par and
 * the ceiling of a fifteen-sector night; both moved when the vault and the
 * fifth climb divided the same thirty-eight minutes seventeen ways.
 *
 * **And the par is comfortable rather than tight, which is worth writing down
 * because the arithmetic looks alarming from the other direction.** Dividing a
 * fixed night by a growing campaign shortens every floor's slot — 134 seconds
 * now against the old 150 — while the maps themselves have grown, sector 17
 * being 60x20 against sector 1's 34x16. Measured through the route model in
 * [level_route.c](level_route.c), the shortest walk from start to the way out
 * costs 32 seconds on the longest sector in the game and under 14 on most of
 * them: a quarter of the par at worst. The slot pays for the card detour, the
 * fighting and the hesitating, not for the walk, so a sector added from here
 * does not threaten the bonus until a floor plan is three times the size of
 * anything the campaign currently holds.
 */
/*
 * Truncated to whole seconds, because the bonus is counted in them and the
 * report prints them. It was an exact 150 while the step was exactly two and a
 * half minutes; a night divided seventeen ways is not a round number of
 * seconds, and a par carrying a fraction would pay a point nobody could see
 * where it came from. Down rather than nearest, so the par is never longer than
 * the slot the dial upstairs actually gives the floor.
 */
#define SECTOR_PAR_SECONDS \
    ((float)(int)(NIGHT_CLOCK_MINUTES_PER_SECTOR * 60.0f))
#define SECTOR_TIME_BONUS_PER_SECOND 20
#define SECTOR_CLEAN_BONUS 500

#endif /* CHUCK_GAME_CONFIG_H */
