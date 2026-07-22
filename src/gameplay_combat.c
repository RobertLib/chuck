#include "gameplay_combat.h"

#include "gameplay_world.h"

#include <math.h>

static float player_height(const GameplayState *state)
{
    return state->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
}

static bool within_radius(float x, float y, float center_x, float center_y,
                          float radius)
{
    float dx = x - center_x;
    float dy = y - center_y;
    return dx * dx + dy * dy <= radius * radius;
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

static void explode_grenade(GameplayState *state, CampaignState *campaign,
                            Grenade *grenade)
{
    grenade->active = false;
    float x = grenade->x + GRENADE_W * 0.5f;
    float y = grenade->y + GRENADE_H * 0.5f;
    game_events_explosion(&state->events, x, y, 64);
    gameplay_world_sound(state, SFX_EXPLOSION, x, y);
    game_events_camera_shake(&state->events, 7.0f, 0.30f);

    for (int i = 0; i < state->enemy_count; ++i)
    {
        Enemy *enemy = &state->enemies[i];
        if (!enemy->dead &&
            within_radius(enemy->x + ENEMY_W * 0.5f,
                          enemy->y + ENEMY_H * 0.5f,
                          x, y, GRENADE_RADIUS))
        {
            enemy->hp = 0;
            enemy->dead = true;
            game_events_particles(&state->events,
                                  enemy->x + ENEMY_W * 0.5f,
                                  enemy->y + ENEMY_H * 0.5f,
                                  24, enemy->dir);
            campaign->score += 150;
        }
    }
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (!dog->dead &&
            within_radius(dog->x + DOG_W * 0.5f,
                          dog->y + DOG_H * 0.5f,
                          x, y, GRENADE_RADIUS))
        {
            dog->hp = 0;
            dog->dead = true;
            game_events_particles(&state->events,
                                  dog->x + DOG_W * 0.5f,
                                  dog->y + DOG_H * 0.5f,
                                  14, dog->dir);
            gameplay_world_sound(state, SFX_DOG_YELP,
                                 dog->x + DOG_W * 0.5f,
                                 dog->y + DOG_H * 0.5f);
            campaign->score += 75;
        }
    }
    damage_crates_in_radius(state, campaign, x, y, GRENADE_RADIUS);
    if (state->invuln_timer <= 0.0f &&
        within_radius(state->player.x + PLAYER_W * 0.5f,
                      state->player.y + player_height(state) * 0.5f,
                      x, y, GRENADE_RADIUS))
        gameplay_hit_player(state);
}

void gameplay_combat_update_explosives(GameplayState *state,
                                       CampaignState *campaign, float dt)
{
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

        mine->active = false;
        float x = mine->x + MINE_W * 0.5f;
        float y = mine->y + MINE_H * 0.5f;
        game_events_explosion(&state->events, x, y, 48);
        gameplay_world_sound(state, SFX_EXPLOSION, x, y);
        game_events_camera_shake(&state->events, 5.0f, 0.24f);
        damage_crates_in_radius(state, campaign, x, y, MINE_RADIUS);
        if (state->invuln_timer <= 0.0f &&
            within_radius(state->player.x + PLAYER_W * 0.5f,
                          state->player.y + player_height(state) * 0.5f,
                          x, y, MINE_RADIUS))
            gameplay_hit_player(state);
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
                   GRENADE_W, GRENADE_H, dt, false, &on_ground, false);
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

void gameplay_combat_handle_player_action(GameplayState *state,
                                          CampaignState *campaign,
                                          Input *input)
{
    (void)campaign;
    if (!input->shoot)
        return;

    if (state->player.grenades > 0)
    {
        int slot = find_grenade_slot(state);
        if (slot >= 0)
        {
            Grenade *grenade = &state->grenades[slot];
            grenade->active = true;
            grenade->timer = GRENADE_FUSE_TIME;
            grenade->fuse_sound_timer = 0.22f;
            grenade->grounded = false;
            grenade->y = state->player.y + player_height(state) * 0.45f;
            float speed = GRENADE_THROW_SPEED * 0.9f;
            grenade->vx = state->player.facing > 0 ? speed : -speed;
            grenade->x = state->player.x +
                         (state->player.facing > 0
                              ? PLAYER_W + 6.0f
                              : -(GRENADE_W + 6.0f));
            float vertical = 160.0f * GRAVITY /
                             (2.0f * fabsf(grenade->vx));
            if (vertical < 30.0f)
                vertical = 30.0f;
            if (vertical > 220.0f)
                vertical = 220.0f;
            grenade->vy = -vertical;
            state->player.grenades = 0;
            state->player.shot_vertical = 0;
            state->player.action_timer = 0.18f;
            game_events_sound(&state->events, SFX_GRENADE_THROW);
        }
    }
    else if (state->player.bullets > 0)
    {
        for (int i = 0; i < MAX_BULLETS; ++i)
        {
            Bullet *bullet = &state->bullets[i];
            if (bullet->active)
                continue;
            bullet->active = true;
            int vertical = 0;
            if (state->player.on_ladder)
            {
                if (input->up && !input->down)
                    vertical = -1;
                else if (input->down && !input->up)
                    vertical = 1;
            }
            if (vertical != 0)
            {
                bullet->x = state->player.x +
                            (PLAYER_W - BULLET_H) * 0.5f;
                bullet->y = vertical < 0
                                ? state->player.y - BULLET_W
                                : state->player.y + PLAYER_H;
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
            state->player.action_timer = 0.12f;
            game_events_sound(&state->events, SFX_PLAYER_SHOT);
            break;
        }
    }
    else
        game_events_sound(&state->events, SFX_EMPTY_CLICK);
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
            gameplay_hit_player(state);
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
            gameplay_hit_player(state);
            return;
        }
    }
}

void gameplay_combat_update_player_bullets(GameplayState *state,
                                           CampaignState *campaign,
                                           float dt)
{
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        Bullet *bullet = &state->bullets[i];
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
                gameplay_boxes_overlap(bullet->x, bullet->y, width, height,
                                       crate->x, crate->y, CRATE_W, CRATE_H))
            {
                bullet->active = false;
                gameplay_destroy_crate(state, campaign, crate);
                break;
            }
        }
        if (!bullet->active)
            continue;

        for (int j = 0; j < state->dog_count; ++j)
        {
            Dog *dog = &state->dogs[j];
            if (!dog->dead &&
                gameplay_boxes_overlap(bullet->x, bullet->y, width, height,
                                       dog->x, dog->y, DOG_W, DOG_H))
            {
                bullet->active = false;
                dog->hp--;
                if (dog->hp <= 0)
                {
                    dog->dead = true;
                    campaign->score += 75;
                    game_events_particles(&state->events,
                                          dog->x + DOG_W * 0.5f,
                                          dog->y + DOG_H * 0.5f,
                                          14, dog->dir);
                }
                gameplay_world_sound(state, SFX_DOG_YELP,
                                     dog->x + DOG_W * 0.5f,
                                     dog->y + DOG_H * 0.5f);
                break;
            }
        }
        if (!bullet->active)
            continue;

        for (int j = 0; j < state->enemy_count; ++j)
        {
            Enemy *enemy = &state->enemies[j];
            if (!enemy->dead &&
                gameplay_boxes_overlap(bullet->x, bullet->y, width, height,
                                       enemy->x, enemy->y,
                                       ENEMY_W, ENEMY_H))
            {
                bullet->active = false;
                enemy->hp--;
                if (enemy->hp <= 0)
                {
                    enemy->dead = true;
                    campaign->score += 150;
                    game_events_particles(&state->events,
                                          enemy->x + ENEMY_W * 0.5f,
                                          enemy->y + ENEMY_H * 0.5f,
                                          24, enemy->dir);
                    gameplay_world_sound(state, SFX_ENEMY_DOWN,
                                         enemy->x + ENEMY_W * 0.5f,
                                         enemy->y + ENEMY_H * 0.5f);
                }
                else
                    gameplay_world_sound(state, SFX_ENEMY_HIT,
                                         enemy->x + ENEMY_W * 0.5f,
                                         enemy->y + ENEMY_H * 0.5f);
                gameplay_provoke_enemy(state, j);
                break;
            }
        }
    }
}

void gameplay_combat_update_enemy_bullets(GameplayState *state, float dt)
{
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        Bullet *bullet = &state->enemy_bullets[i];
        if (!bullet->active)
            continue;
        bullet->x += bullet->vx * dt;
        bullet->y += bullet->vy * dt;
        if (bullet->x + BULLET_W < 0.0f ||
            bullet->x > state->level.map.width * (float)TILE_SIZE ||
            bullet->y + BULLET_H < 0.0f)
        {
            bullet->active = false;
            continue;
        }

        int col = (int)floorf((bullet->x +
                               (bullet->vx > 0.0f ? BULLET_W - 1 : 0)) /
                              TILE_SIZE);
        int row = (int)floorf((bullet->y + BULLET_H * 0.5f) / TILE_SIZE);
        if (level_is_solid(&state->level, col, row))
        {
            bullet->active = false;
            gameplay_world_sound(state, SFX_BULLET_IMPACT,
                                 bullet->x + BULLET_W * 0.5f,
                                 bullet->y + BULLET_H * 0.5f);
            continue;
        }
        for (int j = 0; j < state->level.runtime.crate_count; ++j)
        {
            const Crate *crate = &state->level.runtime.crates[j];
            if (crate->active &&
                gameplay_boxes_overlap(bullet->x, bullet->y,
                                       BULLET_W, BULLET_H,
                                       crate->x, crate->y,
                                       CRATE_W, CRATE_H))
            {
                bullet->active = false;
                gameplay_world_sound(state, SFX_BULLET_IMPACT,
                                     bullet->x + BULLET_W * 0.5f,
                                     bullet->y + BULLET_H * 0.5f);
                break;
            }
        }
        if (bullet->active && state->invuln_timer <= 0.0f &&
            gameplay_boxes_overlap(bullet->x, bullet->y,
                                   BULLET_W, BULLET_H,
                                   state->player.x, state->player.y,
                                   PLAYER_W, player_height(state)))
        {
            bullet->active = false;
            gameplay_hit_player(state);
        }
    }
}

void gameplay_combat_check_contacts(GameplayState *state)
{
    if (state->invuln_timer > 0.0f)
        return;
    float height = player_height(state);
    for (int i = 0; i < state->enemy_count; ++i)
    {
        const Enemy *enemy = &state->enemies[i];
        if (!enemy->dead &&
            gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, height,
                                   enemy->x, enemy->y,
                                   ENEMY_W, ENEMY_H))
        {
            gameplay_hit_player(state);
            return;
        }
    }
    for (int i = 0; i < state->dog_count; ++i)
    {
        Dog *dog = &state->dogs[i];
        if (dog->dead || dog->bite_cooldown > 0.0f)
            continue;
        if (gameplay_boxes_overlap(state->player.x, state->player.y,
                                   PLAYER_W, height,
                                   dog->x, dog->y, DOG_W, DOG_H))
        {
            dog->bite_cooldown = DOG_BITE_COOLDOWN;
            dog->attack_timer = 0.18f;
            dog->state = DOG_CHASE;
            dog->lost_timer = DOG_LOST_TIME;
            dog->chase_target_x = state->player.x + PLAYER_W * 0.5f;
            dog->has_chase_target = true;
            gameplay_world_sound(state, SFX_DOG_BITE,
                                 dog->x + DOG_W * 0.5f,
                                 dog->y + DOG_H * 0.5f);
            gameplay_hit_player(state);
            return;
        }
    }
}
