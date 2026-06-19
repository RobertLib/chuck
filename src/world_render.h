#ifndef CHUCK_WORLD_RENDER_H
#define CHUCK_WORLD_RENDER_H

/*
 * The sector as a place. See [world_render.c](world_render.c).
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "game.h"

/* The whole world layer of a sector or a climb: backdrop, masonry, fittings,
 * pickups, hazards, the cast and the effects over them. Everything under the
 * HUD. */
void render_world(Game *game);

#endif /* CHUCK_WORLD_RENDER_H */
