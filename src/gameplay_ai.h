#ifndef CHUCK_GAMEPLAY_AI_H
#define CHUCK_GAMEPLAY_AI_H

#include "gameplay_state.h"

void gameplay_ai_spawn_level_entities(GameplayState *state);
void gameplay_ai_update_spawns(GameplayState *state, float dt);
void gameplay_ai_update_movement(GameplayState *state, float dt);
void gameplay_ai_update_combat(GameplayState *state, float dt);

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
