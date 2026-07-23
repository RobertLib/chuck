#ifndef CHUCK_GAMEPLAY_WORLD_H
#define CHUCK_GAMEPLAY_WORLD_H

#include "gameplay_state.h"

bool gameplay_boxes_overlap(float ax, float ay, float aw, float ah,
                            float bx, float by, float bw, float bh);
void gameplay_world_sound(GameplayState *state, SoundEffect effect,
                          float x, float y);
bool gameplay_alarm_active(const GameplayState *state);
void gameplay_trigger_alarm(GameplayState *state, float source_x,
                            float source_y, int switch_index);
void gameplay_refresh_alarm_from_player(GameplayState *state);
void gameplay_update_alarm(GameplayState *state, float dt);
void gameplay_hit_player(GameplayState *state);
void gameplay_unlock_exit(GameplayState *state);
bool gameplay_player_near_active_terminal(const GameplayState *state);
void gameplay_provoke_enemy(GameplayState *state, int enemy_index);
/* A loud disturbance (gunfire, an explosion) draws nearby calm guards to walk
 * over and investigate its origin. Guards already fighting or raising the alarm
 * are unaffected. */
void gameplay_alert_enemies_to_noise(GameplayState *state, float x, float y,
                                     float radius);
void gameplay_destroy_crate(GameplayState *state, CampaignState *campaign,
                            Crate *crate);
void gameplay_kill_enemy_with_crate(GameplayState *state,
                                    CampaignState *campaign, Enemy *enemy);
void gameplay_kill_dog_with_crate(GameplayState *state,
                                  CampaignState *campaign, Dog *dog);

#endif /* CHUCK_GAMEPLAY_WORLD_H */
