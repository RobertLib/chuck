#ifndef CHUCK_GAMEPLAY_COMBAT_H
#define CHUCK_GAMEPLAY_COMBAT_H

#include "gameplay_state.h"

void gameplay_combat_update_explosives(GameplayState *state,
                                       CampaignState *campaign, float dt);
void gameplay_combat_handle_player_action(GameplayState *state,
                                          CampaignState *campaign,
                                          Input *input);
void gameplay_combat_update_hazards(GameplayState *state);
void gameplay_combat_update_player_bullets(GameplayState *state,
                                           CampaignState *campaign,
                                           float dt);
void gameplay_combat_update_enemy_bullets(GameplayState *state, float dt);
void gameplay_combat_check_contacts(GameplayState *state);

#endif /* CHUCK_GAMEPLAY_COMBAT_H */
