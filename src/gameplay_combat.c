#include "gameplay_combat.h"

#include "gameplay_world.h"

#include <math.h>

static float player_height(const GameplayState *state)
{
    return state->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
}

/* A released climb key must not turn the next ladder attack sideways. Explicit
 * vertical input takes priority, while left/right still selects a side attack. */
static int player_ladder_attack_direction(const Player *player,
                                          const Input *input)
{
    if (!player->on_ladder)
        return 0;
    if (input->up && !input->down)
        return -1;
    if (input->down && !input->up)
        return 1;
    if (input->up || input->down || input->left || input->right)
        return 0;
    return player->ladder_direction;
}

static bool within_radius(float x, float y, float center_x, float center_y,
                          float radius)
{
    float dx = x - center_x;
    float dy = y - center_y;
    return dx * dx + dy * dy <= radius * radius;
}

/*
 * The rise on an underarm throw, from how fast the thing is leaving the hand.
 *
 * A flat throw lands at the foot of the first wall, which is exactly where the
 * player already is, so everything lobbed out of Chuck's hand goes up first. The
 * rise is solved from the horizontal speed — the faster it leaves, the flatter it
 * flies, which is the arc a hand actually produces — and clamped at both ends so a
 * slow lob does not stall overhead and a fast one still clears a crate.
 *
 * **At every speed the game actually throws at, the solve is over the cap and the
 * cap is the answer.** `THROW_ARC_STRENGTH * GRAVITY / (2 * vx)` is 335 for the
 * grenade, 348 for the flash charge and 261 for the bolt against a
 * `THROW_ARC_MAX_RISE` of 220, so all three rise at 220 and the derivation changes
 * nothing today; the lower clamp needs a throw over 2600px/s to be reached at all.
 * Worth knowing before tuning a lob, because the number that moves the feel is the
 * cap and not the strength — and worth knowing before believing the sentence above
 * it, which describes a behaviour the shipped speeds never reach.
 * `test_every_underarm_throw_rises_at_the_cap` is what will say so if that stops
 * being true.
 *
 * One function because it is one arc. It was written out three times, once in each
 * throw branch, with the decoy's copy carrying a comment claiming it was "the same
 * arc the grenade is thrown on" — an equality that nothing held: tuning the
 * grenade's lob would have left the flash charge and the bolt behind, and the
 * comment asserting otherwise is exactly what would stop the next reader checking.
 */
static float throw_arc_speed(float vx)
{
    float arc_speed = THROW_ARC_STRENGTH * GRAVITY / (2.0f * fabsf(vx));
    if (arc_speed < THROW_ARC_MIN_RISE)
        arc_speed = THROW_ARC_MIN_RISE;
    if (arc_speed > THROW_ARC_MAX_RISE)
        arc_speed = THROW_ARC_MAX_RISE;
    return arc_speed;
}

static int find_grenade_slot(GameplayState *state)
{
    for (int i = 0; i < state->grenade_count; ++i)
        if (!state->grenades[i].active)
            return i;
    if (state->grenade_count < MAX_GRENADES)
        return state->grenade_count++;
    return -1;
}

static int find_rocket_slot(GameplayState *state)
{
    for (int i = 0; i < MAX_ROCKETS; ++i)
        if (!state->rockets[i].active)
            return i;
    return -1;
}

static bool rocket_is_vertical(const Rocket *rocket)
{
    return fabsf(rocket->vy) > fabsf(rocket->vx);
}

static float rocket_width(const Rocket *rocket)
{
    return rocket_is_vertical(rocket) ? (float)ROCKET_H : (float)ROCKET_W;
}

static float rocket_height(const Rocket *rocket)
{
    return rocket_is_vertical(rocket) ? (float)ROCKET_W : (float)ROCKET_H;
}

static void damage_dog(GameplayState *state, CampaignState *campaign,
                       Dog *dog)
{
    dog->hp--;
    if (dog->hp <= 0)
    {
        dog->dead = true;
        gameplay_record_neutralized(state, campaign);
        campaign->score += DOG_SCORE;
        game_events_particles(&state->events,
                              dog->x + DOG_W * 0.5f,
                              dog->y + DOG_H * 0.5f,
                              14, dog->dir);
    }
    gameplay_world_sound(state, SFX_DOG_YELP,
                         dog->x + DOG_W * 0.5f,
                         dog->y + DOG_H * 0.5f);
}

static void damage_enemy(GameplayState *state, CampaignState *campaign,
                         int enemy_index)
{
    Enemy *enemy = &state->enemies[enemy_index];
    enemy->hp--;
    if (enemy->hp <= 0)
    {
        enemy->dead = true;
        gameplay_record_neutralized(state, campaign);
        campaign->score += ENEMY_SCORE;
        game_events_particles(&state->events,
                              enemy->x + ENEMY_W * 0.5f,
                              enemy->y + ENEMY_H * 0.5f,
                              24, enemy->dir);
        gameplay_world_sound(state, SFX_ENEMY_DOWN,
                             enemy->x + ENEMY_W * 0.5f,
                             enemy->y + ENEMY_H * 0.5f);
        /* Downed in direct combat, the guard's spare magazine survives; an
         * explosion would have taken it with him. */
        gameplay_spawn_ammo_drop(state, enemy->x + ENEMY_W * 0.5f,
                                 enemy->y + ENEMY_H - AMMO_DROP_H);
    }
    else
    {
        gameplay_world_sound(state, SFX_ENEMY_HIT,
                             enemy->x + ENEMY_W * 0.5f,
                             enemy->y + ENEMY_H * 0.5f);
    }
    gameplay_provoke_enemy(state, enemy_index);
}

/*
 * A guard who has no idea Chuck is in the room.
 *
 * Everything a guard knows about the player is already held in four flags, and
 * this is all four of them being clear: the building alarm is down, nobody has
 * shot him (`provoked`), he is not on his way to a switch, and he has not
 * decided an encounter — which is the flag `update_guard_encounters` sets the
 * first frame he lays eyes on Chuck and clears again `GUARD_ENCOUNTER_RESET_TIME`
 * after losing him. A man already mid-aim is excluded with them, because that
 * aim is started by `update_enemy_reactions` for a guard who has just been
 * walked up behind and *has* turned: the telegraph is running, the player can
 * see it, and treating him as unaware would take away the one answer the
 * telegraph exists to offer.
 *
 * **Deliberately not a line-of-sight test.** Sight is what sets these flags in
 * the first place, and asking it again here would make the rule depend on which
 * frame the swing landed on rather than on what the guard had actually
 * noticed — a takedown that works or does not according to where a patrol turn
 * happened to be in its cycle is not a rule anybody can learn to play.
 */
static bool guard_is_unaware(const GameplayState *state, const Enemy *enemy)
{
    return !gameplay_alarm_active(state) && !enemy->provoked &&
           !enemy->raising_alarm && !enemy->encounter_decided &&
           enemy->aim_timer <= 0.0f;
}

/*
 * And which side the blade arrived from. `enemy->dir` is the way he is facing,
 * so a negative product is Chuck standing behind his shoulders. Level with him
 * is not behind him — the product is nought and the ordinary three-hit knife
 * applies — and neither is a spawn that has not picked a direction yet, which
 * is what a zeroed `dir` means.
 */
static bool player_is_behind_guard(const GameplayState *state,
                                   const Enemy *enemy)
{
    float dx = (state->player.x + PLAYER_W * 0.5f) -
               (enemy->x + ENEMY_W * 0.5f);
    return dx * (float)enemy->dir < 0.0f;
}

/*
 * The knife used the way the man holding it was trained to use it.
 *
 * Chuck is twelve years an Army sapper, and up to now the blade was what he was
 * left holding when the clip ran dry: three swings, at a man who turns round
 * after the first and shoots him during the second. That made the one weapon
 * that never runs out the one weapon nobody would choose, and it left the whole
 * perception model — the cone, the peripheral radius, the sight timer, the
 * `GUARD_ENCOUNTER_RESET_TIME` window a guard needs to forget somebody — with
 * exactly one thing the player could do about it, which was to shoot first.
 *
 * What separates this from `damage_enemy` is not the damage. It is that a
 * takedown does not call `gameplay_provoke_enemy`, and that omission is the
 * feature: provoking wakes the man who was hit *and the partner he was talking
 * to*, and it is what turns one guard going down into two guards hunting. A
 * blade behind an unaware man wakes nobody, so the sector stays as quiet as it
 * was — which is the trade being offered, since getting behind him at all costs
 * the player their range advantage and their mercy window.
 *
 * The magazine still drops. It belongs to direct combat and this is the most
 * direct combat in the game; only an explosion destroys it with its owner.
 */
static void takedown_enemy(GameplayState *state, CampaignState *campaign,
                           int enemy_index)
{
    Enemy *enemy = &state->enemies[enemy_index];
    enemy->hp = 0;
    enemy->dead = true;
    gameplay_record_neutralized(state, campaign);
    campaign->score += PLAYER_TAKEDOWN_SCORE;
    /* Fewer than the two dozen a shot throws, and `SFX_ENEMY_HIT` rather than
     * `SFX_ENEMY_DOWN`: a man folding without a sound is the entire point of
     * the move, and the death cry the other kills play would be the sector
     * announcing to the player the very thing they just avoided announcing to
     * the floor. It is not silent — a press that does nothing audible reads as
     * a press the game missed — it is quiet. */
    game_events_particles(&state->events,
                          enemy->x + ENEMY_W * 0.5f,
                          enemy->y + ENEMY_H * 0.5f,
                          10, enemy->dir);
    gameplay_world_sound(state, SFX_ENEMY_HIT,
                         enemy->x + ENEMY_W * 0.5f,
                         enemy->y + ENEMY_H * 0.5f);
    gameplay_spawn_ammo_drop(state, enemy->x + ENEMY_W * 0.5f,
                             enemy->y + ENEMY_H - AMMO_DROP_H);
}

static void player_knife_attack(GameplayState *state,
                                CampaignState *campaign, int vertical)
{
    float height = player_height(state);
    float attack_x = state->player.x;
    float attack_y = state->player.y;
    float attack_w = PLAYER_W;
    float attack_h = PLAYER_KNIFE_RANGE;
    if (vertical < 0)
    {
        attack_y -= PLAYER_KNIFE_RANGE;
    }
    else if (vertical > 0)
    {
        attack_y += height;
    }
    else
    {
        attack_x = state->player.facing > 0
                       ? state->player.x + PLAYER_W
                       : state->player.x - PLAYER_KNIFE_RANGE;
        attack_w = PLAYER_KNIFE_RANGE;
        attack_h = height;
    }

    state->player.shot_vertical = vertical;
    state->player.knife_attacking = true;
    state->player.grenade_throwing = false;
    state->player.bazooka_firing = false;
    state->player.action_timer = PLAYER_KNIFE_ACTION_TIME;
    game_events_sound(&state->events, SFX_KNIFE_SWING);

    /* A dog is never taken from behind, and that is a rule about the animal
     * rather than a gap in the one below. A guard is beaten by getting outside
     * the cone he is looking down; a dog has no cone to get outside of — it
     * hears and smells, `dog_sees_player` is a range and a rough heading rather
     * than a facing test, and the whole reason `DOG_BITE_WINDUP` announces
     * itself is that the animal has already found you. Handing the blade a
     * silent answer to one would make the quietest way through a sector the one
     * that walks straight past the thing placed to stop it. */
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (!dog->dead &&
            gameplay_boxes_overlap(attack_x, attack_y, attack_w, attack_h,
                                   dog->x, dog->y, DOG_W, DOG_H))
        {
            damage_dog(state, campaign, dog);
        }
    }

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead ||
            !gameplay_boxes_overlap(attack_x, attack_y, attack_w, attack_h,
                                    enemy->x, enemy->y, ENEMY_W, ENEMY_H))
        {
            continue;
        }
        if (guard_is_unaware(state, enemy) &&
            player_is_behind_guard(state, enemy))
            takedown_enemy(state, campaign, i);
        else
            damage_enemy(state, campaign, i);
    }
}

static void damage_crates_in_radius(GameplayState *state,
                                    CampaignState *campaign,
                                    float x, float y, float radius)
{
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        Crate *crate = &state->level.runtime.crates[i];
        if (crate->active &&
            within_radius(crate->x + CRATE_W * 0.5f,
                          crate->y + CRATE_H * 0.5f, x, y, radius))
            gameplay_destroy_crate(state, campaign, crate);
    }
}

static void explode_gas_canister(GameplayState *state,
                                 CampaignState *campaign,
                                 GasCanister *canister);
static void explode_mine(GameplayState *state, CampaignState *campaign,
                         Mine *mine);
static void explode_grenade(GameplayState *state, CampaignState *campaign,
                            Grenade *grenade);
static void explode_rocket(GameplayState *state, CampaignState *campaign,
                           Rocket *rocket);

/*
 * Everything a blast does to the world, in one place.
 *
 * The four explosives differ only in where they go off, how far they reach and
 * how hard they shake the frame; what a blast *does* is one rule, and it is
 * this one. Four hand-written copies of it is what let a rocket set off a gas
 * canister while a grenade landing against the same canister did nothing, and
 * what let a mine bring a wall down without troubling the guard standing in
 * the hole it made — a blast that picks which of the things beside it are real
 * is a blast the player cannot reason about.
 *
 * A guard taken by a blast leaves no magazine: the drop belongs to direct
 * combat, and an explosion destroys it with its owner. The player can only be
 * hurt once however many blasts a chain sets off, because the first one opens
 * the mercy window.
 *
 * **Everything explosive in reach goes off with it**, and that is the rule
 * rather than a list: a canister, a mine and a live grenade are all charges
 * sitting in the blast, and a blast that set off one of the three and left the
 * other two lying there is a blast the player cannot reason about. The canister
 * chained on its own for a long time while the mine did not, so a rocket fired
 * into a mined corridor cleared the canisters and stepped over the mines — and
 * nothing on screen said why. Every chain terminates for the same reason: each
 * charge is deactivated *before* its own blast is applied, so the set of live
 * explosives strictly shrinks and a charge can never re-enter through its own
 * radius.
 *
 * **A blast is a radius and nothing else: it is not stopped by a wall.** This
 * is the one place the game's single solidity rule does not apply — a bullet
 * stops on masonry and a guard cannot see through it, but pressure goes round
 * the corner. It is deliberate, and the reason is the `%` patch: an explosive
 * spent bringing a blocked-up opening down is supposed to take whoever was
 * standing behind it with the wall, and a blast that opened the hole and left
 * the man in it untouched would read as the charge going off in a different
 * room. It cuts both ways, which is what keeps it fair — Chuck's own rocket
 * reaches him through the wall he fired it at just as readily.
 *
 * What decides whether it *actually* reaches through is the radius against the
 * geometry, and the four explosives land on both sides of that line. Across one
 * tile of wall the nearest a guard's centre can be to the blast is the tile
 * itself plus half of each body — a little over fifty pixels — so the mine (36)
 * and the grenade (48) fall short of it and the rocket (72) clears it
 * comfortably. The canister (56) sits close enough to that number that which
 * side of it any particular pair of bodies lands on is arithmetic rather than a
 * rule, and nothing should be designed around it either way.
 *
 * That difference is real in how the four play, and it is the kind of thing a
 * tuning pass moves without noticing, so `test_a_blast_carries_through_a_wall`
 * pins the two unambiguous ends: raising a radius past the wall is a decision,
 * not a nudge.
 */
static void apply_blast(GameplayState *state, CampaignState *campaign,
                        float x, float y, float radius)
{
    gameplay_alert_enemies_to_noise(state, x, y, ENEMY_HEAR_RADIUS_BLAST);

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead ||
            !within_radius(enemy->x + ENEMY_W * 0.5f,
                           enemy->y + ENEMY_H * 0.5f,
                           x, y, radius))
        {
            continue;
        }
        enemy->hp = 0;
        enemy->dead = true;
        gameplay_record_neutralized(state, campaign);
        game_events_particles(&state->events,
                              enemy->x + ENEMY_W * 0.5f,
                              enemy->y + ENEMY_H * 0.5f,
                              24, enemy->dir);
        /* A man going down sounds the same whatever took him: a grenade that
         * killed three guards in silence read as having killed nobody. */
        gameplay_world_sound(state, SFX_ENEMY_DOWN,
                             enemy->x + ENEMY_W * 0.5f,
                             enemy->y + ENEMY_H * 0.5f);
        campaign->score += ENEMY_SCORE;
    }
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (dog->dead ||
            !within_radius(dog->x + DOG_W * 0.5f,
                           dog->y + DOG_H * 0.5f,
                           x, y, radius))
        {
            continue;
        }
        dog->hp = 0;
        dog->dead = true;
        gameplay_record_neutralized(state, campaign);
        game_events_particles(&state->events,
                              dog->x + DOG_W * 0.5f,
                              dog->y + DOG_H * 0.5f,
                              14, dog->dir);
        gameplay_world_sound(state, SFX_DOG_YELP,
                             dog->x + DOG_W * 0.5f,
                             dog->y + DOG_H * 0.5f);
        campaign->score += DOG_SCORE;
    }

    damage_crates_in_radius(state, campaign, x, y, radius);
    gameplay_break_walls_in_radius(state, campaign, x, y, radius);

    /* A blast takes the fittings with it. Left out, a grenade could bring the
     * wall down and leave the camera bolted to what was left of it looking at
     * the hole — which is the shape of bug `apply_blast` exists to stop: a
     * blast that picks which of the things beside it are real. */
    for (int i = 0; i < state->level.map.camera_count; ++i)
    {
        float cx = 0.0f;
        float cy = 0.0f;
        float cw = 0.0f;
        float ch = 0.0f;
        gameplay_camera_box(&state->level.map.cameras[i], &cx, &cy, &cw, &ch);
        if (within_radius(cx + cw * 0.5f, cy + ch * 0.5f, x, y, radius))
            gameplay_destroy_camera(state, campaign, i);
    }

    for (int i = 0; i < state->level.runtime.gas_canister_count; ++i)
    {
        GasCanister *canister = &state->level.runtime.gas_canisters[i];
        if (canister->active &&
            within_radius(canister->x + GAS_CANISTER_W * 0.5f,
                          canister->y + GAS_CANISTER_H * 0.5f,
                          x, y, radius))
        {
            explode_gas_canister(state, campaign, canister);
        }
    }

    /* A charge in the blast is a charge that goes off, whether or not anybody
     * ever stepped on it: the player's weight is what *arms* a mine, and
     * pressure is what sets one off. Waiting out MINE_TRIGGER_DELAY here would
     * be the wrong reading of both — the delay is the beat between a boot and
     * the bang, and there is no boot in this. */
    for (int i = 0; i < state->mine_count; ++i)
    {
        Mine *mine = &state->mines[i];
        if (mine->active &&
            within_radius(mine->x + MINE_W * 0.5f, mine->y + MINE_H * 0.5f,
                          x, y, radius))
        {
            explode_mine(state, campaign, mine);
        }
    }

    for (int i = 0; i < state->grenade_count; ++i)
    {
        Grenade *grenade = &state->grenades[i];
        if (grenade->active &&
            within_radius(grenade->x + GRENADE_W * 0.5f,
                          grenade->y + GRENADE_H * 0.5f,
                          x, y, radius))
        {
            explode_grenade(state, campaign, grenade);
        }
    }

    /* A rocket still in the air is a warhead in a fireball, and it was the one
     * charge on the list that a blast stepped over. `MAX_ROCKETS` is one, so
     * this is only ever reachable by walking a rocket into a chain the player
     * set off themselves — but "everything explosive in reach goes off with
     * it" is a rule or it is a list, and a list is what the four hand-written
     * copies of this function used to be. Terminating for the same reason as
     * the rest: `explode_rocket` clears `active` before applying its own
     * blast, so the set of live charges strictly shrinks. */
    for (int i = 0; i < MAX_ROCKETS; ++i)
    {
        Rocket *rocket = &state->rockets[i];
        if (rocket->active &&
            within_radius(rocket->x + rocket_width(rocket) * 0.5f,
                          rocket->y + rocket_height(rocket) * 0.5f,
                          x, y, radius))
        {
            explode_rocket(state, campaign, rocket);
        }
    }

    if (within_radius(state->player.x + PLAYER_W * 0.5f,
                      state->player.y + player_height(state) * 0.5f,
                      x, y, radius))
    {
        gameplay_damage_player(state, EXPLOSION_DAMAGE, x, y);
    }
}

static void explode_gas_canister(GameplayState *state,
                                 CampaignState *campaign,
                                 GasCanister *canister)
{
    if (!canister->active)
        return;

    canister->active = false;
    float x = canister->x + GAS_CANISTER_W * 0.5f;
    float y = canister->y + GAS_CANISTER_H * 0.5f;
    game_events_explosion(&state->events, x, y, 72);
    gameplay_world_sound(state, SFX_EXPLOSION, x, y);
    game_events_camera_shake(&state->events, 8.0f, 0.32f);
    apply_blast(state, campaign, x, y, GAS_CANISTER_RADIUS);
}

/* Its own function so the boot that arms it and the blast that sets it off
 * reach the same code, and so the guard below can stop a chain that has already
 * come through here — see `apply_blast`. */
static void explode_mine(GameplayState *state, CampaignState *campaign,
                         Mine *mine)
{
    if (!mine->active)
        return;

    mine->active = false;
    float x = mine->x + MINE_W * 0.5f;
    float y = mine->y + MINE_H * 0.5f;
    game_events_explosion(&state->events, x, y, 48);
    gameplay_world_sound(state, SFX_EXPLOSION, x, y);
    game_events_camera_shake(&state->events, 5.0f, 0.24f);
    apply_blast(state, campaign, x, y, MINE_RADIUS);
}

static void explode_grenade(GameplayState *state, CampaignState *campaign,
                            Grenade *grenade)
{
    /* The same guard the canister and the mine keep: a grenade caught by a
     * blast is spent by it, and the fuse that was already running must not
     * spend it a second time when the loop that owns it comes round. */
    if (!grenade->active)
        return;

    grenade->active = false;
    float x = grenade->x + GRENADE_W * 0.5f;
    float y = grenade->y + GRENADE_H * 0.5f;
    game_events_explosion(&state->events, x, y, 64);
    gameplay_world_sound(state, SFX_EXPLOSION, x, y);
    game_events_camera_shake(&state->events, 7.0f, 0.30f);
    apply_blast(state, campaign, x, y, GRENADE_RADIUS);
}

static void explode_rocket(GameplayState *state, CampaignState *campaign,
                           Rocket *rocket)
{
    /* The same guard the other three keep, and it is what makes chaining a
     * rocket safe to say: a rocket taken by somebody else's blast is spent by
     * it, and the impact test that owns it must not spend it again. */
    if (!rocket->active)
        return;

    float width = rocket_width(rocket);
    float height = rocket_height(rocket);
    rocket->active = false;
    float x = rocket->x + width * 0.5f;
    float y = rocket->y + height * 0.5f;
    game_events_explosion(&state->events, x, y, 88);
    gameplay_world_sound(state, SFX_EXPLOSION, x, y);
    game_events_camera_shake(&state->events, 10.0f, 0.38f);
    apply_blast(state, campaign, x, y, ROCKET_RADIUS);
}

/*
 * A flash charge going off, and everything it deliberately does not do.
 *
 * It is the shape of `apply_blast` with the whole of `apply_blast` taken out.
 * Nothing is killed, no crate is broken, no weak wall opens, no charge in reach
 * chains, and the player himself is untouched — a flash that could hurt Chuck
 * would be a grenade with a worse radius, and the point of the thing is that it
 * is safe to use in the room you are standing in.
 *
 * What it *does* is take the room's attention away for `FLASH_BLIND_TIME`. It
 * does not clear the alarm, does not unprovoke anybody and does not clear a
 * pursuit target: everyone comes back exactly as they were, still hunting,
 * still remembering where Chuck was standing. The charge buys seconds, and the
 * seconds are for leaving.
 */
/*
 * Whether the charge went off somewhere this pair of eyes could see it.
 *
 * A flash is light, so it is stopped by everything light is stopped by — which
 * is the same `gameplay_sight_line_clear` a guard's own cone is stopped by, and
 * saying it here rather than in three places is the whole reason this is a
 * function. The radius alone was the entire test for as long as the charge
 * existed, and `FLASH_RADIUS` is five tiles: wider than any partition in the
 * building and wider than a storey is tall, so one charge blinded the room next
 * door, the floor above and the floor below. `game_config.h` has said the
 * opposite the whole time — *"it has to catch the room the player is in, and it
 * must not reach the one next door, or 'throw it and walk' would be the answer
 * to every floor"* — and that sentence is what this restores. Sector 12 is six
 * crawl levels stacked one riser apart and sector 14 is panelled rooms with
 * single doorways; both of them were a floor a single charge switched off.
 */
static bool flash_reaches(const GameplayState *state, float x, float y,
                          float tx, float ty)
{
    return within_radius(tx, ty, x, y, FLASH_RADIUS) &&
           gameplay_sight_line_clear(state, x, y, tx, ty);
}

static void detonate_flashbang(GameplayState *state, CampaignState *campaign,
                               Grenade *flash)
{
    if (!flash->active)
        return;

    flash->active = false;
    float x = flash->x + FLASH_W * 0.5f;
    float y = flash->y + FLASH_H * 0.5f;
    game_events_explosion(&state->events, x, y, 40);
    gameplay_world_sound(state, SFX_MINE_ARM, x, y);
    game_events_camera_shake(&state->events, 3.0f, 0.14f);
    campaign->score += FLASH_SCORE;

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead ||
            !flash_reaches(state, x, y, enemy->x + ENEMY_W * 0.5f,
                           enemy->y + ENEMY_H * 0.5f))
        {
            continue;
        }
        /* Taken rather than added to, so standing in two flashes is not six
         * seconds of blindness — the second charge is spent for nothing, which
         * is the right answer to spending it. */
        if (enemy->blind_timer < FLASH_BLIND_TIME)
            enemy->blind_timer = FLASH_BLIND_TIME;
        enemy->aim_timer = 0.0f;
        enemy->sight_timer = 0.0f;
        /* A man on his way to a switch is stopped where he is. It is the one
         * piece of state the charge is allowed to take, because the alternative
         * is a flash that goes off in a guard's face and does not stop him
         * pulling the alarm two paces later. */
        enemy->raising_alarm = false;
        enemy->alarm_switch_index = -1;
        enemy->alarm_use_timer = 0.0f;
        enemy->talking = false;
        enemy->talk_timer = 0.0f;
    }

    /* And the animal, which is the one thing in the room with an actual pair of
     * eyes and was the one thing this charge did not reach. Blinded and stopped
     * like the men, and a bite already being wound up is cancelled — that is the
     * dog's `aim_timer`, and leaving it running would mean a charge that went off
     * in its face and let the teeth land anyway. The chase is deliberately left
     * alone: the guards keep theirs too, because the charge buys seconds and
     * never the encounter. */
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (dog->dead ||
            !flash_reaches(state, x, y, dog->x + DOG_W * 0.5f,
                           dog->y + DOG_H * 0.5f))
        {
            continue;
        }
        if (dog->blind_timer < FLASH_BLIND_TIME)
            dog->blind_timer = FLASH_BLIND_TIME;
        dog->bite_windup = 0.0f;
        dog->bite_ready = false;
    }

    /* The lenses lose what they had as well. A camera is glass and a sensor,
     * and a charge this bright in front of one is the same event it is for a
     * pair of eyes — but it recovers on its own, so nothing here is permanent:
     * only a round or a blast takes a camera off the ceiling. */
    for (int i = 0; i < state->level.map.camera_count; ++i)
    {
        CameraState *cam = &state->cameras[i];
        if (!cam->working)
            continue;
        float cx = 0.0f;
        float cy = 0.0f;
        float cw = 0.0f;
        float ch = 0.0f;
        gameplay_camera_box(&state->level.map.cameras[i], &cx, &cy, &cw, &ch);
        if (!flash_reaches(state, x, y, cx + cw * 0.5f, cy + ch * 0.5f))
            continue;
        cam->notice = 0.0f;
        cam->suspicion = 0.0f;
    }
}

void gameplay_combat_update_explosives(GameplayState *state,
                                       CampaignState *campaign, float dt)
{
    for (int i = 0; i < MAX_FLASHBANGS; ++i)
    {
        Grenade *flash = &state->flashbangs[i];
        if (!flash->active)
            continue;
        flash->vy += GRAVITY * dt;
        if (flash->vy > MAX_FALL_SPEED)
            flash->vy = MAX_FALL_SPEED;
        bool on_ground = false;
        bool was_grounded = flash->grounded;
        level_move(&state->level, &flash->x, &flash->y, &flash->vx, &flash->vy,
                   FLASH_W, FLASH_H, dt, false, &on_ground, false,
                   STANCE_UPRIGHT);
        if (on_ground)
        {
            flash->grounded = true;
            flash->vx = 0.0f;
            flash->vy = 0.0f;
            if (!was_grounded)
                gameplay_world_sound(state, SFX_GRENADE_BOUNCE,
                                     flash->x + FLASH_W * 0.5f,
                                     flash->y + FLASH_H * 0.5f);
        }
        flash->timer -= dt;
        if (flash->timer <= 0.0f)
            detonate_flashbang(state, campaign, flash);
    }

    for (int i = 0; i < state->mine_count; ++i)
    {
        Mine *mine = &state->mines[i];
        if (!mine->active)
            continue;
        if (!mine->triggered)
        {
            if (gameplay_boxes_overlap(state->player.x, state->player.y,
                                       PLAYER_W, player_height(state),
                                       mine->x, mine->y, MINE_W, MINE_H))
            {
                mine->triggered = true;
                mine->timer = MINE_TRIGGER_DELAY;
                gameplay_world_sound(state, SFX_MINE_ARM,
                                     mine->x + MINE_W * 0.5f,
                                     mine->y + MINE_H * 0.5f);
            }
            continue;
        }
        mine->timer -= dt;
        if (mine->timer > 0.0f)
            continue;

        /* Only the player's own weight arms a mine, but the delay between the
         * step and the blast is long enough to run out of and long enough for
         * whoever is chasing him to run into: the charge does not check who is
         * standing over it when it finally goes off. */
        explode_mine(state, campaign, mine);
    }

    for (int i = 0; i < state->grenade_count; ++i)
    {
        Grenade *grenade = &state->grenades[i];
        if (!grenade->active)
            continue;
        grenade->vy += GRAVITY * dt;
        if (grenade->vy > MAX_FALL_SPEED)
            grenade->vy = MAX_FALL_SPEED;
        bool on_ground = false;
        bool was_grounded = grenade->grounded;
        level_move(&state->level, &grenade->x, &grenade->y,
                   &grenade->vx, &grenade->vy,
                   GRENADE_W, GRENADE_H, dt, false, &on_ground, false,
                   STANCE_UPRIGHT);
        if (on_ground)
        {
            grenade->grounded = true;
            grenade->vx = 0.0f;
            grenade->vy = 0.0f;
            if (!was_grounded)
                gameplay_world_sound(state, SFX_GRENADE_BOUNCE,
                                     grenade->x + GRENADE_W * 0.5f,
                                     grenade->y + GRENADE_H * 0.5f);
        }
        grenade->fuse_sound_timer -= dt;
        grenade->timer -= dt;
        if (grenade->timer > 0.0f && grenade->fuse_sound_timer <= 0.0f)
        {
            gameplay_world_sound(state, SFX_GRENADE_FUSE,
                                 grenade->x + GRENADE_W * 0.5f,
                                 grenade->y + GRENADE_H * 0.5f);
            grenade->fuse_sound_timer = grenade->timer > 0.65f ? 0.30f : 0.14f;
        }
        if (grenade->timer <= 0.0f)
            explode_grenade(state, campaign, grenade);
    }
}

static int find_decoy_slot(GameplayState *state)
{
    for (int i = 0; i < MAX_DECOYS; ++i)
        if (!state->decoys[i].active)
            return i;
    return -1;
}

/*
 * A bolt in flight, and the noise it leaves where it stops.
 *
 * It is deliberately the *dumbest* projectile in the game. It does not sweep
 * for collisions the way a bullet does, because there is nothing it could hit
 * that would matter — it passes through guards, dogs, crates and canisters
 * alike, and the only thing it interacts with is the floor it lands on. A bolt
 * that could break a crate or set off a cylinder would be a weapon, and the one
 * thing this has to stay is not a weapon: the player must be able to throw it
 * into a room full of people and have the only consequence be that they all
 * look at it.
 *
 * `level_move` is what stops it, so a wall stops it as readily as a floor does
 * and the noise happens where the bolt actually ended up rather than where it
 * was aimed. Throwing one at the wall of the room next door is a legitimate way
 * to use it, and it is legitimate because this is the same collision every
 * other body in the sector obeys rather than a special case.
 */
void gameplay_combat_update_decoys(GameplayState *state, float dt)
{
    if (state->player.decoy_cooldown > 0.0f)
    {
        state->player.decoy_cooldown -= dt;
        if (state->player.decoy_cooldown < 0.0f)
            state->player.decoy_cooldown = 0.0f;
    }

    for (int i = 0; i < MAX_DECOYS; ++i)
    {
        Decoy *decoy = &state->decoys[i];
        if (!decoy->active)
            continue;

        decoy->vy += GRAVITY * dt;
        if (decoy->vy > MAX_FALL_SPEED)
            decoy->vy = MAX_FALL_SPEED;
        float previous_vx = decoy->vx;
        bool on_ground = false;
        level_move(&state->level, &decoy->x, &decoy->y, &decoy->vx, &decoy->vy,
                   DECOY_W, DECOY_H, dt, false, &on_ground, false,
                   STANCE_UPRIGHT);

        /*
         * Landed, or stopped dead against a wall. Both are the same event: the
         * bolt is where it is going to be.
         *
         * **The wall half is read off the speed the bolt had before the move,
         * not the speed it has after it**, and that is the whole of what this
         * test got wrong for a while. `level_move` *zeroes* the axis it blocks,
         * so a bolt that has just hit masonry comes back with `vx` at nought —
         * which made `vx != 0.0f` the one condition a wall strike can never
         * satisfy. The bolt stayed live, slid down the face it had hit and made
         * its noise on the floor at the bottom, up to a storey below the spot
         * the player aimed at. In a game where the bolt's entire value is
         * *where* the noise is, that is the feature failing quietly: throwing
         * one at the wall of the room next door is the documented use, and it
         * was landing in the wrong room.
         */
        bool struck_wall = previous_vx != 0.0f && decoy->vx == 0.0f;
        if (!on_ground && !struck_wall)
            continue;

        decoy->active = false;
        float x = decoy->x + DECOY_W * 0.5f;
        float y = decoy->y + DECOY_H * 0.5f;
        gameplay_world_sound(state, SFX_GRENADE_BOUNCE, x, y);
        /* The whole feature, in one call: every calm guard in earshot walks to
         * *this* spot rather than to Chuck's. It is the same function gunfire
         * and explosions already use, which is what makes the behaviour the
         * player gets out of it identical to the one they have already learned
         * from being shot at — guards already fighting or already running for a
         * switch are past being curious, and it skips them. */
        gameplay_alert_enemies_to_noise(state, x, y, DECOY_NOISE_RADIUS);
    }
}

void gameplay_combat_handle_player_action(GameplayState *state,
                                          CampaignState *campaign,
                                          Input *input)
{
    if (input->switch_weapon)
    {
        player_select_next_weapon(&state->player);
        input->switch_weapon = false;
    }
    if (input->switch_weapon_back)
    {
        player_select_prev_weapon(&state->player);
        input->switch_weapon_back = false;
    }
    if (!input->shoot)
        return;

    if (!player_weapon_available(&state->player,
                                 state->player.active_weapon))
    {
        player_fall_back_to_sidearm(&state->player);
    }

    if (state->player.active_weapon == PLAYER_WEAPON_BAZOOKA &&
        state->player.bazooka_rockets > 0)
    {
        int slot = find_rocket_slot(state);
        if (slot >= 0)
        {
            Rocket *rocket = &state->rockets[slot];
            int vertical =
                player_ladder_attack_direction(&state->player, input);
            rocket->active = true;
            if (vertical != 0)
            {
                rocket->x = state->player.x +
                            (PLAYER_W - ROCKET_H) * 0.5f;
                rocket->y = vertical < 0
                                ? state->player.y - ROCKET_W - 3.0f
                                : state->player.y + player_height(state) + 3.0f;
                rocket->vx = 0.0f;
                rocket->vy = vertical * ROCKET_SPEED;
            }
            else
            {
                rocket->vx = state->player.facing * ROCKET_SPEED;
                rocket->vy = 0.0f;
                rocket->x = state->player.facing > 0
                                ? state->player.x + PLAYER_W + 3.0f
                                : state->player.x - ROCKET_W - 3.0f;
                rocket->y = state->player.y + player_height(state) * 0.38f -
                            ROCKET_H * 0.5f;
            }
            state->player.bazooka_rockets--;
            state->player.shot_vertical = vertical;
            state->player.knife_attacking = false;
            state->player.grenade_throwing = false;
            state->player.bazooka_firing = true;
            state->player.action_timer = ROCKET_ACTION_TIME;
            gameplay_world_sound(state, SFX_ROCKET_LAUNCH,
                                 rocket->x + ROCKET_W * 0.5f,
                                 rocket->y + ROCKET_H * 0.5f);
            if (state->player.bazooka_rockets == 0)
                player_fall_back_to_sidearm(&state->player);
        }
        else
        {
            /* The tube is loaded but the last rocket is still in the air, and
             * MAX_ROCKETS is one. Reachable the moment a rocket carried in from
             * the sector below is fired and this sector's own `Z` is picked up
             * before it lands: the trigger is pulled and nothing whatever
             * happens, which reads as the pad having missed the press rather
             * than as the weapon being busy. */
            game_events_sound(&state->events, SFX_EMPTY_CLICK);
        }
    }
    else if (state->player.active_weapon == PLAYER_WEAPON_GRENADE &&
             state->player.grenades > 0)
    {
        int slot = find_grenade_slot(state);
        if (slot >= 0)
        {
            Grenade *grenade = &state->grenades[slot];
            grenade->active = true;
            grenade->timer = GRENADE_FUSE_TIME;
            grenade->fuse_sound_timer = 0.22f;
            grenade->grounded = false;
            int vertical =
                player_ladder_attack_direction(&state->player, input);
            if (vertical != 0)
            {
                grenade->x = state->player.x +
                             (PLAYER_W - GRENADE_W) * 0.5f;
                grenade->y = vertical < 0
                                 ? state->player.y - GRENADE_H - 3.0f
                                 : state->player.y + player_height(state) +
                                       3.0f;
                grenade->vx = 0.0f;
                grenade->vy = vertical * GRENADE_THROW_SPEED;
            }
            else
            {
                grenade->y =
                    state->player.y + player_height(state) * 0.45f;
                float speed = GRENADE_THROW_SPEED * 0.9f;
                grenade->vx = state->player.facing > 0 ? speed : -speed;
                grenade->x = state->player.x +
                             (state->player.facing > 0
                                  ? PLAYER_W + 6.0f
                                  : -(GRENADE_W + 6.0f));
                grenade->vy = -throw_arc_speed(grenade->vx);
            }
            /* Spent one at a time, like the rocket two branches above. Cleared
             * outright, as this used to be, a second grenade was destroyed by
             * throwing the first — identical for the campaign, which never hands
             * over two, and wrong for a count all the same. See
             * `test_a_throw_spends_one_grenade`. */
            state->player.grenades--;
            state->player.shot_vertical = vertical;
            state->player.knife_attacking = false;
            state->player.grenade_throwing = true;
            state->player.bazooka_firing = false;
            state->player.action_timer = PLAYER_THROW_ACTION_TIME;
            game_events_sound(&state->events, SFX_GRENADE_THROW);
            player_fall_back_to_sidearm(&state->player);
        }
        else
        {
            /* Same rule as the rocket: a pin pulled with nowhere to put the
             * grenade is a dead press, and a dead press has to be audible. */
            game_events_sound(&state->events, SFX_EMPTY_CLICK);
        }
    }
    else if (state->player.active_weapon == PLAYER_WEAPON_FLASH &&
             state->player.flashbangs > 0)
    {
        int slot = -1;
        for (int i = 0; i < MAX_FLASHBANGS; ++i)
            if (!state->flashbangs[i].active)
            {
                slot = i;
                break;
            }
        if (slot >= 0)
        {
            Grenade *flash = &state->flashbangs[slot];
            flash->active = true;
            flash->timer = FLASH_FUSE_TIME;
            flash->fuse_sound_timer = 0.0f;
            flash->grounded = false;
            int vertical =
                player_ladder_attack_direction(&state->player, input);
            if (vertical != 0)
            {
                flash->x = state->player.x + (PLAYER_W - FLASH_W) * 0.5f;
                flash->y = vertical < 0
                               ? state->player.y - FLASH_H - 3.0f
                               : state->player.y + player_height(state) + 3.0f;
                flash->vx = 0.0f;
                flash->vy = vertical * FLASH_THROW_SPEED;
            }
            else
            {
                flash->y = state->player.y + player_height(state) * 0.45f;
                float speed = FLASH_THROW_SPEED * 0.9f;
                flash->vx = state->player.facing > 0 ? speed : -speed;
                flash->x = state->player.x +
                           (state->player.facing > 0
                                ? PLAYER_W + 6.0f
                                : -(FLASH_W + 6.0f));
                flash->vy = -throw_arc_speed(flash->vx);
            }
            state->player.flashbangs--;
            state->player.shot_vertical = vertical;
            state->player.knife_attacking = false;
            state->player.grenade_throwing = true;
            state->player.bazooka_firing = false;
            state->player.action_timer = PLAYER_THROW_ACTION_TIME;
            game_events_sound(&state->events, SFX_GRENADE_THROW);
            player_fall_back_to_sidearm(&state->player);
        }
        else
        {
            game_events_sound(&state->events, SFX_EMPTY_CLICK);
        }
    }
    else if (state->player.active_weapon == PLAYER_WEAPON_DECOY)
    {
        /* No count to check, only a clock: a bolt is never carried and never
         * spent, so the cooldown is the entire limit on it. */
        int slot = state->player.decoy_cooldown > 0.0f
                       ? -1
                       : find_decoy_slot(state);
        if (slot >= 0)
        {
            Decoy *decoy = &state->decoys[slot];
            decoy->active = true;
            int vertical =
                player_ladder_attack_direction(&state->player, input);
            if (vertical != 0)
            {
                decoy->x = state->player.x + (PLAYER_W - DECOY_W) * 0.5f;
                decoy->y = vertical < 0
                               ? state->player.y - DECOY_H - 3.0f
                               : state->player.y + player_height(state) + 3.0f;
                decoy->vx = 0.0f;
                decoy->vy = vertical * DECOY_THROW_SPEED;
            }
            else
            {
                /* The same arc the grenade is thrown on, because it is
                 * literally the same function — see `throw_arc_speed`. */
                decoy->y = state->player.y + player_height(state) * 0.4f;
                float speed = DECOY_THROW_SPEED;
                decoy->vx = state->player.facing > 0 ? speed : -speed;
                decoy->x = state->player.x +
                           (state->player.facing > 0
                                ? PLAYER_W + 6.0f
                                : -(DECOY_W + 6.0f));
                decoy->vy = -throw_arc_speed(decoy->vx);
            }
            state->player.decoy_cooldown = DECOY_COOLDOWN;
            state->player.shot_vertical = vertical;
            state->player.knife_attacking = false;
            /* The throwing pose, which is what this is. It is the grenade's
             * flag because it is the grenade's animation — a man lobbing
             * something underarm — and a second flag for the same drawing would
             * be a second thing every path that ends an action has to clear. */
            state->player.grenade_throwing = true;
            state->player.bazooka_firing = false;
            state->player.action_timer = PLAYER_THROW_ACTION_TIME;
            game_events_sound(&state->events, SFX_GRENADE_THROW);
            /* And the hand keeps the bolts, unlike every other branch here.
             * Nothing was spent, so falling back to the sidearm would take the
             * tool out of the player's hand between the first throw and the
             * second — which is the pair of throws the whole mechanic is. */
        }
        else
        {
            /* The cooldown, or four bolts already in the air. The same rule the
             * dry clip and the busy launcher keep: a trigger that does nothing
             * has to say so, or it reads as the press having been missed. */
            game_events_sound(&state->events, SFX_EMPTY_CLICK);
        }
    }
    else if (state->player.active_weapon == PLAYER_WEAPON_PISTOL &&
             state->player.bullets > 0)
    {
        bool fired = false;
        for (int i = 0; i < MAX_BULLETS; ++i)
        {
            Bullet *bullet = &state->bullets[i];
            if (bullet->active)
                continue;
            bullet->active = true;
            fired = true;
            int vertical =
                player_ladder_attack_direction(&state->player, input);
            if (vertical != 0)
            {
                bullet->x = state->player.x +
                            (PLAYER_W - BULLET_H) * 0.5f;
                bullet->y = vertical < 0
                                ? state->player.y - BULLET_W
                                : state->player.y + player_height(state);
                bullet->vx = 0.0f;
                bullet->vy = vertical * BULLET_SPEED;
            }
            else
            {
                bullet->y = state->player.y + player_height(state) * 0.35f;
                bullet->vy = 0.0f;
                bullet->x = state->player.facing > 0
                                ? state->player.x + PLAYER_W
                                : state->player.x - BULLET_W;
                bullet->vx = state->player.facing * BULLET_SPEED;
            }
            state->player.bullets--;
            state->player.shot_vertical = vertical;
            state->player.knife_attacking = false;
            state->player.grenade_throwing = false;
            state->player.bazooka_firing = false;
            state->player.action_timer = PLAYER_SHOT_ACTION_TIME;
            game_events_sound(&state->events, SFX_PLAYER_SHOT);
            gameplay_alert_enemies_to_noise(
                state, state->player.x + PLAYER_W * 0.5f,
                state->player.y + player_height(state) * 0.5f,
                ENEMY_HEAR_RADIUS_SHOT);
            if (state->player.bullets == 0)
                player_fall_back_to_sidearm(&state->player);
            break;
        }
        if (!fired)
        {
            /* The same rule the rocket and the grenade keep: a loaded weapon
             * whose trigger does nothing has to say so. Every round of a full
             * clip is only rarely in the air at once — MAX_BULLETS is larger
             * than MAX_AMMO — but a magazine picked up while the last six are
             * still crossing a wide sector reaches it, and a silent press
             * reads as the pad having missed it. */
            game_events_sound(&state->events, SFX_EMPTY_CLICK);
        }
    }
    else
    {
        int vertical =
            player_ladder_attack_direction(&state->player, input);
        player_knife_attack(state, campaign, vertical);
    }
    input->shoot = false;
}

void gameplay_combat_update_hazards(GameplayState *state)
{
    if (state->invuln_timer > 0.0f)
        return;
    float height = player_height(state);
    for (int i = 0; i < state->level.map.ceiling_fan_count; ++i)
    {
        const CeilingFan *fan = &state->level.map.ceiling_fans[i];
        if (gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, height,
                                   fan->x - CEILING_FAN_BLADE_LENGTH,
                                   fan->y - CEILING_FAN_HIT_HEIGHT * 0.25f,
                                   CEILING_FAN_BLADE_LENGTH * 2.0f,
                                   CEILING_FAN_HIT_HEIGHT))
        {
            gameplay_world_sound(state, SFX_FAN_HIT, fan->x, fan->y);
            gameplay_damage_player(state, 1, fan->x, fan->y);
            return;
        }
    }
    for (int i = 0; i < state->level.map.spike_count; ++i)
    {
        const SpikeSpawn *spike = &state->level.map.spike_spawns[i];
        if (gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, height,
                                   spike->x, spike->y, SPIKE_W, SPIKE_H))
        {
            gameplay_world_sound(state, SFX_SPIKE_HIT,
                                 spike->x + SPIKE_W * 0.5f,
                                 spike->y + SPIKE_H * 0.5f);
            /* The pop in gameplay_damage_player lifts the boots back out of
             * the spike bed, so one misstep is one heart, not a lock-in. */
            gameplay_damage_player(state, 1, spike->x + SPIKE_W * 0.5f,
                                   spike->y + SPIKE_H * 0.5f);
            return;
        }
    }
}

void gameplay_combat_update_player_bullets(GameplayState *state,
                                           CampaignState *campaign,
                                           float dt)
{
    for (int i = 0; i < MAX_ROCKETS; ++i)
    {
        Rocket *rocket = &state->rockets[i];
        if (!rocket->active)
            continue;

        float previous_x = rocket->x;
        float previous_y = rocket->y;
        rocket->x += rocket->vx * dt;
        rocket->y += rocket->vy * dt;
        float width = rocket_width(rocket);
        float height = rocket_height(rocket);
        bool impact = false;

        if (rocket->x + width < 0.0f ||
            rocket->x > state->level.map.width * (float)TILE_SIZE ||
            rocket->y + height < 0.0f ||
            rocket->y > state->level.map.height * (float)TILE_SIZE)
        {
            impact = true;
        }
        else
        {
            float leading_x = rocket->x + width * 0.5f;
            float leading_y = rocket->y + height * 0.5f;
            if (rocket->vx > 0.0f)
                leading_x = rocket->x + width - 1.0f;
            else if (rocket->vx < 0.0f)
                leading_x = rocket->x;
            if (rocket->vy > 0.0f)
                leading_y = rocket->y + height - 1.0f;
            else if (rocket->vy < 0.0f)
                leading_y = rocket->y;
            impact = level_is_solid(&state->level,
                                    (int)floorf(leading_x / TILE_SIZE),
                                    (int)floorf(leading_y / TILE_SIZE));
        }

        float swept_x = fminf(previous_x, rocket->x);
        float swept_y = fminf(previous_y, rocket->y);
        float swept_w = width + fabsf(rocket->x - previous_x);
        float swept_h = height + fabsf(rocket->y - previous_y);
        if (!impact)
        {
            for (int j = 0; j < state->level.runtime.crate_count; ++j)
            {
                const Crate *crate = &state->level.runtime.crates[j];
                if (crate->active &&
                    gameplay_boxes_overlap(swept_x, swept_y,
                                           swept_w, swept_h,
                                           crate->x, crate->y,
                                           CRATE_W, CRATE_H))
                {
                    impact = true;
                    break;
                }
            }
        }
        if (!impact)
        {
            for (int j = 0; j < state->level.runtime.gas_canister_count; ++j)
            {
                const GasCanister *canister =
                    &state->level.runtime.gas_canisters[j];
                if (canister->active &&
                    gameplay_boxes_overlap(swept_x, swept_y,
                                           swept_w, swept_h,
                                           canister->x, canister->y,
                                           GAS_CANISTER_W, GAS_CANISTER_H))
                {
                    impact = true;
                    break;
                }
            }
        }
        if (!impact)
        {
            for (int j = 0; j < state->dog_count; ++j)
            {
                const Dog *dog = &state->dogs[j];
                if (!dog->dead &&
                    gameplay_boxes_overlap(swept_x, swept_y,
                                           swept_w, swept_h,
                                           dog->x, dog->y, DOG_W, DOG_H))
                {
                    impact = true;
                    break;
                }
            }
        }
        if (!impact)
        {
            for (int j = 0; j < state->enemy_count; ++j)
            {
                const Enemy *enemy = &state->enemies[j];
                if (!enemy->dead &&
                    gameplay_boxes_overlap(swept_x, swept_y,
                                           swept_w, swept_h,
                                           enemy->x, enemy->y,
                                           ENEMY_W, ENEMY_H))
                {
                    impact = true;
                    break;
                }
            }
        }

        if (impact)
            explode_rocket(state, campaign, rocket);
    }

    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        Bullet *bullet = &state->bullets[i];
        if (!bullet->active)
            continue;
        float previous_x = bullet->x;
        float previous_y = bullet->y;
        bullet->x += bullet->vx * dt;
        bullet->y += bullet->vy * dt;

        bool vertical = fabsf(bullet->vy) > fabsf(bullet->vx);
        float width = vertical ? (float)BULLET_H : (float)BULLET_W;
        float height = vertical ? (float)BULLET_W : (float)BULLET_H;
        /* Everything the round can hit is tested against the ground it crossed
         * this frame, not against where it ended up. A shot fired up a ladder
         * is 4px wide by 8 tall and a dog is 16 tall, so at the frame clamp the
         * two together are shorter than one step: tested at the destination
         * alone, the round arrives past the animal having never overlapped it.
         * The tile test above stays a point test — that one is proved by the
         * `_Static_assert`s beside the projectile speeds. */
        float swept_x = fminf(previous_x, bullet->x);
        float swept_y = fminf(previous_y, bullet->y);
        float swept_w = width + fabsf(bullet->x - previous_x);
        float swept_h = height + fabsf(bullet->y - previous_y);
        if (bullet->x + width < 0.0f ||
            bullet->x > state->level.map.width * (float)TILE_SIZE ||
            bullet->y + height < 0.0f ||
            bullet->y > state->level.map.height * (float)TILE_SIZE)
        {
            bullet->active = false;
            continue;
        }

        float leading_x = bullet->x + width * 0.5f;
        float leading_y = bullet->y + height * 0.5f;
        if (bullet->vx > 0.0f)
            leading_x = bullet->x + width - 1.0f;
        else if (bullet->vx < 0.0f)
            leading_x = bullet->x;
        if (bullet->vy > 0.0f)
            leading_y = bullet->y + height - 1.0f;
        else if (bullet->vy < 0.0f)
            leading_y = bullet->y;
        if (level_is_solid(&state->level,
                           (int)floorf(leading_x / TILE_SIZE),
                           (int)floorf(leading_y / TILE_SIZE)))
        {
            bullet->active = false;
            gameplay_world_sound(state, SFX_BULLET_IMPACT,
                                 bullet->x + width * 0.5f,
                                 bullet->y + height * 0.5f);
            continue;
        }

        for (int j = 0; j < state->level.runtime.crate_count; ++j)
        {
            Crate *crate = &state->level.runtime.crates[j];
            if (crate->active &&
                gameplay_boxes_overlap(swept_x, swept_y, swept_w, swept_h,
                                       crate->x, crate->y, CRATE_W, CRATE_H))
            {
                bullet->active = false;
                gameplay_destroy_crate(state, campaign, crate);
                break;
            }
        }
        if (!bullet->active)
            continue;

        for (int j = 0; j < state->level.runtime.gas_canister_count; ++j)
        {
            GasCanister *canister =
                &state->level.runtime.gas_canisters[j];
            if (canister->active &&
                gameplay_boxes_overlap(swept_x, swept_y, swept_w, swept_h,
                                       canister->x, canister->y,
                                       GAS_CANISTER_W, GAS_CANISTER_H))
            {
                bullet->active = false;
                explode_gas_canister(state, campaign, canister);
                break;
            }
        }
        if (!bullet->active)
            continue;

        for (int j = 0; j < state->dog_count; ++j)
        {
            Dog *dog = &state->dogs[j];
            if (!dog->dead &&
                gameplay_boxes_overlap(swept_x, swept_y, swept_w, swept_h,
                                       dog->x, dog->y, DOG_W, DOG_H))
            {
                bullet->active = false;
                damage_dog(state, campaign, dog);
                break;
            }
        }
        if (!bullet->active)
            continue;

        for (int j = 0; j < state->enemy_count; ++j)
        {
            Enemy *enemy = &state->enemies[j];
            if (!enemy->dead &&
                gameplay_boxes_overlap(swept_x, swept_y, swept_w, swept_h,
                                       enemy->x, enemy->y,
                                       ENEMY_W, ENEMY_H))
            {
                bullet->active = false;
                damage_enemy(state, campaign, j);
                break;
            }
        }
        if (!bullet->active)
            continue;

        /* Last, and only the player's rounds are tested against them: a guard
         * shooting his own building's cameras out would hand the player the one
         * answer to them for nothing, and it would happen constantly, since a
         * camera hangs over exactly the ground a guard shoots across. */
        for (int j = 0; j < state->level.map.camera_count; ++j)
        {
            if (!state->cameras[j].working)
                continue;
            float cx = 0.0f;
            float cy = 0.0f;
            float cw = 0.0f;
            float ch = 0.0f;
            gameplay_camera_box(&state->level.map.cameras[j], &cx, &cy, &cw,
                                &ch);
            if (gameplay_boxes_overlap(swept_x, swept_y, swept_w, swept_h,
                                       cx, cy, cw, ch))
            {
                bullet->active = false;
                gameplay_destroy_camera(state, campaign, j);
                break;
            }
        }
    }
}

/*
 * A guard's round hits the same world the player's does.
 *
 * It used to be tested against tiles, crates and Chuck, and against nothing
 * else — so a gas canister was neither cover nor a target on this side of the
 * fight. A round went straight through the steel as if it were air: the player
 * could not shelter behind one, and a guard could empty a clip past the
 * cylinder he was standing beside without ever setting it off. The manual
 * teaches "crawl and shoot a GAS CANISTER" as a rule about the world, and a
 * rule the world only obeys for one of the two people in the room is not a
 * rule, it is a special case nothing on screen explains.
 *
 * The low profile that makes the player crouch to hit one applies to the guard
 * unchanged, and that is what keeps this from being a difficulty change: a
 * guard aiming at a standing Chuck fires between 30% and 70% of body height
 * and passes over the cylinder exactly as the player's standing shot does. It
 * is the shot at a *crawling* Chuck that comes in low enough to find it — the
 * same trade the player makes, read from the other end.
 */
void gameplay_combat_update_enemy_bullets(GameplayState *state,
                                          CampaignState *campaign, float dt)
{
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        Bullet *bullet = &state->enemy_bullets[i];
        if (!bullet->active)
            continue;
        bullet->x += bullet->vx * dt;
        bullet->y += bullet->vy * dt;
        bool vertical = fabsf(bullet->vy) > fabsf(bullet->vx);
        float width = vertical ? (float)BULLET_H : (float)BULLET_W;
        float height = vertical ? (float)BULLET_W : (float)BULLET_H;
        if (bullet->x + width < 0.0f ||
            bullet->x > state->level.map.width * (float)TILE_SIZE ||
            bullet->y + height < 0.0f ||
            bullet->y > state->level.map.height * (float)TILE_SIZE)
        {
            bullet->active = false;
            continue;
        }

        int col = (int)floorf((bullet->x + width * 0.5f +
                               (bullet->vx > 0.0f   ? width * 0.5f - 1.0f
                                : bullet->vx < 0.0f ? -(width * 0.5f)
                                                    : 0.0f)) /
                              TILE_SIZE);
        int row = (int)floorf((bullet->y + height * 0.5f +
                               (bullet->vy > 0.0f   ? height * 0.5f - 1.0f
                                : bullet->vy < 0.0f ? -(height * 0.5f)
                                                    : 0.0f)) /
                              TILE_SIZE);
        if (level_is_solid(&state->level, col, row))
        {
            bullet->active = false;
            gameplay_world_sound(state, SFX_BULLET_IMPACT,
                                 bullet->x + width * 0.5f,
                                 bullet->y + height * 0.5f);
            continue;
        }
        for (int j = 0; j < state->level.runtime.crate_count; ++j)
        {
            const Crate *crate = &state->level.runtime.crates[j];
            if (crate->active &&
                gameplay_boxes_overlap(bullet->x, bullet->y,
                                       width, height,
                                       crate->x, crate->y,
                                       CRATE_W, CRATE_H))
            {
                bullet->active = false;
                gameplay_world_sound(state, SFX_BULLET_IMPACT,
                                     bullet->x + width * 0.5f,
                                     bullet->y + height * 0.5f);
                break;
            }
        }
        if (!bullet->active)
            continue;

        for (int j = 0; j < state->level.runtime.gas_canister_count; ++j)
        {
            GasCanister *canister =
                &state->level.runtime.gas_canisters[j];
            if (canister->active &&
                gameplay_boxes_overlap(bullet->x, bullet->y,
                                       width, height,
                                       canister->x, canister->y,
                                       GAS_CANISTER_W, GAS_CANISTER_H))
            {
                bullet->active = false;
                explode_gas_canister(state, campaign, canister);
                break;
            }
        }
        if (!bullet->active)
            continue;

        if (state->invuln_timer <= 0.0f &&
            gameplay_boxes_overlap(bullet->x, bullet->y,
                                   width, height,
                                   state->player.x, state->player.y,
                                   PLAYER_W, player_height(state)))
        {
            bullet->active = false;
            gameplay_damage_player(state, 1,
                                   bullet->x + width * 0.5f,
                                   bullet->y + height * 0.5f);
        }
    }
}

void gameplay_combat_check_contacts(GameplayState *state,
                                    CampaignState *campaign)
{
    if (state->player.dying)
        return;
    /* The mercy window is a shield, not a stun: it owes the player immunity
     * from what a guard does to him, and nothing at all about what he does to
     * the guard. Gating the whole check on it meant a stomp aimed during the
     * beat after any hit passed straight through the head it landed on —
     * neither a bounce nor a wound — which reads as the answer to a guard
     * failing at random. Only the damage paths below watch it. */
    bool mercy = state->invuln_timer > 0.0f;
    float height = player_height(state);
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead ||
            !gameplay_boxes_overlap(state->player.x, state->player.y,
                                    PLAYER_W, height,
                                    enemy->x, enemy->y,
                                    ENEMY_W, ENEMY_H))
            continue;

        /* Shallower vertical overlap than horizontal means Chuck fell onto
         * the guard's head rather than walking into its side: bounce off
         * instead of dying, and land a hit like any other attack. */
        float overlap_x = fminf(state->player.x + PLAYER_W,
                                enemy->x + ENEMY_W) -
                          fmaxf(state->player.x, enemy->x);
        float overlap_y = fminf(state->player.y + height,
                                enemy->y + ENEMY_H) -
                          fmaxf(state->player.y, enemy->y);
        /* Which of the two is on top, which a shallow overlap does not say on
         * its own — it reports only that the boxes just met on that axis.
         * Jumping up into a guard standing on the ledge above satisfies
         * everything else here the moment the rise turns into a fall, and used
         * to read as a stomp landing on a man who was over Chuck's head. */
        bool from_above = state->player.y + height * 0.5f <
                          enemy->y + ENEMY_H * 0.5f;
        /* A helmet is the one answer this man takes away. Everything else
         * about him is an ordinary guard with more rounds in him and less pace,
         * and that is deliberate: the stomp is the free kill — no ammunition,
         * no position, no noise — and it is what a player reaches for the
         * moment a floor gets busy. Landing on him costs the heart a side
         * contact costs instead, which is what the branch below already does. */
        if (state->player.vy > 0.0f && overlap_y < overlap_x && from_above &&
            enemy_kind_can_be_stomped(enemy->kind))
        {
            /* Climbing down onto a guard would otherwise just have the
             * ladder overwrite this with the climb speed next frame. */
            state->player.vy = -ENEMY_STOMP_BOUNCE_SPEED;
            state->player.on_ladder = false;
            state->player.ladder_direction = 0;
            state->player.ladder_lockout_timer = ENEMY_STOMP_LADDER_LOCKOUT;
            /* The bounce is not a jump: releasing the jump key must never
             * shorten it back down into the guard. */
            state->player.jump_cut_ok = false;
            damage_enemy(state, campaign, i);
            return;
        }

        /* Walking into a second guard costs nothing during the window, but the
         * scan carries on: one of the others may still be under his boots. */
        if (mercy)
            continue;

        gameplay_damage_player(state, 1,
                               enemy->x + ENEMY_W * 0.5f,
                               enemy->y + ENEMY_H * 0.5f);
        return;
    }
    if (mercy)
        return;
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (dog->dead || dog->bite_cooldown > 0.0f)
            continue;
        if (gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, height,
                                   dog->x, dog->y, DOG_W, DOG_H))
        {
            /* A bite is announced: the first contact only starts the crouch
             * and growl, and the teeth land a beat later if Chuck is still
             * there. The windup ticks (and is cancelled by escape) in the dog
             * AI update, which owns dt. */
            if (!dog->bite_ready)
            {
                if (dog->bite_windup <= 0.0f)
                {
                    dog->bite_windup = DOG_BITE_WINDUP;
                    gameplay_world_sound(state, SFX_DOG_GROWL,
                                         dog->x + DOG_W * 0.5f,
                                         dog->y + DOG_H * 0.5f);
                }
                continue;
            }
            dog->bite_ready = false;
            dog->bite_cooldown = DOG_BITE_COOLDOWN;
            dog->attack_timer = 0.18f;
            dog->state = DOG_CHASE;
            dog->lost_timer = DOG_LOST_TIME;
            dog->chase_target_x = state->player.x + PLAYER_W * 0.5f;
            dog->has_chase_target = true;
            gameplay_world_sound(state, SFX_DOG_BITE,
                                 dog->x + DOG_W * 0.5f,
                                 dog->y + DOG_H * 0.5f);
            gameplay_damage_player(state, 1,
                                   dog->x + DOG_W * 0.5f,
                                   dog->y + DOG_H * 0.5f);
            return;
        }
    }
}
