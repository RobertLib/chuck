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
/* Unconditional kill: the physics deaths (fatal fall, elevator crush) and the
 * moment the last heart goes. Ordinary hits go through gameplay_damage_player.
 */
void gameplay_hit_player(GameplayState *state);
/* Ordinary damage: costs hearts, grants a mercy window, pops the player away
 * from the source, and only kills when the hearts run out. Ignored while the
 * mercy window is open. */
void gameplay_damage_player(GameplayState *state, int amount,
                            float source_x, float source_y);
/* Interior checkpoints: banked at real progress, restored on respawn. The
 * facade climb keeps its own banking; restore handles both modes. */
void gameplay_bank_checkpoint(GameplayState *state);
void gameplay_restore_checkpoint(GameplayState *state);
/* Ammunition drops from guards downed in direct combat. */
void gameplay_spawn_ammo_drop(GameplayState *state, float x, float y);
void gameplay_update_ammo_drops(GameplayState *state, float dt);
void gameplay_handle_player_landing(GameplayState *state, bool was_grounded,
                                    float fall_speed);
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
/* Open every weak wall a blast reaches. Only explosions call this: a pistol
 * round or a knife leaves a blocked-up opening exactly where it was, so the
 * route through a wall always costs an explosive. Returns how many tiles went.
 */
int gameplay_break_walls_in_radius(GameplayState *state,
                                   CampaignState *campaign,
                                   float x, float y, float radius);
void gameplay_kill_enemy_with_crate(GameplayState *state,
                                    CampaignState *campaign, Enemy *enemy);
void gameplay_kill_dog_with_crate(GameplayState *state,
                                  CampaignState *campaign, Dog *dog);
/* One more hostile off the floor, however it went down. Every path that sets a
 * `dead` flag calls this, because the flags themselves are the living
 * population and a reinforcement reusing a downed guard's slot would otherwise
 * un-count the kill that emptied it. */
void gameplay_record_neutralized(GameplayState *state);

#endif /* CHUCK_GAMEPLAY_WORLD_H */
