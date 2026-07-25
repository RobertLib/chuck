#ifndef CHUCK_GAMEPLAY_CLIMB_H
#define CHUCK_GAMEPLAY_CLIMB_H

#include "gameplay_state.h"

/* Seed the per-source facade timers. Update also calls this lazily so custom
 * maps driven directly by tests do not need shell-level setup. */
void gameplay_climb_init(GameplayState *state);

/* Direct four-way movement while Chuck is gripping the outside wall. This is
 * deliberately independent of tile physics, gravity and ladder rules. */
void gameplay_climb_update_player(GameplayState *state, const Input *input,
                                  float dt);

/* Spawn and advance thrown objects and birds on exterior climbing maps. */
void gameplay_climb_update(GameplayState *state, float dt);

#endif /* CHUCK_GAMEPLAY_CLIMB_H */
