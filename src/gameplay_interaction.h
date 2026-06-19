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
/* Picking up, hauling and putting down a body. Reads the same held button the
 * terminal does and yields to it; see the note in gameplay_interaction.c. */
void gameplay_update_body_drag(GameplayState *state, const Input *input);
/* Whether a body is close enough to take hold of, which is what the on-screen
 * prompt is asking. Same function underneath as the grab, so the two cannot
 * disagree about where it works. */
bool gameplay_body_within_reach(const GameplayState *state);
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
