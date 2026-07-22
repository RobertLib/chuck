#ifndef CHUCK_GAMEPLAY_PHYSICS_H
#define CHUCK_GAMEPLAY_PHYSICS_H

#include "gameplay_state.h"

void gameplay_update_crates(GameplayState *state, CampaignState *campaign,
                            float dt);
void gameplay_resolve_player_crates(GameplayState *state,
                                    float previous_x, float previous_y,
                                    float previous_height);
void gameplay_resolve_enemy_crates(GameplayState *state, Enemy *enemy,
                                   float previous_x, float previous_y);
void gameplay_resolve_dog_crates(GameplayState *state, Dog *dog,
                                 float previous_x, float previous_y);
bool gameplay_crate_blocks_row(const GameplayState *state,
                               float ax, float bx, int row);
bool gameplay_box_tiles_clear(const GameplayState *state,
                              float x, float y, float w, float h);

#endif /* CHUCK_GAMEPLAY_PHYSICS_H */
