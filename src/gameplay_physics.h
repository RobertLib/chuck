#ifndef CHUCK_GAMEPLAY_PHYSICS_H
#define CHUCK_GAMEPLAY_PHYSICS_H

#include "gameplay_state.h"

void gameplay_update_crates(GameplayState *state, CampaignState *campaign,
                            float dt);
void gameplay_resolve_player_crates(GameplayState *state,
                                    float previous_x, float previous_y,
                                    float previous_height);
void gameplay_resolve_enemy_crates(GameplayState *state, Enemy *enemy,
                                   float previous_y);
void gameplay_resolve_dog_crates(GameplayState *state, Dog *dog,
                                 float previous_x, float previous_y);
bool gameplay_crate_blocks_row(const GameplayState *state,
                               float ax, float bx, int row);
/* Whether a box of this size at this position clears the tile map. 'stance' is
 * the posture of whatever is being placed: everything in the building but Chuck
 * on his elbows passes `STANCE_UPRIGHT`, and only a duct answers the two
 * differently. Getting it wrong on the player's own path is not a near miss —
 * the tile a crawler is *inside* is the tile this asks about, so an upright
 * answer says a man in a duct is in masonry. */
bool gameplay_box_tiles_clear(const GameplayState *state,
                              float x, float y, float w, float h,
                              Stance stance);

/* Nudge a rider up with the elevator he stood on last frame, before gravity
 * gets a chance to drop him off it. Runs before the player's own physics. */
void gameplay_carry_player_on_elevator(GameplayState *state, float dt);
/* Anything that moves the player vertically can drive him into a slab. Slide
 * him into whichever neighbouring column has room for him and only kill him
 * when neither does. Returns true if he was crushed. */
bool gameplay_resolve_player_crush(GameplayState *state);
/* Stand the player on an elevator or a moving platform he has come down onto,
 * and carry him along with it. Sets player_on_elevator /
 * player_on_moving_platform, which the shell compares against last frame's to
 * know when a ride starts. Runs after the platforms have moved. */
void gameplay_ride_platforms(GameplayState *state, float dt);

#endif /* CHUCK_GAMEPLAY_PHYSICS_H */
