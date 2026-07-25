#ifndef CHUCK_CHASE_RENDER_H
#define CHUCK_CHASE_RENDER_H

#include "chase.h"
#include "common.h"

/*
 * Draws the prologue pursuit, HUD included. The shake offsets come from the
 * shell's camera shake and move the road only, so the readouts stay readable
 * while the car is being hit.
 */
void chase_render(SDL_Renderer *renderer, const Chase *chase,
                  int win_w, int win_h, float shake_x, float shake_y);

#endif /* CHUCK_CHASE_RENDER_H */
