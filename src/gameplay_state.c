#include "gameplay_state.h"

#include <string.h>

void gameplay_state_begin_level(GameplayState *state)
{
    Rng rng = state->rng;
    memset(state, 0, sizeof(*state));
    state->rng = rng;
    state->player_on_elevator = -1;
    state->player_on_moving_platform = -1;
    state->active_alarm_switch = -1;
}
