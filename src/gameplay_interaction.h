#ifndef CHUCK_GAMEPLAY_INTERACTION_H
#define CHUCK_GAMEPLAY_INTERACTION_H

#include "gameplay_state.h"

typedef enum
{
    SUBLEVEL_DOOR_NONE = 0,
    SUBLEVEL_DOOR_ENTER,
    SUBLEVEL_DOOR_RETURN
} SublevelDoorAction;

void gameplay_prepare_terminal(GameplayState *state, const Input *input,
                               float dt);
bool gameplay_advance_terminal(GameplayState *state,
                               CampaignState *campaign, float dt);
int gameplay_player_door_index(const GameplayState *state);
void gameplay_use_door(GameplayState *state, Input *input);
SublevelDoorAction gameplay_player_sublevel_door_action(
    const GameplayState *state);
SublevelDoorAction gameplay_use_sublevel_door(GameplayState *state,
                                              Input *input);
void gameplay_collect_items(GameplayState *state, CampaignState *campaign,
                            float dt);
bool gameplay_player_reached_exit(const GameplayState *state);
int gameplay_neutralized_hostiles(const GameplayState *state);

#endif /* CHUCK_GAMEPLAY_INTERACTION_H */
