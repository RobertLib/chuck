#ifndef CHUCK_GAMEPLAY_WORLD_H
#define CHUCK_GAMEPLAY_WORLD_H

#include "gameplay_state.h"

bool gameplay_boxes_overlap(float ax, float ay, float aw, float ah,
                            float bx, float by, float bw, float bh);
/*
 * Whether one world point can see another: sample the segment and report
 * whether solid tiles or crates block it. Ladders and elevator shafts are
 * transparent, because `level_is_solid` already treats them as non-solid, so a
 * guard sees through a ladder and not through a floor. Endpoints are skipped —
 * both entity centres sit inside empty tiles and must not count as
 * self-occlusion.
 *
 * **It is here rather than in gameplay_ai.c because the third caller is not a
 * pair of eyes.** It was a private helper of the perception model for as long as
 * only the guards and the ceiling lens asked the question, and the flash charge
 * — which is a claim about what a room can *see* — reached every man inside
 * `FLASH_RADIUS` on a bare distance test. Five tiles is wider than any partition
 * in the building and wider than a storey is tall, so one charge blinded the
 * room next door and the floors above and below it, which is the opposite of
 * what `FLASH_RADIUS` is written down as meaning. One sight line, or the game
 * has two answers to "can this be seen from here".
 */
bool gameplay_sight_line_clear(const GameplayState *state,
                               float ax, float ay, float bx, float by);
void gameplay_world_sound(GameplayState *state, SoundEffect effect,
                          float x, float y);
/* Somebody on the crew spoke. The simulation reports the kind, the man and the
 * place, and draws one number for the shell to pick a line with; it never
 * holds a word of what was said. See [crew.h](crew.h). */
void gameplay_crew_chatter(GameplayState *state, ChatterKind kind, int speaker,
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
/* The box a camera occupies, for the two things that can hit one. Its own
 * function because a camera is a map position rather than an entity with a
 * rect, and three callers would otherwise each write the same arithmetic. */
void gameplay_camera_box(const SecurityCamera *camera, float *x, float *y,
                         float *w, float *h);
/* Take a camera off the ceiling. Loud, permanent for the visit, and worth
 * points — see the note in gameplay_world.c. Silently ignores an index that is
 * out of range or a lens that has already gone. */
void gameplay_destroy_camera(GameplayState *state, CampaignState *campaign,
                             int index);
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
 * un-count the kill that emptied it.
 *
 * Two counters, answering two questions: the sector's tally is what the report
 * between floors prints and is wiped with the sector, while the run's — in
 * `CampaignState` — is what the crew's net reads, and a crew does not get its
 * men back because the player opened a stair door. Both are bumped here rather
 * than at the six kill sites, which is the whole reason this exists. */
void gameplay_record_neutralized(GameplayState *state,
                                 CampaignState *campaign);

#endif /* CHUCK_GAMEPLAY_WORLD_H */
