#include "game.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

static const char *LEVEL_FILES[LEVEL_COUNT] = {
    "levels/level1.txt",
    "levels/level2.txt"};

static bool boxes_overlap(float ax, float ay, float aw, float ah,
                          float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

/* Find a free slot in the enemies array: reuse a dead slot or extend the array. */
static int find_enemy_slot(Game *game)
{
    for (int i = 0; i < game->enemy_count; ++i)
        if (game->enemies[i].dead)
            return i;
    if (game->enemy_count < MAX_ENEMIES)
        return game->enemy_count++;
    return -1;
}

/*
 * Returns true when the enemy has an unobstructed horizontal view of the
 * player: same floor level (within 1 tile of Y), within ENEMY_SHOOT_RANGE,
 * and no solid tile along the row between them.
 */
static bool enemy_has_los(const Game *game, const Enemy *e)
{
    const Player *p = &game->player;
    float ex = e->x + ENEMY_W * 0.5f;
    float ey = e->y + ENEMY_H * 0.5f;
    float px = p->x + PLAYER_W * 0.5f;
    float py = p->y + PLAYER_H * 0.5f;

    if (fabsf(px - ex) > (float)ENEMY_SHOOT_RANGE)
        return false;
    if (fabsf(py - ey) > (float)TILE_SIZE * 1.2f)
        return false;

    /* Enemy must be facing toward the player */
    float dx = px - ex;
    if ((dx > 0.0f && e->dir < 0) || (dx < 0.0f && e->dir > 0))
        return false;

    /* Scan all tile columns between enemy and player at enemy mid-row */
    int row = (int)floorf(ey / TILE_SIZE);
    int col0 = (int)floorf(fminf(ex, px) / TILE_SIZE);
    int col1 = (int)floorf(fmaxf(ex, px) / TILE_SIZE);
    for (int c = col0; c <= col1; ++c)
    {
        if (level_is_solid(&game->level, c, row))
            return false;
    }
    return true;
}

static bool load_level(Game *game, int index)
{
    if (!level_load(&game->level, LEVEL_FILES[index]))
    {
        return false;
    }
    game->current_level = index;

    player_reset(&game->player, &game->level);

    game->enemy_count = game->level.enemy_count;
    for (int i = 0; i < game->enemy_count; ++i)
    {
        enemy_init(&game->enemies[i],
                   game->level.enemy_spawns[i].x,
                   game->level.enemy_spawns[i].y);
    }

    game->invuln_timer = 0.0f;
    game->message_timer = 0.0f;
    game->state = STATE_PLAYING;

    /* Initialise per-door state */
    for (int i = 0; i < game->level.door_count; ++i)
    {
        game->door_spawns[i] = game->level.door_spawn_counts[i];
        /* Stagger initial spawn times so doors don't all fire at once */
        game->door_timers[i] = DOOR_SPAWN_INTERVAL * (0.4f + SDL_rand(60) * 0.01f);
    }
    game->teleport_cooldown = 0.0f;

    game->player_on_elevator = -1;

    SDL_memset(game->bullets, 0, sizeof(game->bullets));
    SDL_memset(game->enemy_bullets, 0, sizeof(game->enemy_bullets));

    int win_w = game->level.width * TILE_SIZE;
    int win_h = game->level.height * TILE_SIZE + HUD_HEIGHT;
    SDL_SetWindowSize(game->window, win_w, win_h);

    return true;
}

static void restart_game(Game *game)
{
    game->lives = PLAYER_LIVES;
    game->score = 0;
    load_level(game, 0);
}

bool game_init(Game *game)
{
    SDL_zerop(game);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_CreateWindowAndRenderer("Chuck", 800, 552, 0,
                                     &game->window, &game->renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderVSync(game->renderer, 1);
    srand((unsigned int)time(NULL));
    SDL_srand(SDL_GetTicksNS());

    game->lives = PLAYER_LIVES;
    game->score = 0;
    if (!load_level(game, 0))
    {
        return false;
    }

    game->last_tick = SDL_GetTicksNS();
    return true;
}

void game_handle_event(Game *game, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
    {
        SDL_Keycode key = event->key.key;
        if (key == SDLK_SPACE)
        {
            game->input.jump = true;
        }
        if (key == SDLK_X || key == SDLK_LCTRL)
        {
            game->input.shoot = true;
        }
        if (key == SDLK_DOWN || key == SDLK_S)
        {
            game->input.use_door = true;
        }
        if (key == SDLK_R &&
            (game->state == STATE_GAME_OVER || game->state == STATE_WIN))
        {
            restart_game(game);
        }
    }
}

static void read_input(Game *game)
{
    const bool *ks = SDL_GetKeyboardState(NULL);
    game->input.left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
    game->input.right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
    game->input.up = ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W];
    game->input.down = ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S];
}

static void hit_player(Game *game)
{
    game->lives -= 1;
    game->invuln_timer = INVULN_TIME;
    if (game->lives <= 0)
    {
        game->state = STATE_GAME_OVER;
    }
    else
    {
        player_reset(&game->player, &game->level);
    }
}

static void advance_level(Game *game)
{
    if (game->current_level + 1 < LEVEL_COUNT)
    {
        load_level(game, game->current_level + 1);
    }
    else
    {
        game->state = STATE_WIN;
    }
}

void game_update(Game *game, float dt)
{
    read_input(game);

    if (game->state == STATE_LEVEL_CLEARED)
    {
        game->message_timer -= dt;
        if (game->message_timer <= 0.0f)
        {
            advance_level(game);
        }
        game->input.jump = false;
        return;
    }

    if (game->state != STATE_PLAYING)
    {
        game->input.jump = false;
        return;
    }

    if (game->invuln_timer > 0.0f)
    {
        game->invuln_timer -= dt;
    }

    /* --- Elevator: pre-carry (upward), player physics, update platforms, snap --- */
    {
        int prev_elev = game->player_on_elevator;
        /* If riding an upward-moving elevator, nudge the player up before physics
         * so gravity doesn't immediately drop them off. */
        if (prev_elev >= 0 && prev_elev < game->level.elevator_count)
        {
            const Elevator *el = &game->level.elevators[prev_elev];
            if (el->vy < 0.0f)
            {
                game->player.y += el->vy * dt;
                game->player.vy = 0.0f;
            }
        }
    }

    /* Crush detection: if anything pushed the player into a solid tile overhead, they die. */
    {
        int col_left = (int)floorf(game->player.x / (float)TILE_SIZE);
        int col_right = (int)floorf((game->player.x + PLAYER_W - 1.0f) / (float)TILE_SIZE);
        int row_top = (int)floorf(game->player.y / (float)TILE_SIZE);
        for (int c = col_left; c <= col_right; ++c)
        {
            if (level_is_solid(&game->level, c, row_top))
            {
                hit_player(game);
                break;
            }
        }
    }

    player_update(&game->player, &game->level, &game->input, dt);
    game->input.jump = false;

    level_update_elevators(&game->level, dt);
    game->player_on_elevator = -1;
    for (int i = 0; i < game->level.elevator_count; ++i)
    {
        const Elevator *el = &game->level.elevators[i];
        float plat_x = el->col * (float)TILE_SIZE;
        float player_cx = game->player.x + PLAYER_W * 0.5f;
        float player_feet = game->player.y + PLAYER_H;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            game->player.vy >= 0.0f &&
            player_feet >= el->y - 2.0f &&
            player_feet <= el->y + ELEVATOR_PLAT_H + 8.0f)
        {
            game->player.y = el->y - PLAYER_H;
            game->player.vy = 0.0f;
            game->player.on_ground = true;
            game->player_on_elevator = i;
        }
    }

    if (game->teleport_cooldown > 0.0f)
        game->teleport_cooldown -= dt;

    if (game->input.use_door && game->player.on_ground && game->teleport_cooldown <= 0.0f)
    {
        int center_col = (int)floorf((game->player.x + PLAYER_W * 0.5f) / TILE_SIZE);
        int center_row = (int)floorf((game->player.y + PLAYER_H * 0.5f) / TILE_SIZE);
        const Level *lvl = &game->level;
        for (int d = 0; d < lvl->door_count; ++d)
        {
            if (lvl->doors[d].col == center_col && lvl->doors[d].row == center_row)
            {
                int pair = d ^ 1; /* 0<->1, 2<->3, ... */
                if (pair < lvl->door_count)
                {
                    const Door *dest = &lvl->doors[pair];
                    game->player.x = dest->col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
                    game->player.y = (dest->row + 1) * TILE_SIZE - PLAYER_H;
                    game->player.vx = 0.0f;
                    game->player.vy = 0.0f;
                    game->teleport_cooldown = TELEPORT_COOLDOWN;
                }
                break;
            }
        }
    }
    game->input.use_door = false;

    /* --- Door enemy spawning --- */
    for (int d = 0; d < game->level.door_count; ++d)
    {
        if (game->door_spawns[d] <= 0)
            continue;
        game->door_timers[d] -= dt;
        if (game->door_timers[d] > 0.0f)
            continue;

        /* Don't spawn on top of the player */
        const Door *door = &game->level.doors[d];
        float dx = door->col * (float)TILE_SIZE;
        float dy = door->row * (float)TILE_SIZE;
        if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H,
                          dx, dy, (float)TILE_SIZE, (float)TILE_SIZE))
        {
            game->door_timers[d] = 0.5f; /* retry soon */
            continue;
        }

        int slot = find_enemy_slot(game);
        if (slot >= 0)
        {
            float sx = door->col * TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
            float sy = (door->row + 1) * TILE_SIZE - ENEMY_H;
            enemy_init(&game->enemies[slot], sx, sy);
            game->door_spawns[d]--;
            game->door_timers[d] = DOOR_SPAWN_INTERVAL * (0.8f + SDL_rand(40) * 0.01f);
        }
        else
        {
            game->door_timers[d] = 1.0f; /* no free slot, retry later */
        }
    }

    /* Shoot */
    if (game->input.shoot && game->player.bullets > 0)
    {
        for (int i = 0; i < MAX_BULLETS; ++i)
        {
            if (!game->bullets[i].active)
            {
                Bullet *b = &game->bullets[i];
                b->active = true;
                b->y = game->player.y + PLAYER_H * 0.35f;
                if (game->player.facing > 0)
                {
                    b->x = game->player.x + PLAYER_W;
                    b->vx = BULLET_SPEED;
                }
                else
                {
                    b->x = game->player.x - BULLET_W;
                    b->vx = -BULLET_SPEED;
                }
                game->player.bullets--;
                break;
            }
        }
    }
    game->input.shoot = false;

    for (int i = 0; i < game->enemy_count; ++i)
    {
        if (!game->enemies[i].dead)
            enemy_update(&game->enemies[i], &game->level, dt);
    }

    /* Collect items. */
    for (int i = 0; i < game->level.item_count; ++i)
    {
        Item *it = &game->level.items[i];
        if (it->collected)
        {
            continue;
        }
        if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H,
                          it->x - 8.0f, it->y - 8.0f, 16.0f, 16.0f))
        {
            it->collected = true;
            if (it->type == ITEM_CARD)
            {
                game->score += 100;
                if (i == game->level.active_card_index)
                {
                    game->level.items_remaining = 0;
                }
            }
            else /* ITEM_GUN */
            {
                game->player.bullets = MAX_AMMO;
            }
        }
    }

    /* Update bullets. */
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        Bullet *b = &game->bullets[i];
        if (!b->active)
            continue;

        b->x += b->vx * dt;

        /* Off-screen */
        if (b->x + BULLET_W < 0.0f || b->x > game->level.width * (float)TILE_SIZE)
        {
            b->active = false;
            continue;
        }

        /* Wall collision */
        int col = (int)floorf((b->x + (b->vx > 0 ? BULLET_W - 1 : 0)) / TILE_SIZE);
        int row = (int)floorf((b->y + BULLET_H * 0.5f) / TILE_SIZE);
        if (level_is_solid(&game->level, col, row))
        {
            b->active = false;
            continue;
        }

        /* Enemy collision */
        for (int j = 0; j < game->enemy_count; ++j)
        {
            Enemy *e = &game->enemies[j];
            if (e->dead)
                continue;
            if (boxes_overlap(b->x, b->y, BULLET_W, BULLET_H,
                              e->x, e->y, ENEMY_W, ENEMY_H))
            {
                b->active = false;
                e->hp--;
                if (e->hp <= 0)
                {
                    e->dead = true;
                    game->score += 150;
                }
                break;
            }
        }
    }

    /* Enemy shooting: tick cooldowns, fire when LOS to player is clear. */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *e = &game->enemies[i];
        if (e->dead || e->climbing)
            continue;

        /* Tick aim timer; fire bullet when it expires. */
        if (e->aim_timer > 0.0f)
        {
            e->aim_timer -= dt;
            if (e->aim_timer <= 0.0f)
            {
                e->aim_timer = 0.0f;
                for (int j = 0; j < MAX_ENEMY_BULLETS; ++j)
                {
                    Bullet *b = &game->enemy_bullets[j];
                    if (b->active)
                        continue;
                    float px = game->player.x + PLAYER_W * 0.5f;
                    b->vx = (px > e->x + ENEMY_W * 0.5f) ? ENEMY_BULLET_SPEED : -ENEMY_BULLET_SPEED;
                    b->x = (b->vx > 0) ? e->x + ENEMY_W : e->x - BULLET_W;
                    b->y = e->y + ENEMY_H * 0.35f;
                    b->active = true;
                    break;
                }
                e->shoot_cooldown = ENEMY_SHOOT_COOLDOWN * (0.7f + SDL_rand(60) * 0.01f);
            }
            continue;
        }

        e->shoot_cooldown -= dt;
        if (e->shoot_cooldown > 0.0f || !enemy_has_los(game, e))
            continue;

        /* Start aiming – enemy freezes for ENEMY_AIM_TIME before firing. */
        e->aim_timer = ENEMY_AIM_TIME;
    }

    /* Update enemy bullets: move, wall check, player hit. */
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        Bullet *b = &game->enemy_bullets[i];
        if (!b->active)
            continue;

        b->x += b->vx * dt;

        if (b->x + BULLET_W < 0.0f || b->x > game->level.width * (float)TILE_SIZE)
        {
            b->active = false;
            continue;
        }

        int col = (int)floorf((b->x + (b->vx > 0 ? BULLET_W - 1 : 0)) / TILE_SIZE);
        int row = (int)floorf((b->y + BULLET_H * 0.5f) / TILE_SIZE);
        if (level_is_solid(&game->level, col, row))
        {
            b->active = false;
            continue;
        }

        if (game->invuln_timer <= 0.0f &&
            boxes_overlap(b->x, b->y, BULLET_W, BULLET_H,
                          game->player.x, game->player.y, PLAYER_W, PLAYER_H))
        {
            b->active = false;
            hit_player(game);
        }
    }

    /* Enemy contact. */
    if (game->invuln_timer <= 0.0f)
    {
        for (int i = 0; i < game->enemy_count; ++i)
        {
            Enemy *e = &game->enemies[i];
            if (e->dead)
                continue;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H,
                              e->x, e->y, ENEMY_W, ENEMY_H))
            {
                hit_player(game);
                break;
            }
        }
    }

    /* Reach the exit once every item is collected. */
    if (game->state == STATE_PLAYING && game->level.has_exit &&
        game->level.items_remaining == 0)
    {
        float ex = game->level.exit_col * (float)TILE_SIZE;
        float ey = game->level.exit_row * (float)TILE_SIZE;
        if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H,
                          ex, ey, TILE_SIZE, TILE_SIZE))
        {
            game->state = STATE_LEVEL_CLEARED;
            game->message_timer = 1.2f;
        }
    }
}

/* ---------------- Rendering ---------------- */

static void fill_rect(SDL_Renderer *r, float x, float y, float w, float h)
{
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      Uint8 cr, Uint8 cg, Uint8 cb, const char *text)
{
    SDL_SetRenderScale(r, scale, scale);
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDebugText(r, x / scale, y / scale, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static void draw_text_centered(Game *game, float center_y, float scale,
                               Uint8 cr, Uint8 cg, Uint8 cb, const char *text)
{
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(game->window, &win_w, &win_h);
    float text_w = (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
    float text_h = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
    float x = (win_w - text_w) * 0.5f;
    float y = center_y - text_h * 0.5f;
    draw_text(game->renderer, x, y, scale, cr, cg, cb, text);
}

static void render_world(Game *game)
{
    SDL_Renderer *r = game->renderer;
    const Level *lvl = &game->level;
    const float oy = HUD_HEIGHT;

    /* Tiles */
    for (int row = 0; row < lvl->height; ++row)
    {
        for (int col = 0; col < lvl->width; ++col)
        {
            float x = col * (float)TILE_SIZE;
            float y = row * (float)TILE_SIZE + oy;
            TileType t = lvl->tiles[row][col];
            if (t == TILE_WALL)
            {
                SDL_SetRenderDrawColor(r, 110, 70, 40, 255);
                fill_rect(r, x, y, TILE_SIZE, TILE_SIZE);
                SDL_SetRenderDrawColor(r, 140, 95, 60, 255);
                fill_rect(r, x, y, TILE_SIZE, 4);
            }
            else if (t == TILE_LADDER)
            {
                SDL_SetRenderDrawColor(r, 210, 200, 90, 255);
                SDL_RenderLine(r, x + 6, y, x + 6, y + TILE_SIZE);
                SDL_RenderLine(r, x + TILE_SIZE - 6, y, x + TILE_SIZE - 6, y + TILE_SIZE);
                for (int rung = 4; rung < TILE_SIZE; rung += 10)
                {
                    SDL_RenderLine(r, x + 6, y + rung, x + TILE_SIZE - 6, y + rung);
                }
            }
            else if (t == TILE_ELEVATOR_SHAFT)
            {
                /* Draw two vertical guide rails */
                SDL_SetRenderDrawColor(r, 100, 105, 115, 255);
                SDL_RenderLine(r, x + 9, y, x + 9, y + TILE_SIZE);
                SDL_RenderLine(r, x + TILE_SIZE - 9, y, x + TILE_SIZE - 9, y + TILE_SIZE);
            }
        }
    }

    /* Elevator moving platforms */
    for (int i = 0; i < lvl->elevator_count; ++i)
    {
        const Elevator *el = &lvl->elevators[i];
        float px = el->col * (float)TILE_SIZE;
        float py = el->y + oy;
        /* Platform body */
        SDL_SetRenderDrawColor(r, 160, 165, 175, 255);
        fill_rect(r, px, py, TILE_SIZE, ELEVATOR_PLAT_H);
        /* Bright top edge */
        SDL_SetRenderDrawColor(r, 220, 225, 235, 255);
        fill_rect(r, px, py, TILE_SIZE, 2);
    }

    /* Doors (all identical — part of the puzzle) */
    for (int d = 0; d < lvl->door_count; ++d)
    {
        float x = lvl->doors[d].col * (float)TILE_SIZE;
        float y = lvl->doors[d].row * (float)TILE_SIZE + oy;
        /* Frame */
        SDL_SetRenderDrawColor(r, 75, 45, 15, 255);
        fill_rect(r, x, y, TILE_SIZE, TILE_SIZE);
        /* Panel */
        SDL_SetRenderDrawColor(r, 115, 65, 25, 255);
        fill_rect(r, x + 3, y + 3, TILE_SIZE - 6, TILE_SIZE - 3);
        /* Knob */
        SDL_SetRenderDrawColor(r, 195, 165, 55, 255);
        fill_rect(r, x + TILE_SIZE - 10, y + TILE_SIZE / 2 - 2, 5, 5);
    }

    /* Exit */
    if (lvl->has_exit)
    {
        float x = lvl->exit_col * (float)TILE_SIZE;
        float y = lvl->exit_row * (float)TILE_SIZE + oy;
        if (lvl->items_remaining == 0)
        {
            SDL_SetRenderDrawColor(r, 60, 220, 90, 255);
        }
        else
        {
            SDL_SetRenderDrawColor(r, 80, 80, 90, 255);
        }
        fill_rect(r, x + 4, y + 2, TILE_SIZE - 8, TILE_SIZE - 2);
        SDL_SetRenderDrawColor(r, 30, 30, 35, 255);
        fill_rect(r, x + TILE_SIZE - 12, y + TILE_SIZE / 2 - 2, 4, 4);
    }

    /* Items */
    for (int i = 0; i < lvl->item_count; ++i)
    {
        const Item *it = &lvl->items[i];
        if (it->collected)
        {
            continue;
        }
        float x = it->x - 7.0f;
        float y = it->y - 9.0f + oy;
        if (it->type == ITEM_CARD)
        {
            /* Card body */
            SDL_SetRenderDrawColor(r, 30, 190, 200, 255);
            fill_rect(r, x, y, 14, 18);
            /* Card border */
            SDL_SetRenderDrawColor(r, 200, 255, 255, 255);
            fill_rect(r, x, y, 14, 2);
            fill_rect(r, x, y + 16, 14, 2);
            fill_rect(r, x, y, 2, 18);
            fill_rect(r, x + 12, y, 2, 18);
            /* Small dot symbol */
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            fill_rect(r, x + 5, y + 7, 4, 4);
        }
        else /* ITEM_GUN */
        {
            /* barrel */
            SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
            fill_rect(r, x + 4, y + 2, 11, 4);
            /* grip */
            fill_rect(r, x + 4, y + 5, 6, 7);
            /* highlight on barrel */
            SDL_SetRenderDrawColor(r, 140, 140, 140, 255);
            fill_rect(r, x + 4, y + 2, 11, 2);
        }
    }

    /* Enemies */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        const Enemy *e = &game->enemies[i];
        if (e->dead)
            continue;
        float x = e->x;
        float y = e->y + oy;
        /* Body color reflects remaining HP */
        if (e->hp >= ENEMY_HP)
            SDL_SetRenderDrawColor(r, 210, 50, 50, 255);
        else if (e->hp == 2)
            SDL_SetRenderDrawColor(r, 210, 130, 50, 255);
        else
            SDL_SetRenderDrawColor(r, 200, 200, 50, 255);
        fill_rect(r, x, y, ENEMY_W, ENEMY_H);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        fill_rect(r, x + 4, y + 6, 4, 4);
        fill_rect(r, x + ENEMY_W - 8, y + 6, 4, 4);
        /* HP dots above the enemy head */
        for (int h = 0; h < ENEMY_HP; ++h)
        {
            if (h < e->hp)
                SDL_SetRenderDrawColor(r, 60, 220, 60, 255);
            else
                SDL_SetRenderDrawColor(r, 70, 30, 30, 255);
            fill_rect(r, x + 2 + h * 7, y - 6, 5, 4);
        }
    }

    /* Bullets (player = yellow, enemy = orange-red) */
    SDL_SetRenderDrawColor(r, 255, 220, 0, 255);
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        const Bullet *b = &game->bullets[i];
        if (!b->active)
            continue;
        fill_rect(r, b->x, b->y + oy, BULLET_W, BULLET_H);
    }
    SDL_SetRenderDrawColor(r, 255, 90, 30, 255);
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        const Bullet *b = &game->enemy_bullets[i];
        if (!b->active)
            continue;
        fill_rect(r, b->x, b->y + oy, BULLET_W, BULLET_H);
    }

    /* Player (blink while invulnerable) */
    bool blink = game->invuln_timer > 0.0f &&
                 ((int)(game->invuln_timer * 12.0f) % 2 == 0);
    if (!blink)
    {
        float x = game->player.x;
        float y = game->player.y + oy;
        SDL_SetRenderDrawColor(r, 60, 120, 230, 255);
        fill_rect(r, x, y, PLAYER_W, PLAYER_H);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        float eye = (game->player.facing > 0) ? x + PLAYER_W - 8 : x + 4;
        fill_rect(r, eye, y + 6, 4, 4);
    }
}

static void render_hud(Game *game)
{
    SDL_Renderer *r = game->renderer;

    SDL_SetRenderDrawColor(r, 20, 20, 28, 255);
    fill_rect(r, 0, 0, (float)(game->level.width * TILE_SIZE), HUD_HEIGHT);

    char buf[128];
    SDL_snprintf(buf, sizeof(buf), "LIVES %d  AMMO %d  KEY: %s  LEVEL %d  SCORE %d",
                 game->lives,
                 game->player.bullets,
                 game->level.items_remaining == 0 ? "YES" : "NO",
                 game->current_level + 1,
                 game->score);
    draw_text(r, 8, 12, 2.0f, 240, 240, 240, buf);
}

void game_render(Game *game)
{
    SDL_Renderer *r = game->renderer;

    SDL_SetRenderDrawColor(r, 18, 18, 24, 255);
    SDL_RenderClear(r);

    render_world(game);
    render_hud(game);

    if (game->state == STATE_LEVEL_CLEARED)
    {
        draw_text_centered(game, 240.0f, 4.0f, 80, 230, 120, "LEVEL CLEARED");
    }
    else if (game->state == STATE_GAME_OVER)
    {
        draw_text_centered(game, 220.0f, 4.0f, 230, 70, 70, "GAME OVER");
        draw_text_centered(game, 280.0f, 2.0f, 230, 230, 230, "PRESS R TO RESTART");
    }
    else if (game->state == STATE_WIN)
    {
        draw_text_centered(game, 220.0f, 4.0f, 90, 230, 120, "YOU WIN!");
        draw_text_centered(game, 280.0f, 2.0f, 230, 230, 230, "PRESS R TO RESTART");
    }

    SDL_RenderPresent(r);
}

void game_shutdown(Game *game)
{
    if (game->renderer)
    {
        SDL_DestroyRenderer(game->renderer);
    }
    if (game->window)
    {
        SDL_DestroyWindow(game->window);
    }
    SDL_Quit();
}
