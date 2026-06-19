#include "gameplay_world.h"

#include "gameplay_ai.h"
#include "gameplay_climb.h"

#include <math.h>

bool gameplay_boxes_overlap(float ax, float ay, float aw, float ah,
                            float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

static bool point_in_active_crate(const GameplayState *state,
                                  float px, float py)
{
    for (int i = 0; i < state->level.runtime.crate_count; ++i)
    {
        const Crate *crate = &state->level.runtime.crates[i];
        if (crate->active &&
            px >= crate->x && px < crate->x + CRATE_W &&
            py >= crate->y && py < crate->y + CRATE_H)
            return true;
    }
    return false;
}

bool gameplay_sight_line_clear(const GameplayState *state,
                               float ax, float ay, float bx, float by)
{
    float dx = bx - ax;
    float dy = by - ay;
    float dist = sqrtf(dx * dx + dy * dy);
    int steps = (int)(dist / ENEMY_LOS_STEP) + 1;
    for (int s = 1; s < steps; ++s)
    {
        float t = (float)s / (float)steps;
        float px = ax + dx * t;
        float py = ay + dy * t;
        if (level_is_solid(&state->level, (int)floorf(px / TILE_SIZE),
                           (int)floorf(py / TILE_SIZE)))
            return false;
        if (point_in_active_crate(state, px, py))
            return false;
    }
    return true;
}

void gameplay_world_sound(GameplayState *state, SoundEffect effect,
                          float x, float y)
{
    game_events_world_sound(&state->events, effect, x, y);
}

void gameplay_crew_chatter(GameplayState *state, ChatterKind kind, int speaker,
                           float x, float y)
{
    /* The draw is made here rather than at each call site so every kind of
     * line costs the seeded stream exactly one number, whatever the shell
     * later does with it. `rng_range` and not the table size: the count of
     * lines is presentation, and a gameplay module that knew it would have to
     * be recompiled — and re-tested for determinism — every time somebody
     * wrote another one. */
    game_events_chatter(&state->events, x, y, speaker, kind,
                        rng_range(&state->rng, 1 << 15));
}

bool gameplay_alarm_active(const GameplayState *state)
{
    return state->terminal_alarm_timer > 0.0f;
}

void gameplay_trigger_alarm(GameplayState *state, float source_x,
                            float source_y, int switch_index)
{
    bool was_active = gameplay_alarm_active(state);
    state->terminal_alarm_timer = ALARM_CALM_TIME;
    state->alarm_target_x = source_x;
    state->alarm_target_y = source_y;
    state->active_alarm_switch = switch_index;
    state->alarm_siren_timer = ALARM_SIREN_INTERVAL;
    if (!was_active)
    {
        if (switch_index >= 0)
            gameplay_world_sound(state, SFX_CARD_SCAN, source_x, source_y);
        /* A building alarm is ambient infrastructure, not a sound emitted
         * only at the call point. Keep the siren audible everywhere. */
        game_events_sound(&state->events, SFX_TERMINAL_ALARM);
        for (int i = 0; i < state->enemy_count; ++i)
        {
            Enemy *enemy = &state->enemies[i];
            if (enemy->dead)
                continue;
            if (enemy->shoot_cooldown > ENEMY_ALARM_INITIAL_SHOT_DELAY)
                enemy->shoot_cooldown = ENEMY_ALARM_INITIAL_SHOT_DELAY;
            float alarm_aim_time =
                ENEMY_AIM_TIME * ENEMY_ALARM_AIM_MULTIPLIER;
            if (enemy->aim_timer > alarm_aim_time)
                enemy->aim_timer = alarm_aim_time;
        }
    }
}

void gameplay_refresh_alarm_from_player(GameplayState *state)
{
    if (!gameplay_alarm_active(state))
        return;
    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    state->terminal_alarm_timer = ALARM_CALM_TIME;
    state->alarm_target_x = state->player.x + PLAYER_W * 0.5f;
    state->alarm_target_y = state->player.y + height * 0.5f;
}

void gameplay_update_alarm(GameplayState *state, float dt)
{
    if (!gameplay_alarm_active(state))
        return;

    state->terminal_alarm_timer -= dt;
    if (state->terminal_alarm_timer <= 0.0f)
    {
        state->terminal_alarm_timer = 0.0f;
        state->alarm_siren_timer = 0.0f;
        state->active_alarm_switch = -1;
        state->terminal_reinforcement_timer = 0.0f;
        state->terminal_reinforcements_pending = 0;
        return;
    }

    state->alarm_siren_timer -= dt;
    if (state->alarm_siren_timer <= 0.0f)
    {
        game_events_sound(&state->events, SFX_TERMINAL_ALARM);
        state->alarm_siren_timer += ALARM_SIREN_INTERVAL;
    }
}

void gameplay_hit_player(GameplayState *state)
{
    if (state->player.dying)
        return;

    state->player.hp = 0;
    state->terminal_hack_progress = 0.0f;
    state->terminal_hack_tick_timer = 0.0f;
    state->terminal_in_range = false;
    state->terminal_hacking = false;
    state->terminal_alarm_timer = 0.0f;
    state->alarm_siren_timer = 0.0f;
    state->active_alarm_switch = -1;
    state->terminal_reinforcement_timer = 0.0f;
    state->terminal_reinforcements_pending = 0;

    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    float x = state->player.x + PLAYER_W * 0.5f;
    float y = state->player.y + height * 0.5f;
    game_events_particles(&state->events, x, y, 32, state->player.facing);
    game_events_sound(&state->events, SFX_PLAYER_HIT);
    state->player.dying = true;
    state->player.death_timer = 0.75f;
}

void gameplay_damage_player(GameplayState *state, int amount,
                            float source_x, float source_y)
{
    if (state->player.dying || state->invuln_timer > 0.0f)
        return;

    state->player.hp -= amount;
    if (state->player.hp <= 0)
    {
        gameplay_hit_player(state);
        return;
    }

    /* Survivable: open the mercy window and pop the player off the source.
     * Horizontal knockback would be overwritten by the walk input on the very
     * next frame, so only the vertical pop is real; on a ladder or on the
     * facade even that would fight the climb, so those keep their footing. */
    state->invuln_timer = PLAYER_HIT_INVULN;
    float height = state->player.crawling
                       ? (float)PLAYER_CRAWL_H
                       : (float)PLAYER_H;
    float x = state->player.x + PLAYER_W * 0.5f;
    float y = state->player.y + height * 0.5f;
    if (!state->player.on_ladder && !state->player.facade_climbing)
    {
        state->player.vy = y <= source_y
                               ? -PLAYER_HIT_KNOCKBACK_Y
                               : PLAYER_HIT_KNOCKBACK_Y * 0.35f;
        state->player.jump_cut_ok = false;
    }
    int direction = x < source_x ? -1 : 1;
    game_events_particles(&state->events, x, y, 12, direction);
    game_events_sound(&state->events, SFX_PLAYER_HIT);
}

void gameplay_bank_checkpoint(GameplayState *state)
{
    if (state->level.map.mode == LEVEL_MODE_FACADE)
        return;
    state->interior_checkpoint_x = state->player.x;
    state->interior_checkpoint_y = state->player.y;
    state->interior_has_checkpoint = true;
}

void gameplay_restore_checkpoint(GameplayState *state)
{
    if (state->level.map.mode == LEVEL_MODE_FACADE)
    {
        gameplay_climb_restore_checkpoint(state);
        return;
    }
    /*
     * Nothing already in flight may greet the respawn — and the map's own
     * start tile is a respawn as much as a banked checkpoint is.
     *
     * This clear used to sit *below* the guard, so it only ran for a player
     * who had already banked something. That made the one death the sector
     * gives nothing back for — before the first card, terminal, door or
     * medkit — also the only one the rule was not kept for: `player_reset`
     * had put Chuck back on `S` and the rounds that killed him were still
     * crossing the room. `finish_player_death` has already moved him either
     * way, so there is always a respawn here to protect.
     */
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
        state->enemy_bullets[i].active = false;

    if (!state->interior_has_checkpoint)
        return;

    state->player.x = state->interior_checkpoint_x;
    state->player.y = state->interior_checkpoint_y;
    state->player.vx = 0.0f;
    state->player.vy = 0.0f;
}

void gameplay_spawn_ammo_drop(GameplayState *state, float x, float y)
{
    for (int i = 0; i < MAX_AMMO_DROPS; ++i)
    {
        AmmoDrop *drop = &state->ammo_drops[i];
        if (drop->active)
            continue;
        drop->x = x - AMMO_DROP_W * 0.5f;
        drop->y = y;
        drop->vy = 0.0f;
        drop->active = true;
        return;
    }
}

void gameplay_update_ammo_drops(GameplayState *state, float dt)
{
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    for (int i = 0; i < MAX_AMMO_DROPS; ++i)
    {
        AmmoDrop *drop = &state->ammo_drops[i];
        if (!drop->active)
            continue;

        drop->vy += GRAVITY * dt;
        if (drop->vy > MAX_FALL_SPEED)
            drop->vy = MAX_FALL_SPEED;
        float vx = 0.0f;
        bool on_ground = false;
        level_move(&state->level, &drop->x, &drop->y, &vx, &drop->vy,
                   AMMO_DROP_W, AMMO_DROP_H, dt, false, &on_ground, false,
                   STANCE_UPRIGHT);
        if (on_ground)
            drop->vy = 0.0f;

        /* Left lying until it is actually useful, so a full magazine does not
         * eat the pickup. */
        if (state->player.bullets >= MAX_AMMO || state->player.dying)
            continue;
        if (gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, player_h,
                                   drop->x, drop->y,
                                   AMMO_DROP_W, AMMO_DROP_H))
        {
            drop->active = false;
            /* The same exception the boxed magazine gets: rounds handed to a
             * man holding a knife because his clip is dry raise the sidearm,
             * and nothing else a pickup does may change the weapon in hand. */
            if (state->player.bullets == 0 &&
                state->player.active_weapon == PLAYER_WEAPON_KNIFE)
                state->player.active_weapon = PLAYER_WEAPON_PISTOL;
            state->player.bullets += AMMO_DROP_BULLETS;
            if (state->player.bullets > MAX_AMMO)
                state->player.bullets = MAX_AMMO;
            game_events_sound(&state->events, SFX_PICKUP_AMMO);
        }
    }
}

void gameplay_handle_player_landing(GameplayState *state, bool was_grounded,
                                    float fall_speed)
{
    if (was_grounded || !state->player.on_ground)
        return;

    if (fall_speed >= PLAYER_FATAL_FALL_SPEED)
    {
        gameplay_hit_player(state);
        return;
    }

    if (fall_speed > PLAYER_LAND_SOUND_SPEED)
        game_events_sound(&state->events, SFX_LAND);
}

void gameplay_unlock_exit(GameplayState *state)
{
    /* In an interior with an escape window the security door is physically
     * barricaded. Cards and terminals remain usable/scorable, but cannot turn
     * that door into the route forward. */
    if (state->level.map.has_window || state->level.runtime.exit_unlocked)
        return;
    state->level.runtime.exit_unlocked = true;
    state->terminal_in_range = false;
    state->terminal_hacking = false;
    game_events_sound(&state->events, SFX_EXIT_UNLOCKED);
}

bool gameplay_player_near_active_terminal(const GameplayState *state)
{
    int index = state->level.runtime.active_terminal_index;
    if (state->level.runtime.exit_unlocked ||
        state->level.runtime.terminal_hacked ||
        index < 0 || index >= state->level.map.terminal_count)
    {
        return false;
    }

    const Terminal *terminal = &state->level.map.terminals[index];
    float terminal_x = terminal->col * (float)TILE_SIZE + TILE_SIZE * 0.5f;
    float terminal_y = terminal->row * (float)TILE_SIZE + TILE_SIZE * 0.5f;
    float player_h = state->player.crawling
                         ? (float)PLAYER_CRAWL_H
                         : (float)PLAYER_H;
    float player_x = state->player.x + PLAYER_W * 0.5f;
    float player_y = state->player.y + player_h * 0.5f;
    return fabsf(player_x - terminal_x) <= TERMINAL_INTERACT_RANGE &&
           fabsf(player_y - terminal_y) <= TILE_SIZE * 0.65f;
}

void gameplay_provoke_enemy(GameplayState *state, int enemy_index)
{
    if (enemy_index < 0 || enemy_index >= state->enemy_count)
        return;

    Enemy *attacked = &state->enemies[enemy_index];
    int participant_count = attacked->talking ? 2 : 1;
    int participants[2] = {enemy_index, attacked->talk_partner};
    float target_x = state->player.x + PLAYER_W * 0.5f;
    float target_y = state->player.y +
                     (state->player.crawling
                          ? (float)PLAYER_CRAWL_H * 0.45f
                          : (float)PLAYER_H * 0.15f);
    Enemy *alert_source = NULL;

    for (int i = 0; i < participant_count; ++i)
    {
        int index = participants[i];
        if (index < 0 || index >= state->enemy_count ||
            (i > 0 && index == participants[0]))
            continue;

        Enemy *enemy = &state->enemies[index];
        enemy->raising_alarm = false;
        enemy->alarm_switch_index = -1;
        enemy->alarm_use_timer = 0.0f;
        enemy->alarm_run_timer = 0.0f;
        enemy->alarm_switches_tried = 0;
        enemy->encounter_decided = true;
        enemy->encounter_lost_timer = GUARD_ENCOUNTER_RESET_TIME;
        enemy->talking = false;
        enemy->talk_timer = 0.0f;
        enemy->talk_partner = -1;
        enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
        if (enemy->dead)
            continue;

        float enemy_x = enemy->x + ENEMY_W * 0.5f;
        enemy->provoked = true;
        enemy->pursuit_target_x = target_x;
        enemy->pursuit_target_y = target_y;
        enemy->has_pursuit_target = true;
        enemy->dir = target_x < enemy_x ? -1 : 1;
        /*
         * And he aims where the player *is*, which for as long as this was a
         * flat `aim_vdir = 0` he did not.
         *
         * A round on the horizontal axis leaves the muzzle clamped to chest
         * height (`ENEMY_MUZZLE_MIN_Y_FACTOR`..`MAX`), so it can only ever
         * reach somebody standing roughly level with him — and the stomp is
         * the one attack in the game delivered from the one place that round
         * cannot go. Measured before this line existed: a guard answered every
         * bounce off his own head, on the frame the player could see the
         * telegraph, with a shot 43px under the player's boots — twice inside
         * the 1.5 seconds a three-stomp kill takes, and never once a hit. He
         * was not defenceless in the code, only in effect, which is why
         * nothing had ever noticed.
         *
         * `gameplay_ai_aim_at_player` is the axis and the aim together. Note
         * what it does *not* ask: whether he can see Chuck. Being shot,
         * knifed, crated or landed on has already answered that, and the whole
         * of this function is what a guard knows the instant he is hit.
         */
        gameplay_ai_aim_at_player(state, index);
        if (alert_source == NULL)
            alert_source = enemy;
    }

    if (alert_source != NULL)
    {
        gameplay_world_sound(state, SFX_ENEMY_ALERT,
                             alert_source->x + ENEMY_W * 0.5f,
                             alert_source->y + ENEMY_H * 0.5f);
    }
}

void gameplay_alert_enemies_to_noise(GameplayState *state, float x, float y,
                                     float radius)
{
    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (enemy->dead || enemy->raising_alarm || enemy->provoked)
            continue;
        float ex = enemy->x + ENEMY_W * 0.5f;
        float ey = enemy->y + ENEMY_H * 0.5f;
        float dx = x - ex;
        float dy = y - ey;
        if (dx * dx + dy * dy > radius * radius)
            continue;
        /* Break off a conversation and turn toward the sound. */
        enemy->talking = false;
        enemy->talk_timer = 0.0f;
        if (enemy->talk_cooldown <= 0.0f)
            enemy->talk_cooldown = ENEMY_TALK_COOLDOWN;
        enemy->investigate_x = x;
        enemy->investigate_y = y;
        enemy->investigate_timer = ENEMY_INVESTIGATE_TIME;
        enemy->investigate_scan_timer = ENEMY_INVESTIGATE_SCAN_FLIP;
        enemy->dir = dx < 0.0f ? -1 : 1;
    }
}

void gameplay_destroy_crate(GameplayState *state, CampaignState *campaign,
                            Crate *crate)
{
    if (!crate->active)
        return;
    crate->active = false;
    crate->vx = 0.0f;
    crate->vy = 0.0f;
    float x = crate->x + CRATE_W * 0.5f;
    float y = crate->y + CRATE_H * 0.5f;
    game_events_explosion(&state->events, x, y, 18);
    gameplay_world_sound(state, SFX_CRATE_BREAK, x, y);
    campaign->score += CRATE_SCORE;
}

void gameplay_camera_box(const SecurityCamera *camera, float *x, float *y,
                         float *w, float *h)
{
    *x = (camera->col + 0.5f) * (float)TILE_SIZE - CAMERA_W * 0.5f;
    /* Hung from the slab above, so it sits at the top of its own tile rather
     * than in the middle of it — which is where it is drawn and where a shot
     * aimed at the thing on the ceiling actually goes. */
    *y = (float)camera->row * (float)TILE_SIZE + 2.0f;
    *w = CAMERA_W;
    *h = CAMERA_H;
}

/*
 * A camera coming off the ceiling.
 *
 * **Shooting one is a decision rather than a freebie**, and the cost is built
 * into the act rather than bolted on: a shot is the loudest thing the player
 * can do, `gameplay_alert_enemies_to_noise` is already listening to it, and the
 * only reason to fire is that the lens is looking somewhere Chuck has to be. So
 * the sector trades a permanent problem for an immediate one, which is the same
 * bargain the weak wall makes with an explosive.
 *
 * It is worth points for the reason a weak wall is: it is a piece of the
 * building the player took apart on purpose, and the score is the only thing
 * that says the game noticed. Deliberately less than a guard — this is
 * furniture, and a scoring route that ran on shooting fittings would be a worse
 * game than one that ran on the men.
 */
void gameplay_destroy_camera(GameplayState *state, CampaignState *campaign,
                             int index)
{
    if (index < 0 || index >= state->level.map.camera_count)
        return;
    CameraState *cam = &state->cameras[index];
    if (!cam->working)
        return;

    cam->working = false;
    cam->notice = 0.0f;
    cam->suspicion = 0.0f;

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    gameplay_camera_box(&state->level.map.cameras[index], &x, &y, &w, &h);
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    game_events_explosion(&state->events, cx, cy, 12);
    gameplay_world_sound(state, SFX_CRATE_BREAK, cx, cy);
    campaign->score += CAMERA_SCORE;
}

int gameplay_break_walls_in_radius(GameplayState *state,
                                   CampaignState *campaign,
                                   float x, float y, float radius)
{
    Level *level = &state->level;
    int first_col = (int)floorf((x - radius) / TILE_SIZE);
    int last_col = (int)floorf((x + radius) / TILE_SIZE);
    int first_row = (int)floorf((y - radius) / TILE_SIZE);
    int last_row = (int)floorf((y + radius) / TILE_SIZE);
    int broken = 0;

    for (int row = first_row; row <= last_row; ++row)
    {
        for (int col = first_col; col <= last_col; ++col)
        {
            /* Measured to the tile's centre, so a blast beside a wall takes out
             * the panel it went off against and its immediate neighbours rather
             * than every tile its bounding box happens to touch. */
            float centre_x = (col + 0.5f) * (float)TILE_SIZE;
            float centre_y = (row + 0.5f) * (float)TILE_SIZE;
            float dx = centre_x - x;
            float dy = centre_y - y;
            if (dx * dx + dy * dy > radius * radius)
                continue;
            if (!level_break_wall(level, col, row))
                continue;
            broken++;
            campaign->score += WEAK_WALL_SCORE;
            game_events_dust(&state->events, centre_x, centre_y,
                             WEAK_WALL_DUST, (float)TILE_SIZE);
        }
    }

    /* One report per blast, not one per tile: a wall that goes in three pieces
     * is still one wall coming down, and sixteen voices would be spent on it. */
    if (broken > 0)
        gameplay_world_sound(state, SFX_WALL_BREAK, x, y);
    return broken;
}

/*
 * One man down, counted twice, and the two counts answer different questions.
 *
 * The sector's own tally is what the report between floors prints, so it is
 * wiped with the sector. The run's is what the crew's net reads — twelve men
 * who have noticed how few of them are answering — and it has to outlive the
 * floor they were lost on, so it sits in `CampaignState` beside the score.
 * Both are bumped here rather than at the six kill sites, which is the whole
 * reason this function exists.
 *
 * `campaign` is not optional, and used to be checked here as though it were —
 * the two crate kills below and every kill in
 * [gameplay_combat.c](gameplay_combat.c) score straight through the same pointer
 * with no guard at all, so a caller who honoured the invitation would have
 * reached one of those instead. A check that only one of seven call sites keeps
 * is not a defence, it is a note claiming the parameter is optional when it is
 * not.
 */
void gameplay_record_neutralized(GameplayState *state,
                                 CampaignState *campaign)
{
    state->hostiles_neutralized++;
    campaign->hostiles_down++;
}

void gameplay_kill_enemy_with_crate(GameplayState *state,
                                    CampaignState *campaign, Enemy *enemy)
{
    if (enemy->dead)
        return;
    enemy->hp = 0;
    enemy->dead = true;
    gameplay_record_neutralized(state, campaign);
    float x = enemy->x + ENEMY_W * 0.5f;
    float y = enemy->y + ENEMY_H * 0.5f;
    game_events_particles(&state->events, x, y, 24, enemy->dir);
    gameplay_world_sound(state, SFX_ENEMY_DOWN, x, y);
    campaign->score += ENEMY_SCORE;
}

void gameplay_kill_dog_with_crate(GameplayState *state,
                                  CampaignState *campaign, Dog *dog)
{
    if (dog->dead)
        return;
    dog->hp = 0;
    dog->dead = true;
    gameplay_record_neutralized(state, campaign);
    float x = dog->x + DOG_W * 0.5f;
    float y = dog->y + DOG_H * 0.5f;
    game_events_particles(&state->events, x, y, 14, dog->dir);
    gameplay_world_sound(state, SFX_DOG_YELP, x, y);
    campaign->score += DOG_SCORE;
}
