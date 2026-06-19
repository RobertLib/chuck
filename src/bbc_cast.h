#ifndef CHUCK_BBC_CAST_H
#define CHUCK_BBC_CAST_H

/*
 * Everybody in the game, and everything they carry, as BBC sprites.
 *
 * One file for the whole cast because the same people appear on four kinds of
 * screen — the sector, the prologue, the report between sectors and the
 * ending — and a figure drawn twice is two figures. Chuck at the kerb is the
 * Chuck on the ladder; the guard on the manual's sheet is the guard on the
 * floor. Renderers own where a sprite goes and which frame it is on; they do
 * not own what it looks like.
 *
 * The conventions every figure here keeps:
 *
 *  - **Eight columns by sixteen rows** for anyone standing, which is a 32x32
 *    logical box over a 26x32 collision box — the extra pixel each side is
 *    the shoulder and the toe. Smaller figures (the dog, staff at six
 *    columns) say so beside them.
 *  - **Facing right.** `bbc_sprite`'s `dir` mirrors them.
 *  - **Feet on the bottom row**, so `bbc_sprite_at_feet` stands a figure on
 *    its box's floor.
 *  - **No outline.** Rooms are black and the cast is bright; a black ring
 *    round a figure on a BBC is a hole cut in whatever is behind it.
 *  - **Skin is white, eyes are black.** There is no flesh tone in eight
 *    colours, and white is the one every BBC game of the period used.
 *
 * SDL-free, in `TEST_SOURCES`: the suite holds every sprite in
 * `BBC_CAST_ALL` to `bbc_sprite_valid`.
 */

#include "bbc_art.h"

/* ---- Chuck ---------------------------------------------------------- */

extern const BbcSprite SPR_CHUCK_STAND;
/* A walk is WALK1, WALK2, WALK3, WALK2: stride, pass, the other stride, pass. */
extern const BbcSprite SPR_CHUCK_WALK1;
extern const BbcSprite SPR_CHUCK_WALK2;
extern const BbcSprite SPR_CHUCK_WALK3;
/* The beat after a landing: knees taken up. */
extern const BbcSprite SPR_CHUCK_LAND;
extern const BbcSprite SPR_CHUCK_JUMP;
extern const BbcSprite SPR_CHUCK_FALL;
extern const BbcSprite SPR_CHUCK_AIM;
extern const BbcSprite SPR_CHUCK_AIM_UP;
extern const BbcSprite SPR_CHUCK_AIM_DOWN;
extern const BbcSprite SPR_CHUCK_KNIFE;
extern const BbcSprite SPR_CHUCK_THROW;
extern const BbcSprite SPR_CHUCK_SHOULDER;
extern const BbcSprite SPR_CHUCK_DRAG;
/* The other half of the hauling walk. */
extern const BbcSprite SPR_CHUCK_DRAG2;
extern const BbcSprite SPR_CHUCK_HACK1;
extern const BbcSprite SPR_CHUCK_HACK2;
/* From behind; mirror it for the other half of the stroke. */
extern const BbcSprite SPR_CHUCK_CLIMB;
/* On a rung, from behind, one hand on the rail and the other at work: out
 * to the side (faces right; mirror it), straight up the shaft, and straight
 * down it. The working hand is at (7, 6), (6, 0) and (7, 11). */
extern const BbcSprite SPR_CHUCK_CLIMB_AIM;
extern const BbcSprite SPR_CHUCK_CLIMB_AIM_UP;
extern const BbcSprite SPR_CHUCK_CLIMB_AIM_DOWN;
/* Nine rows tall: the crawl box is eighteen pixels. */
extern const BbcSprite SPR_CHUCK_CRAWL1;
extern const BbcSprite SPR_CHUCK_CRAWL2;
/* Four rows, lying where he fell. */
extern const BbcSprite SPR_CHUCK_DEAD;
/* Ten by five, laid with SPR_CHUCK_SHOULDER, its left edge at the sprite's
 * column CHUCK_BAZOOKA_COL and its top at row CHUCK_BAZOOKA_ROW. Drawn first
 * and the man over it, the tube passes behind his head, which is how the
 * sector draws it; laid over him it crosses his jaw. */
extern const BbcSprite SPR_BAZOOKA;
#define CHUCK_BAZOOKA_COL (-1)
#define CHUCK_BAZOOKA_ROW 3
/* The tube stood on end for a shot up or down a ladder shaft, four by
 * eleven, mouth at the top and at the bottom. */
extern const BbcSprite SPR_BAZOOKA_UP;
extern const BbcSprite SPR_BAZOOKA_DOWN;

/* ---- Guards ------------------------------------------------------------ */

/* Slots: '1' uniform, '2' helmet, '3' trousers and boots, '4' webbing. */
extern const BbcSprite SPR_GUARD_STAND;
/* A patrol is WALK1, STAND, WALK2, STAND. */
extern const BbcSprite SPR_GUARD_WALK1;
extern const BbcSprite SPR_GUARD_WALK2;
extern const BbcSprite SPR_GUARD_AIM;
extern const BbcSprite SPR_GUARD_AIM_UP;
extern const BbcSprite SPR_GUARD_AIM_DOWN;
extern const BbcSprite SPR_GUARD_TALK;
/* The other half of the gesture. */
extern const BbcSprite SPR_GUARD_TALK2;
/* On the crew's net, handset at the shoulder and the whip up. */
extern const BbcSprite SPR_GUARD_RADIO;
extern const BbcSprite SPR_GUARD_REACH;
extern const BbcSprite SPR_GUARD_DAZZLED;
extern const BbcSprite SPR_GUARD_CLIMB;
/* Nine by four, on his back. */
extern const BbcSprite SPR_GUARD_DOWN;
/* The heavy's pauldrons, laid on any guard frame at row 5 — under it or over
 * it alike, since every pixel is outside a guard's torso: '2' is the plate. */
extern const BbcSprite SPR_HEAVY_PLATE;
#define HEAVY_PLATE_ROW 5
/* And his full helmet, laid *under* a side-on guard frame at row 0 so the
 * guard's own visor slit stays on top of it. */
extern const BbcSprite SPR_HEAVY_HELM;

/* ---- The dog ----------------------------------------------------------- */

/* Eight by eight over a 24x16 box. Slots: '1' coat, '2' nose. */
extern const BbcSprite SPR_DOG_STAND;
extern const BbcSprite SPR_DOG_RUN1;
extern const BbcSprite SPR_DOG_RUN2;
extern const BbcSprite SPR_DOG_BITE;
/* The windup before a bite: crouched, head low, teeth out. */
extern const BbcSprite SPR_DOG_GROWL;
extern const BbcSprite SPR_DOG_DOWN;

/* ---- Staff ------------------------------------------------------------- */

extern const BbcSprite SPR_JANITOR_WALK1;
extern const BbcSprite SPR_JANITOR_WALK2;
extern const BbcSprite SPR_JANITOR_PAUSE;
/* Working: the mop swung out and drawn back. */
extern const BbcSprite SPR_JANITOR_MOP1;
extern const BbcSprite SPR_JANITOR_MOP2;
/* His cart, six by twelve, push bar on the right: with the mop standing in
 * the bucket while he walks, and without it while the mop is in his hands. */
extern const BbcSprite SPR_JANITOR_CART;
extern const BbcSprite SPR_JANITOR_CART_EMPTY;

/* Seven columns over a 24-pixel box. Slots: '1' top, '2' legs, '3' hair,
 * '4' a visitor's case. STAND is the startle, hands up at the face. */
extern const BbcSprite SPR_CIVILIAN_STAND;
extern const BbcSprite SPR_CIVILIAN_RUN1;
extern const BbcSprite SPR_CIVILIAN_RUN2;
/* The visitor who runs with his case. */
extern const BbcSprite SPR_CIVILIAN_CASE1;
extern const BbcSprite SPR_CIVILIAN_CASE2;
/* Down on one knee where the run tripped. */
extern const BbcSprite SPR_CIVILIAN_STUMBLE;

/* Waist up, nine rows: the counter in front of her is the world's. Same
 * slots as a civilian. */
extern const BbcSprite SPR_RECEPTIONIST_IDLE;
extern const BbcSprite SPR_RECEPTIONIST_PHONE;
extern const BbcSprite SPR_RECEPTIONIST_ALARM;
/* The whole of her, seven by sixteen, for the floor: standing to the post,
 * walking an errand with the folder tucked, and reading it out. */
extern const BbcSprite SPR_RECEPTIONIST_STAND;
extern const BbcSprite SPR_RECEPTIONIST_WALK1;
extern const BbcSprite SPR_RECEPTIONIST_WALK2;
extern const BbcSprite SPR_RECEPTIONIST_READ;

/* ---- Birds ------------------------------------------------------------- */

extern const BbcSprite SPR_BIRD_UP;
extern const BbcSprite SPR_BIRD_DOWN;

/* ---- What comes off the wall at a climber ------------------------------ */

/* Four by seven over the 14-pixel box, upright and on its side. */
extern const BbcSprite SPR_THROWN_POT;
extern const BbcSprite SPR_THROWN_POT_SIDE;
extern const BbcSprite SPR_THROWN_BOTTLE;
extern const BbcSprite SPR_THROWN_BOTTLE_SIDE;
extern const BbcSprite SPR_THROWN_BRICK;
extern const BbcSprite SPR_THROWN_BRICK_SIDE;

/* ---- The story --------------------------------------------------------- */

extern const BbcSprite SPR_ELLEN_STAND;
extern const BbcSprite SPR_ELLEN_WALK1;
extern const BbcSprite SPR_ELLEN_WALK2;
extern const BbcSprite SPR_ELLEN_BOUND1;
extern const BbcSprite SPR_ELLEN_BOUND2;

extern const BbcSprite SPR_VOSS_STAND;
extern const BbcSprite SPR_VOSS_WALK;
extern const BbcSprite SPR_VOSS_AIM;

/* The crew at the kerb. Slots: '1' fatigues, '2' balaclava. */
extern const BbcSprite SPR_AGENT_STAND;
extern const BbcSprite SPR_AGENT_WALK;
extern const BbcSprite SPR_AGENT_AIM;

/* ---- What is carried, picked up and counted ---------------------------- */

extern const BbcSprite SPR_ICON_HEART;
extern const BbcSprite SPR_ICON_HEART_EMPTY;
extern const BbcSprite SPR_ICON_ROUND;
extern const BbcSprite SPR_ICON_ROUND_SPENT;
extern const BbcSprite SPR_ICON_CARD;
extern const BbcSprite SPR_ICON_MEDKIT;
extern const BbcSprite SPR_ICON_GRENADE;
extern const BbcSprite SPR_ICON_FLASH;
extern const BbcSprite SPR_ICON_ROCKET;
extern const BbcSprite SPR_ICON_DECOY;
extern const BbcSprite SPR_ICON_EVIDENCE;
extern const BbcSprite SPR_ICON_PISTOL;
extern const BbcSprite SPR_ICON_BAZOOKA;
/* Slot '1' is the round's colour: yellow for Chuck's, red for theirs. */
extern const BbcSprite SPR_SHOT;
extern const BbcSprite SPR_MUZZLE_FLASH;
/* The flashes of a shot in play, five rows tall so they read as a star at
 * 1x: a side-on burst four by five facing right (Chuck's), the same with the
 * enemy's red at its points (a rifle's), and a symmetrical three by five star
 * for a shot straight up or down. */
extern const BbcSprite SPR_MUZZLE_BURST;
extern const BbcSprite SPR_MUZZLE_FLASH_RIFLE;
extern const BbcSprite SPR_MUZZLE_STAR;

/* ---- The standard paints ----------------------------------------------- */

/*
 * The slot lists the cast is drawn in. They are here rather than in the
 * renderers for the reason the sprites are: the guard on the manual's sheet
 * and the guard on the floor have to be the same man.
 */
extern const BbcColour BBC_GUARD_PAINT[BBC_SPRITE_SLOTS];
/* A guard who has taken a round: the uniform goes yellow, so a wounded man is
 * told from a fresh one across the room. */
extern const BbcColour BBC_GUARD_HURT_PAINT[BBC_SPRITE_SLOTS];
extern const BbcColour BBC_HEAVY_PAINT[BBC_SPRITE_SLOTS];
extern const BbcColour BBC_HEAVY_HURT_PAINT[BBC_SPRITE_SLOTS];
extern const BbcColour BBC_DOG_PAINT[BBC_SPRITE_SLOTS];
extern const BbcColour BBC_AGENT_PAINT[BBC_SPRITE_SLOTS];
/* Staff and visitors, by variant: a crowd is several of these. */
#define BBC_CIVILIAN_PAINTS 4
extern const BbcColour BBC_CIVILIAN_PAINT[BBC_CIVILIAN_PAINTS][BBC_SPRITE_SLOTS];
/* The round in flight. */
extern const BbcColour BBC_PLAYER_SHOT_PAINT[BBC_SPRITE_SLOTS];
extern const BbcColour BBC_ENEMY_SHOT_PAINT[BBC_SPRITE_SLOTS];

/* Every sprite above, for the suite. */
extern const BbcSprite *const BBC_CAST_ALL[];
extern const int BBC_CAST_COUNT;

#endif /* CHUCK_BBC_CAST_H */
