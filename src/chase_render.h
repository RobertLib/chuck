#ifndef CHUCK_CHASE_RENDER_H
#define CHUCK_CHASE_RENDER_H

#include "chase.h"
#include "common.h"
#include "pad_hint.h"

/*
 * Draws the prologue pursuit, HUD included, entirely through bbc.h: expects
 * `bbc_begin_frame` to have been called for the frame. The shake offsets come
 * from the shell's camera shake and move the road only, so the readouts stay
 * readable while the car is being hit. `steady_lights` is the player's
 * reduced-motion switch: the cordon's bars flash for the whole minute the drive
 * lasts, and hold still under it through `bbc_flash`, and the switch is carried
 * down to the few glints here that move on a clock of their own.
 */
void chase_render(SDL_Renderer *renderer, const Chase *chase,
                  int win_w, int win_h, float shake_x, float shake_y,
                  bool steady_lights, const PadHints *pad);

#endif /* CHUCK_CHASE_RENDER_H */
