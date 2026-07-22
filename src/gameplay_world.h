#ifndef CHUCK_GAMEPLAY_WORLD_H
#define CHUCK_GAMEPLAY_WORLD_H

#include "gameplay_state.h"

bool gameplay_boxes_overlap(float ax, float ay, float aw, float ah,
                            float bx, float by, float bw, float bh);
void gameplay_world_sound(GameplayState *state, SoundEffect effect,
                          float x, float y);
void gameplay_hit_player(GameplayState *state);
void gameplay_unlock_exit(GameplayState *state);
bool gameplay_player_near_active_terminal(const GameplayState *state);
void gameplay_provoke_enemy(GameplayState *state, int enemy_index);
void gameplay_destroy_crate(GameplayState *state, CampaignState *campaign,
                            Crate *crate);
void gameplay_kill_enemy_with_crate(GameplayState *state,
                                    CampaignState *campaign, Enemy *enemy);
void gameplay_kill_dog_with_crate(GameplayState *state,
                                  CampaignState *campaign, Dog *dog);

#endif /* CHUCK_GAMEPLAY_WORLD_H */
