#include "gameplay_interaction.h"

#include "gameplay_physics.h"
#include "gameplay_world.h"

#include <math.h>

static float reinforcement_delay(Rng *rng, float minimum, float maximum)
{
    return minimum + (maximum - minimum) * rng_unit(rng);
}

/*
 * Standing at the terminal with the button held, and what that costs.
 *
 * **The reinforcements belong to the terminal, not to the alarm.** They used to
 * be queued off `alarm_started` — the frame the hack put the alarm up — which
 * quietly made them conditional on the building having been quiet beforehand: a
 * player who had already been seen, whose alarm was already ringing, hacked the
 * same terminal and nobody came out of the doors at all. That is the incentive
 * exactly backwards. The manual sells the console as the loud way through
 * ("Hacking wakes the building: guards are sent to the terminal you used"), and
 * a rule that waives the cost for the player who was *caught first* rewards
 * getting spotted on the three sectors that have both a terminal and a door
 * pair to send anyone out of — 4, 8 and 14. Nothing on screen explained it,
 * because there was nothing to explain: it was the gate reading the alarm when
 * what it meant to read was the hack.
 *
 * So it is the *start of a hack* that calls them, and `was_hacking` is the only
 * thing needed to see one: `terminal_hacking` is recomputed every frame, so the
 * value it is about to lose is the previous frame's answer. Two guards keep it
 * honest. A batch already on its way is not doubled, so tapping the button does
 * not stack them up — and since `gameplay_update_alarm` clears the pending
 * count when the alarm lapses, that guard can never wedge permanently. And the
 * alarm itself is still raised only when it was not already up, because
 * `gameplay_trigger_alarm` is what makes the whole floor turn round and doing
 * that twice is not a second event.
 */
void gameplay_prepare_terminal(GameplayState *state, const Input *input,
                               float dt)
{
    bool was_hacking = state->terminal_hacking;
    state->terminal_in_range = gameplay_player_near_active_terminal(state);
    state->terminal_hacking = state->terminal_in_range &&
                              input->interact &&
                              state->player.on_ground &&
                              !state->player.on_ladder;
    bool alarm_started = state->terminal_hacking &&
                         !gameplay_alarm_active(state);
    bool hack_began = state->terminal_hacking && !was_hacking;
    if (state->terminal_hacking)
    {
        const Terminal *terminal =
            &state->level.map.terminals[state->level.runtime.active_terminal_index];
        state->player.x = terminal->col * (float)TILE_SIZE +
                          ((float)TILE_SIZE - PLAYER_W) * 0.5f;
        if (alarm_started)
        {
            gameplay_trigger_alarm(state,
                                   (terminal->col + 0.5f) * TILE_SIZE,
                                   (terminal->row + 0.5f) * TILE_SIZE, -1);
        }
        /* Held after the trigger above, which sets the same two values: while
         * the hack runs, the floor converges on the console rather than on
         * wherever Chuck was last seen, however the alarm came to be up. */
        state->terminal_alarm_timer = ALARM_CALM_TIME;
        state->alarm_target_x = (terminal->col + 0.5f) * TILE_SIZE;
        state->alarm_target_y = (terminal->row + 0.5f) * TILE_SIZE;
        if (hack_began && state->level.map.door_count > 0 &&
            state->terminal_reinforcements_pending <= 0)
        {
            int count_range = TERMINAL_REINFORCEMENT_MAX_COUNT -
                              TERMINAL_REINFORCEMENT_MIN_COUNT + 1;
            state->terminal_reinforcements_pending =
                TERMINAL_REINFORCEMENT_MIN_COUNT +
                rng_range(&state->rng, count_range);
            state->terminal_reinforcement_timer =
                reinforcement_delay(&state->rng,
                                    TERMINAL_REINFORCEMENT_FIRST_MIN,
                                    TERMINAL_REINFORCEMENT_FIRST_MAX);
        }
    }
    else
    {
        state->terminal_hack_progress = 0.0f;
        state->terminal_hack_tick_timer = 0.0f;
    }
    (void)dt;
}

bool gameplay_advance_terminal(GameplayState *state,
                               CampaignState *campaign, float dt)
{
    if (!state->terminal_hacking)
        return false;

    state->terminal_hack_progress += dt;
    state->terminal_hack_tick_timer -= dt;
    if (state->terminal_hack_tick_timer <= 0.0f)
    {
        game_events_sound(&state->events, SFX_CARD_SCAN);
        state->terminal_hack_tick_timer = 0.45f;
    }
    if (state->terminal_hack_progress < TERMINAL_HACK_TIME)
        return false;

    state->terminal_hack_progress = TERMINAL_HACK_TIME;
    state->level.runtime.terminal_hacked = true;
    campaign->score += TERMINAL_SCORE;
    /* A finished hack is real progress: a later death resumes here. */
    gameplay_bank_checkpoint(state);
    bool was_unlocked = state->level.runtime.exit_unlocked;
    gameplay_unlock_exit(state);
    if (!was_unlocked && state->level.runtime.exit_unlocked)
        return true;

    /*
     * **A finished hack always answers, the same way the live card does.**
     *
     * The return value is "did this open the door", and the shell spends it on
     * the banner and on `SFX_EXIT_UNLOCKED`. On a sector whose stair core is
     * welded that is false, and it was the whole of the feedback: the prompt
     * reads `BREACHING SECURITY... 97%` and then vanishes mid-count, because
     * `gameplay_player_near_active_terminal` stops answering the moment
     * `terminal_hacked` is set. Four seconds of standing still on the busiest
     * floor in the building, ending in the HUD quietly deleting itself.
     *
     * This is the card's bug in the sibling branch. The fix in
     * `gameplay_collect_items` below was written for the pickup alone and
     * reasoned that no shipped map put a card in a window sector — sector 14
     * does, and it puts three consoles there too, so both halves of the same
     * mistake were live on the same floor.
     *
     * `SFX_CARD_TARGET` rather than the `SFX_CARD_SCAN` the progress ticks are
     * made of: the last tick lands 0.4s before this does, so scanning again
     * would read as one more tick rather than as an answer. The sweep's own
     * "that is the one" is the sound the game already owns for a console
     * search that has finished.
     */
    game_events_sound(&state->events, SFX_CARD_TARGET);
    return false;
}

/*
 * Where a body is and how big it is, whichever of the two arrays it came out
 * of. Both halves below need the same four numbers and a guard and a dog are
 * different sizes, so it is asked once here rather than branched at each of
 * them. Returning false for a slot that is not a corpse is the whole of the
 * validation either half does.
 */
static bool body_slot(GameplayState *state, int index, bool is_dog,
                      float **x, float **y, float *w, float *h)
{
    if (is_dog)
    {
        if (index < 0 || index >= state->dog_count ||
            !state->dogs[index].dead)
            return false;
        Dog *dog = &state->dogs[index];
        *x = &dog->x;
        *y = &dog->y;
        *w = DOG_W;
        *h = DOG_H;
        return true;
    }
    if (index < 0 || index >= state->enemy_count ||
        !state->enemies[index].dead)
        return false;
    Enemy *enemy = &state->enemies[index];
    *x = &enemy->x;
    *y = &enemy->y;
    *w = ENEMY_W;
    *h = ENEMY_H;
    return true;
}

/* Everything that lets go, in one place, so no path can drop the body without
 * also clearing the two fields that describe it. */
static void release_body(Player *player)
{
    player->dragging = false;
    player->dragging_body = 0;
    player->dragging_is_dog = false;
    player->drag_side = 0;
}

/*
 * Whether Chuck is in any state to be hauling a dead man about.
 *
 * All five of these are things that would otherwise put the body somewhere it
 * could not be. A ladder and the facade are the obvious two — a corpse does not
 * climb, and letting it follow a climber would hang it in mid-air on a surface
 * `settle_body` deliberately falls through. Crawling is here because
 * a man on his elbows has no spare hand, which is also what keeps the crawl the
 * quicker of the two quiet ways across a floor. Leaving the ground is here
 * because a jump would drag the body up with it. And dying ends it for the
 * reason everything else ends at a death.
 */
static bool player_can_drag(const GameplayState *state)
{
    const Player *player = &state->player;
    return player->on_ground && !player->on_ladder && !player->crawling &&
           !player->facade_climbing && !player->dying;
}

/*
 * The body Chuck could take hold of from where he is standing, or -1.
 *
 * The prompt on screen and the grab itself ask this one function, which is the
 * only way the two can agree: a prompt that appears where the grab then does
 * nothing is worse than no prompt at all, and it is the exact failure this
 * codebase refuses everywhere a button is named. The console's claim on the
 * same held button is part of the question rather than a separate check at each
 * call site, for the same reason.
 *
 * **And so is the doorway's, which is the half that was missing.** `USE` is one
 * physical key read two ways — `interact` while it is down, `use_door` on the
 * press — so standing on a `D` or a `U` with a corpse at his feet, Chuck was
 * told `HOLD E TO DRAG BODY`, and the press grabbed the body *and* walked him
 * through the door: a teleport across the sector, a checkpoint banked, and the
 * corpse dropped back where it lay on the very next frame, because the leash
 * cannot stretch that far. The prompt named one action and the key performed
 * another and more expensive one. The door wins for the same reason the console
 * does — passing through it is a decision the player made, and a body is
 * furniture they happened to stand next to — and a door tile is one tile, so the
 * grab is available from either side of it, exactly as it is beside a terminal.
 *
 * Only *starting* a grab is refused. `gameplay_update_body_drag`'s carrying
 * branch asks `player_can_drag` and not this, so a body already in hand is
 * hauled across a door tile without being dropped; the press that would open the
 * door needs a fresh edge, and holding `USE` is what carrying a body *is*.
 */
static int nearest_body_in_reach(const GameplayState *state, bool *out_is_dog)
{
    if (state->terminal_in_range || !player_can_drag(state))
        return -1;
    if (gameplay_player_door_index(state) >= 0 ||
        gameplay_player_sublevel_door_action(state) != SUBLEVEL_DOOR_NONE)
        return -1;

    float player_x = state->player.x + PLAYER_W * 0.5f;
    float player_y = state->player.y + PLAYER_H * 0.5f;
    int best = -1;
    float best_distance = BODY_DRAG_REACH;

    for (int i = 0; i < state->enemy_count; ++i)
    {
        const Enemy *enemy = &state->enemies[i];
        if (!enemy->dead)
            continue;
        float dx = (enemy->x + ENEMY_W * 0.5f) - player_x;
        float dy = (enemy->y + ENEMY_H * 0.5f) - player_y;
        float distance = sqrtf(dx * dx + dy * dy);
        if (distance < best_distance)
        {
            best_distance = distance;
            best = i;
            *out_is_dog = false;
        }
    }
    for (int i = 0; i < state->dog_count; ++i)
    {
        const Dog *dog = &state->dogs[i];
        if (!dog->dead)
            continue;
        float dx = (dog->x + DOG_W * 0.5f) - player_x;
        float dy = (dog->y + DOG_H * 0.5f) - player_y;
        float distance = sqrtf(dx * dx + dy * dy);
        if (distance < best_distance)
        {
            best_distance = distance;
            best = i;
            *out_is_dog = true;
        }
    }
    return best;
}

/* What the prompt asks, and it asks it of the same function the grab does. */
bool gameplay_body_within_reach(const GameplayState *state)
{
    bool is_dog = false;
    return nearest_body_in_reach(state, &is_dog) >= 0;
}

/*
 * Picking a body up, carrying it, and every way of putting it down.
 *
 * **The whole feature is that the guards already read the floor.**
 * `update_body_discovery` walks the next calm man who sees a corpse over to
 * look at it and often on to the nearest alarm switch — a rule the campaign is
 * balanced around and which the player, up to now, could only pray about, since
 * a patrol route is the one thing about a sector that cannot be read off the
 * map. Nothing in this function tells the AI anything. It moves a body, and the
 * perception model that was already running does the rest: a corpse behind a
 * partition is a corpse `enemy_sees_point` returns false for, and that is the
 * entire mechanic.
 *
 * **It answers the same held button the terminal does, and the terminal wins.**
 * A second binding for "put your hands on the thing in front of you" would be a
 * key the manual has to teach and the options sheet has to carry, for an action
 * that can never be wanted at the same moment as the first — nobody hacks a
 * console while holding a dead man. Where the two do overlap the console takes
 * it, because a hack is a decision the player made and a body is furniture they
 * happened to stand next to.
 */
void gameplay_update_body_drag(GameplayState *state, const Input *input)
{
    Player *player = &state->player;

    if (player->dragging)
    {
        float *bx = NULL;
        float *by = NULL;
        float w = 0.0f;
        float h = 0.0f;
        /* The slot may have stopped being a body since last frame:
         * `find_enemy_slot` hands a corpse's place to a reinforcement when the
         * array is full, and a live guard is not something anybody is holding
         * by the collar. */
        bool still_a_body = body_slot(state, player->dragging_body,
                                      player->dragging_is_dog,
                                      &bx, &by, &w, &h);
        if (!still_a_body || !input->interact || !player_can_drag(state))
        {
            release_body(player);
            return;
        }

        /* Held at a fixed offset on the shoulder it was picked up on, so the
         * body follows the walk exactly rather than being chased toward a
         * target it can never reach. */
        float target_x = player->drag_side > 0
                             ? player->x + BODY_DRAG_OFFSET
                             : player->x - BODY_DRAG_OFFSET;
        /* Walked into a wall with it: the body stays where it is rather than
         * being pushed through the masonry, and the leash below decides how
         * long Chuck gets to keep pulling. */
        if (gameplay_box_tiles_clear(state, target_x, *by, w, h, STANCE_UPRIGHT))
            *bx = target_x;
        /* Only `x` is ever written here, which is why this function takes no
         * `dt` at all. Gravity belongs to `settle_body` in gameplay_ai.c, which
         * already runs over every corpse on the floor once a frame; doing it a
         * second time here would drop the body in Chuck's hands faster than the
         * one lying next to it.
         */
        float reach = fabsf((*bx + w * 0.5f) -
                            (player->x + PLAYER_W * 0.5f));
        if (reach > BODY_DRAG_BREAK)
            release_body(player);
        return;
    }

    /* Not holding anything. A grab needs the button down, a body within arm's
     * reach, and the console to have no claim on the same press. */
    if (!input->interact || !player_can_drag(state))
        return;

    bool best_is_dog = false;
    int best = nearest_body_in_reach(state, &best_is_dog);
    if (best < 0)
        return;

    float *bx = NULL;
    float *by = NULL;
    float w = 0.0f;
    float h = 0.0f;
    if (!body_slot(state, best, best_is_dog, &bx, &by, &w, &h))
        return;

    player->dragging = true;
    player->dragging_body = best;
    player->dragging_is_dog = best_is_dog;
    /* Latched from where the body actually is, not from which way Chuck is
     * looking: he has just walked up to it and is as likely to be facing past
     * it as at it, and a side chosen off `facing` would snap the corpse across
     * him on the first frame. */
    player->drag_side =
        (*bx + w * 0.5f) < (player->x + PLAYER_W * 0.5f) ? -1 : 1;
    gameplay_world_sound(state, SFX_CRATE_PUSH, *bx + w * 0.5f,
                         *by + h * 0.5f);
}

int gameplay_player_door_index(const GameplayState *state)
{
    if (!state->player.on_ground || state->teleport_cooldown > 0.0f)
        return -1;

    int center_col = (int)floorf((state->player.x + PLAYER_W * 0.5f) /
                                 TILE_SIZE);
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    int center_row = (int)floorf((state->player.y + player_h * 0.5f) /
                                 TILE_SIZE);
    for (int index = 0; index < state->level.map.door_count; ++index)
    {
        const Door *door = &state->level.map.doors[index];
        if (door->col == center_col && door->row == center_row)
            return index;
    }

    return -1;
}

SublevelDoorAction gameplay_player_sublevel_door_action(
    const GameplayState *state)
{
    if (!state->player.on_ground || state->teleport_cooldown > 0.0f)
        return SUBLEVEL_DOOR_NONE;

    int center_col = (int)floorf((state->player.x + PLAYER_W * 0.5f) /
                                 TILE_SIZE);
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    int center_row = (int)floorf((state->player.y + player_h * 0.5f) /
                                 TILE_SIZE);

    if (state->level.map.has_sublevel_entrance &&
        center_col == state->level.map.sublevel_entrance_col &&
        center_row == state->level.map.sublevel_entrance_row)
    {
        return SUBLEVEL_DOOR_ENTER;
    }
    if (state->level.map.has_sublevel_return &&
        center_col == state->level.map.sublevel_return_col &&
        center_row == state->level.map.sublevel_return_row)
    {
        return SUBLEVEL_DOOR_RETURN;
    }
    return SUBLEVEL_DOOR_NONE;
}

SublevelDoorAction gameplay_use_sublevel_door(GameplayState *state,
                                              Input *input)
{
    SublevelDoorAction action =
        gameplay_player_sublevel_door_action(state);
    if (!input->use_door || action == SUBLEVEL_DOOR_NONE)
        return SUBLEVEL_DOOR_NONE;

    input->use_door = false;
    return action;
}

void gameplay_use_door(GameplayState *state, Input *input)
{
    int index = gameplay_player_door_index(state);
    if (!input->use_door || index < 0)
    {
        input->use_door = false;
        return;
    }

    int pair = index ^ 1;
    if (pair < state->level.map.door_count)
    {
        const Door *destination = &state->level.map.doors[pair];
        float player_h = state->player.crawling
                             ? (float)PLAYER_CRAWL_H
                             : (float)PLAYER_H;
        state->player.x = destination->col * TILE_SIZE +
                          (TILE_SIZE - PLAYER_W) * 0.5f;
        state->player.y = (destination->row + 1) * TILE_SIZE - player_h;
        state->player.vx = 0.0f;
        state->player.vy = 0.0f;
        state->teleport_cooldown = TELEPORT_COOLDOWN;
        /* Passing a door banks the arrival side, so a death on the new
         * storey does not replay the trip to the door. */
        gameplay_bank_checkpoint(state);
        game_events_sound(&state->events, SFX_DOOR);
    }
    else
    {
        /* Doors are matched 0<->1, 2<->3, so an odd number of them leaves the
         * last one with nowhere to go. The editor calls that an error and no
         * shipped sector has it, but a press that does nothing at all still has
         * to say so: it is the same rule the dry clip and the busy launcher
         * keep, and a silent door reads as the key having missed rather than as
         * the door being dead. */
        game_events_sound(&state->events, SFX_EMPTY_CLICK);
    }
    input->use_door = false;
}

/*
 * A pickup that would change nothing is left where it is.
 *
 * This is the rule `gameplay_update_ammo_drops` already keeps — *"Left lying
 * until it is actually useful, so a full magazine does not eat the pickup"* —
 * said for the boxes on the floor, and it was missing from all three of the
 * ones that cannot come back. Walking over a second `N` while already carrying
 * a grenade set `collected` with a nought respawn timer and played
 * `SFX_PICKUP_GRENADE`: the scarcest thing in the sector destroyed by crossing
 * a tile, announced with the sound of a successful pickup. Sector 12 carries
 * two grenades, sectors 10, 12, 16 and 17 two medkits apiece, and every restroom
 * hands out the grenade the campaign's own budget is balanced on — so the case
 * is not a corner, it is the middle of four maps.
 * (That list read `10, 12 and 15` for a while, and 15 is a facade carrying one
 * medkit — the same sentence `docs/gameplay.md` gets right because
 * [../tools/check_docs.py](../tools/check_docs.py) holds it there. A sector list
 * in a comment is prose too, so this file is on that script's list now.)
 *
 * The boxed magazine is deliberately *not* on this list, and the difference is
 * the respawn: `ITEM_GUN` comes back on `ITEM_RESPAWN_TIME`, so taking one with
 * a full clip costs the player nothing and the box is there again before it is
 * wanted. Nothing else in the game gets a second chance, which is exactly why
 * nothing else may be spent on a counter that is already full.
 */
static bool item_would_be_wasted(const GameplayState *state,
                                 const CampaignState *campaign,
                                 ItemType type)
{
    switch (type)
    {
    case ITEM_GRENADE:
        return state->player.grenades > 0;
    case ITEM_BAZOOKA:
        return state->player.bazooka_rockets >= BAZOOKA_AMMO;
    case ITEM_FLASHBANG:
        /* One at a time, exactly like the grenade, and left on the floor rather
         * than destroyed by walking over it while already carrying one. */
        return state->player.flashbangs > 0;
    case ITEM_MEDKIT:
        /* The kit answers the hearts first and the spare lives second, so it
         * is only wasted when both are already at their cap. */
        return state->player.hp >= gameplay_player_max_hp(state) &&
               campaign->lives >= MAX_LIVES;
    case ITEM_CARD:
    case ITEM_GUN:
    case ITEM_EVIDENCE:
        /* Paper cannot be wasted: there is no counter for it to be full of, and
         * two sheets are two sheets. It is on this list so the switch stays
         * exhaustive — a new item type that nobody decided about is exactly how
         * the three explosives came to be missing from it. */
        break;
    }
    return false;
}

void gameplay_collect_items(GameplayState *state, CampaignState *campaign,
                            float dt)
{
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    for (int i = 0; i < state->level.runtime.item_count; ++i)
    {
        Item *item = &state->level.runtime.items[i];
        if (!item->collected &&
            !item_would_be_wasted(state, campaign, item->type) &&
            gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, player_h,
                                   item->x - 8.0f, item->y - 8.0f,
                                   16.0f, 16.0f))
        {
            item->collected = true;
            /* The magazine is the only thing that comes back.
             *
             * It has to: the sidearm is what the sector is played with, and a
             * player who has spent it and cannot find another box is playing
             * the rest of the floor with a knife. Nothing else is in that
             * position. The grenade used to come back with it, which made one
             * `N` an unlimited supply at ten seconds apiece — enough to clear
             * a floor a blast at a time, and enough to open every blocked-up
             * patch in the campaign without ever needing the bazooka the
             * patches were placed for. A one-shot explosive that regrows is
             * not a decision about when to spend it. */
            item->respawn_timer =
                item->type == ITEM_GUN ? ITEM_RESPAWN_TIME : 0.0f;
            switch (item->type)
            {
            case ITEM_CARD:
                campaign->score += CARD_SCORE;
                /* Any card found is progress worth resuming at. */
                gameplay_bank_checkpoint(state);
                if (i == state->level.runtime.active_card_index)
                {
                    /*
                     * The live card always answers, and only the door's own
                     * fanfare is conditional.
                     *
                     * `gameplay_unlock_exit` returns without a sound when there
                     * is no door to open — an interior whose stair core is
                     * welded and whose route out is the window, or a sector
                     * where a finished hack already opened it. Left at that,
                     * the *right* card was the one pickup in the game that made
                     * no sound at all, while a decoy buzzed: the feedback
                     * exactly backwards, in a sector where the strip reads
                     * BLOCKED and cannot report it either.
                     */
                    bool was_locked = !state->level.runtime.exit_unlocked;
                    gameplay_unlock_exit(state);
                    if (!(was_locked && state->level.runtime.exit_unlocked))
                        game_events_sound(&state->events, SFX_CARD_SCAN);
                }
                else if (state->level.map.has_window)
                {
                    /*
                     * **And no card is wrong in a sector nothing unlocks.**
                     *
                     * The paragraph above used to end "no shipped map has a
                     * card in a window sector today, which is the only reason
                     * nobody has heard it", and sector 14 is one: a `Y`, a
                     * welded `E` and two `C` on the busiest floor below the
                     * roof. So the branch it excuses runs in the shipped game,
                     * and the half nobody had looked at is this one — a decoy
                     * buzzing `SFX_CARD_WRONG` at a player whose strip already
                     * reads BLOCKED, which reads as "wrong one, keep looking"
                     * on a floor where looking finds nothing. The seed still
                     * picks a live card here and it still opens the nothing it
                     * always opened; what it must not do is send the player
                     * back across twelve men for a card that cannot exist.
                     * A card in a window sector is a card that scanned.
                     */
                    game_events_sound(&state->events, SFX_CARD_SCAN);
                }
                else
                    game_events_sound(&state->events, SFX_CARD_WRONG);
                break;
            case ITEM_GUN:
                /* The one pickup allowed to change what is in the hand, and
                 * only out of the knife: a dry clip is the whole reason Chuck
                 * is holding a blade, so the magazine that ends that has to
                 * end it without a button press. Nothing is spent by holding
                 * the sidearm, so this can never cost the player anything —
                 * unlike the explosives below. A player who deliberately
                 * picked the knife while carrying a loaded gun is not in this
                 * case and keeps it. */
                if (state->player.bullets == 0 &&
                    state->player.active_weapon == PLAYER_WEAPON_KNIFE)
                    state->player.active_weapon = PLAYER_WEAPON_PISTOL;
                state->player.bullets = MAX_AMMO;
                game_events_sound(&state->events, SFX_PICKUP_AMMO);
                break;
            case ITEM_GRENADE:
                /* Picking a one-shot explosive up is not deciding to spend it.
                 * Arming it here meant walking over an `N` mid-firefight and
                 * throwing the grenade with the next press of the trigger that
                 * was meant for the pistol — a single pickup silently spending
                 * the scarcest thing in the sector. The HUD shows the grenade
                 * is carried; the bumpers are how it reaches the hand. */
                state->player.grenades = 1;
                game_events_sound(&state->events, SFX_PICKUP_GRENADE);
                break;
            case ITEM_MEDKIT:
                /* Hearts first; a spare life only once the hearts are full,
                 * so the kit is never wasted on either counter. */
                if (state->player.hp < gameplay_player_max_hp(state))
                    state->player.hp = gameplay_player_max_hp(state);
                else if (campaign->lives < MAX_LIVES)
                    campaign->lives++;
                gameplay_bank_checkpoint(state);
                game_events_sound(&state->events, SFX_PICKUP_HEALTH);
                break;
            case ITEM_BAZOOKA:
                /* Same rule as the grenade, and it matters more here: one `Z`
                 * a sector, and the patched walls it was placed for are only
                 * opened by a rocket that was not fired at the first guard
                 * along. */
                state->player.bazooka_rockets = BAZOOKA_AMMO;
                game_events_sound(&state->events, SFX_PICKUP_BAZOOKA);
                break;
            case ITEM_FLASHBANG:
                /* Same rule as every other one-shot: picking it up is not
                 * deciding to spend it, so nothing arms itself here. */
                state->player.flashbangs = 1;
                game_events_sound(&state->events, SFX_PICKUP_GRENADE);
                break;
            case ITEM_EVIDENCE:
                /*
                 * The one pickup that changes nothing about the next five
                 * minutes. It is counted on the *campaign* rather than on the
                 * sector, because a sheet of the docket is not a floor's
                 * business — it is what the night amounts to, and the sector's
                 * own counters are wiped at every doorway.
                 *
                 * No checkpoint is banked. Cards, hacks, doors and medkits bank
                 * one because they are progress the player would have to redo;
                 * this is a detour they chose, and banking it would quietly
                 * make the safest route through a sector "touch the
                 * collectable first".
                 */
                campaign->evidence_collected++;
                campaign->score += EVIDENCE_SCORE;
                game_events_sound(&state->events, SFX_CARD_SCAN);
                break;
            }
        }

        if (item->collected && item->type == ITEM_GUN)
        {
            item->respawn_timer -= dt;
            if (item->respawn_timer <= 0.0f)
            {
                item->collected = false;
                item->respawn_timer = 0.0f;
            }
        }
    }
}

bool gameplay_player_reached_exit(const GameplayState *state)
{
    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    if (state->level.map.has_window)
    {
        return gameplay_boxes_overlap(
            state->player.x, state->player.y, PLAYER_W, height,
            state->level.map.window_col * (float)TILE_SIZE,
            state->level.map.window_row * (float)TILE_SIZE,
            TILE_SIZE, TILE_SIZE);
    }
    if (!state->level.map.has_exit || !state->level.runtime.exit_unlocked)
        return false;
    return gameplay_boxes_overlap(
        state->player.x, state->player.y, PLAYER_W, height,
        state->level.map.exit_col * (float)TILE_SIZE,
        state->level.map.exit_row * (float)TILE_SIZE,
        TILE_SIZE, TILE_SIZE);
}

int gameplay_neutralized_hostiles(const GameplayState *state)
{
    /* The running tally, not a sweep of the `dead` flags. Those flags say who
     * is still standing: a reinforcement takes over a downed guard's slot, so
     * counting them reported one kill fewer for every guard the doors sent
     * after the first — the report between sectors credited the player with
     * less than the floor they had actually cleared. */
    return state->hostiles_neutralized;
}
