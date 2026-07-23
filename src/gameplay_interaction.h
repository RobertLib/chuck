#ifndef CHUCK_GAMEPLAY_INTERACTION_H
#define CHUCK_GAMEPLAY_INTERACTION_H

#include "gameplay_state.h"

void gameplay_prepare_terminal(GameplayState *state, const Input *input,
                               float dt);
bool gameplay_advance_terminal(GameplayState *state,
                               CampaignState *campaign, float dt);
int gameplay_player_door_index(const GameplayState *state);
void gameplay_use_door(GameplayState *state, Input *input);
void gameplay_collect_items(GameplayState *state, CampaignState *campaign,
                            float dt);
bool gameplay_player_reached_exit(const GameplayState *state);
int gameplay_neutralized_hostiles(const GameplayState *state);

#endif /* CHUCK_GAMEPLAY_INTERACTION_H */
