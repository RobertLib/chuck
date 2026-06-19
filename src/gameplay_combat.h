#ifndef CHUCK_GAMEPLAY_COMBAT_H
#define CHUCK_GAMEPLAY_COMBAT_H

#include "gameplay_state.h"

void gameplay_combat_update_explosives(GameplayState *state,
                                       CampaignState *campaign, float dt);
/* Bolts in the air, and the throw cooldown that limits them. Its own update
 * rather than a branch of the explosives one above: nothing here explodes,
 * chains, scores or damages anybody, and folding it in would put a
 * `CampaignState` in the signature of something that can never touch one. */
void gameplay_combat_update_decoys(GameplayState *state, float dt);
void gameplay_combat_handle_player_action(GameplayState *state,
                                          CampaignState *campaign,
                                          Input *input);
void gameplay_combat_update_hazards(GameplayState *state);
void gameplay_combat_update_player_bullets(GameplayState *state,
                                           CampaignState *campaign,
                                           float dt);
/* Takes the campaign for the same reason the player's rounds do: a guard's
 * round can set off a gas canister, and a blast scores. */
void gameplay_combat_update_enemy_bullets(GameplayState *state,
                                          CampaignState *campaign, float dt);
void gameplay_combat_check_contacts(GameplayState *state,
                                    CampaignState *campaign);

#endif /* CHUCK_GAMEPLAY_COMBAT_H */
