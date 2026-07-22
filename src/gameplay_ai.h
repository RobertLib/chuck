#ifndef CHUCK_GAMEPLAY_AI_H
#define CHUCK_GAMEPLAY_AI_H

#include "gameplay_state.h"

void gameplay_ai_spawn_level_entities(GameplayState *state);
void gameplay_ai_update_spawns(GameplayState *state, float dt);
void gameplay_ai_update_movement(GameplayState *state, float dt);
void gameplay_ai_update_combat(GameplayState *state, float dt);

#endif /* CHUCK_GAMEPLAY_AI_H */
