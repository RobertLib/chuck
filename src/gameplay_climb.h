#ifndef CHUCK_GAMEPLAY_CLIMB_H
#define CHUCK_GAMEPLAY_CLIMB_H

#include "gameplay_state.h"

/* Seed the per-source facade timers. Update also calls this lazily so custom
 * maps driven directly by tests do not need shell-level setup. */
void gameplay_climb_init(GameplayState *state);

/* Direct four-way movement while Chuck is gripping the outside wall. This is
 * deliberately independent of tile physics, gravity and ladder rules, but
 * masonry (ledges, air-conditioning units) does block him. */
void gameplay_climb_update_player(GameplayState *state, const Input *input,
                                  float dt);

/* Signed sideways force of the current gust, zero while it is not blowing.
 * The renderer and HUD read it so wind is shown exactly when it is felt. */
float gameplay_climb_wind_push(const GameplayState *state);

/* Put the climber back on the highest banked position instead of at the foot
 * of the wall. No-op outside facade levels or before anything is banked. */
void gameplay_climb_restore_checkpoint(GameplayState *state);

/* Spawn and advance thrown objects and birds on exterior climbing maps. */
void gameplay_climb_update(GameplayState *state, float dt);

#endif /* CHUCK_GAMEPLAY_CLIMB_H */
