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
#define MAX_LEVEL_HEIGHT 48
#define MAX_ITEMS 128
#define MAX_ENEMIES 16
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

/* Weak walls: a blocked-up opening that only an explosion reopens. Nothing
 * smaller than a blast touches one, so a route through a wall always costs an
 * explosive — which is what keeps it a shortcut rather than a corridor. The
 * hole is permanent for the rest of the run, like a fallen panel. */
#define WEAK_WALL_DUST 9
#define WEAK_WALL_SCORE 25

/* Exit-access terminals */
#define MAX_TERMINALS 16
#define TERMINAL_HACK_TIME 4.0f
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
#define GRENADE_FUSE_TIME 1.4f
#define GRENADE_RADIUS 48.0f
#define GRENADE_THROW_SPEED 260.0f

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
#define ENEMY_HEAR_RADIUS_SHOT (7.0f * TILE_SIZE)
#define ENEMY_HEAR_RADIUS_BLAST (11.0f * TILE_SIZE)

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

/* Projectiles */
#define MAX_BULLETS 8
#define BULLET_SPEED 600.0f
#define BULLET_W 8
#define BULLET_H 4
#define MAX_AMMO 6
#define MAX_ENEMY_BULLETS 16
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
 * so the minute hand climbs toward the top of the dial across the fifteen
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
#define NIGHT_CLOCK_FIRST_MINUTE 22.0f
#define NIGHT_CLOCK_MINUTES_PER_SECTOR 2.5f

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
 * a full 150 seconds under par is 3000 and a sector holds eight or so men at
 * 150 apiece. Speed is therefore a real alternative to clearing the floor
 * rather than a rounding error on top of it, which is the whole point — the
 * two ways to play a sector should pay comparably.
 */
#define SECTOR_PAR_SECONDS (NIGHT_CLOCK_MINUTES_PER_SECTOR * 60.0f)
#define SECTOR_TIME_BONUS_PER_SECOND 20
#define SECTOR_CLEAN_BONUS 500

#endif /* CHUCK_GAME_CONFIG_H */
