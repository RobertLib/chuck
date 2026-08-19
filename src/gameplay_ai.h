#ifndef CHUCK_GAMEPLAY_AI_H
#define CHUCK_GAMEPLAY_AI_H

#include "gameplay_state.h"

void gameplay_ai_spawn_level_entities(GameplayState *state);
void gameplay_ai_update_spawns(GameplayState *state, float dt);
void gameplay_ai_update_movement(GameplayState *state, float dt);
void gameplay_ai_update_combat(GameplayState *state, float dt);

/*
 * Send one guard to one wall switch, the way the floor's own two decisions do.
 *
 * Public because a hand-set `raising_alarm` is not a commitment. The run
 * carries a budget derived from the distance and the speed he will travel at
 * (`ALARM_RUN_DETOUR_ALLOWANCE`), and only the commit knows how to compute it,
 * so a caller that set the flag itself was staging a state the game never
 * produces — and then measuring it.
 */
void gameplay_ai_send_to_alarm(GameplayState *state, int enemy_index,
                               int switch_index);

/*
 * Where a ceiling camera's beam is pointing, in radians off straight down.
 *
 * Public because the renderer has to draw the very cone the simulation is
 * testing against, and a second copy of the sweep — even a correct one — would
 * be a picture free to disagree with the rule about where the player is safe.
 * It is a pure function of the camera's own clock and holds no state.
 */
float gameplay_camera_angle(float sweep);

#endif /* CHUCK_GAMEPLAY_AI_H */
