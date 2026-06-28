#ifndef CHUCK_SPRITES_H
#define CHUCK_SPRITES_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/*
 * Sprite-sheet layout.  All cells are SPRITE_CELL (32) pixels wide.
 *
 * assets/tileset.bmp  (192 × 38)
 *   Row 0  y =  0, h = 32  –  32×32 level tiles, left to right:
 *     col 0  WALL
 *     col 1  LADDER
 *     col 2  DOOR
 *     col 3  ELEVATOR SHAFT
 *     col 4  EXIT  (inactive – items still to collect)
 *     col 5  EXIT  (active   – all items collected)
 *   Row 1  y = 32, h =  6  –  32×6 platform sprites:
 *     col 0  ELEVATOR moving platform
 *     col 1  FALLING platform
 *     col 2  MOVING (horizontal) platform
 *
 * assets/player.bmp   (64 × 32)
 *   col 0  standing  – draw your sprite within the first 22×28 px
 *   col 1  crawling  – draw your sprite within the first 22×16 px
 *   Sprite is flipped horizontally for left-facing direction.
 *
 * assets/enemy.bmp    (32 × 32)
 *   Single frame – draw within the first 22×26 px.
 *   Sprite is flipped horizontally when the enemy faces left.
 *
 * assets/items.bmp   (160 × 32)
 *   col 0  CARD key          – draw within 14×18
 *   col 1  GUN / ammo pack   – draw within 15×12
 *   col 2  GRENADE pickup    – draw within 13×14
 *   col 3  MINE              – draw within 16×10
 *   col 4  thrown GRENADE    – draw within 10×10
 */

/* Width of every sprite cell (pixels). */
#define SPRITE_CELL 32

/* Tileset row geometry */
#define SPRITE_TILE_ROW_Y 0  /* y-offset of the 32×32 tile row  */
#define SPRITE_TILE_ROW_H 32 /* height of the tile row           */
#define SPRITE_PLAT_ROW_Y 32 /* y-offset of the 32×6 platform row */

/* Tile column indices (tileset.bmp, row 0) */
#define SPRITE_TILE_WALL 0
#define SPRITE_TILE_LADDER 1
#define SPRITE_TILE_DOOR 2
#define SPRITE_TILE_ELEVATOR_SHAFT 3
#define SPRITE_TILE_EXIT_OFF 4
#define SPRITE_TILE_EXIT_ON 5

/* Platform column indices (tileset.bmp, row 1) */
#define SPRITE_PLAT_ELEV 0
#define SPRITE_PLAT_FALL 1
#define SPRITE_PLAT_MOVE 2

/* Player frame column indices (player.bmp) */
#define SPRITE_PLAYER_STAND 0
#define SPRITE_PLAYER_CRAWL 1

/* Item column indices (items.bmp) */
#define SPRITE_ITEM_CARD 0
#define SPRITE_ITEM_GUN 1
#define SPRITE_ITEM_GRENADE 2
#define SPRITE_ITEM_MINE 3
#define SPRITE_ITEM_THROWN 4
#define SPRITE_ITEM_MEDKIT 5

typedef struct
{
  SDL_Texture *tileset; /* level tiles & platforms; NULL = procedural fallback */
  SDL_Texture *player;  /* player frames;           NULL = procedural fallback */
  SDL_Texture *enemy;   /* enemy frame;             NULL = procedural fallback */
  SDL_Texture *items;   /* items, mine, grenade;    NULL = procedural fallback */
} Sprites;

/*
 * Load all sprite sheets from assets/.
 * Missing or unreadable files are logged and leave the pointer NULL; the game
 * then uses procedural rendering for that sheet.  Always returns true.
 */
bool sprites_load(Sprites *s, SDL_Renderer *renderer);
void sprites_free(Sprites *s);

#endif /* CHUCK_SPRITES_H */
