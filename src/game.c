#include "game.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

/* Helper: get the current logical render size if enabled, otherwise fall
 * back to actual window size. This ensures camera and clipping use the same
 * coordinate system as the renderer's logical presentation. */
static void get_view_size(Game *game, int *out_w, int *out_h)
{
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode;
    if (SDL_GetRenderLogicalPresentation(game->renderer, &lw, &lh, &mode) && lw > 0 && lh > 0)
    {
        *out_w = lw;
        *out_h = lh;
        return;
    }
    SDL_GetWindowSize(game->window, out_w, out_h);
}

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

static int find_grenade_slot(Game *game)
{
    for (int i = 0; i < game->grenade_count; ++i)
        if (!game->grenades[i].active)
            return i;
    if (game->grenade_count < MAX_GRENADES)
        return game->grenade_count++;
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
    float ph = p->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    float py = p->y + ph * 0.5f;

    /* If the enemy is currently talking to another enemy, they will ignore
     * the player unless the player comes very close. */
    if (e->talking)
    {
        float dx = px - ex;
        float dy = py - ey;
        if (fabsf(dx) > (float)ENEMY_TALK_NOTICE_RADIUS || fabsf(dy) > (float)ENEMY_TALK_NOTICE_RADIUS)
            return false;
    }

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

    /* Start with no active enemies/mines: we'll reveal the level first and
     * spawn entities after the tile-reveal animation completes. */
    game->enemy_count = 0;
    game->mine_count = 0;

    game->invuln_timer = 0.0f;
    game->message_timer = 0.0f;
    game->state = STATE_LEVEL_START;
    level_reveal_init(&game->level);

    /* Initialise grenades state */
    game->grenade_count = 0;
    for (int i = 0; i < MAX_GRENADES; ++i)
    {
        game->grenades[i].active = false;
        game->grenades[i].timer = 0.0f;
        game->grenades[i].grounded = false;
        game->grenades[i].vx = 0.0f;
        game->grenades[i].vy = 0.0f;
    }

    /* Initialise per-door state */
    for (int i = 0; i < game->level.door_count; ++i)
    {
        game->door_spawns[i] = game->level.door_spawn_counts[i];
        /* Stagger initial spawn times so doors don't all fire at once */
        game->door_timers[i] = DOOR_SPAWN_INTERVAL * (0.4f + SDL_rand(60) * 0.01f);
    }
    game->teleport_cooldown = 0.0f;

    game->player_on_elevator = -1;
    game->player_on_moving_platform = -1;

    SDL_memset(game->bullets, 0, sizeof(game->bullets));
    SDL_memset(game->enemy_bullets, 0, sizeof(game->enemy_bullets));

    /* Keep the existing window size; initialise camera to centre on player. */
    int win_w = 0, win_h = 0;
    get_view_size(game, &win_w, &win_h);
    float max_cam = game->level.width * (float)TILE_SIZE - (float)win_w;
    if (max_cam < 0.0f)
        max_cam = 0.0f;
    float desired = game->player.x + PLAYER_W * 0.5f - (float)win_w * 0.5f;
    if (desired < 0.0f)
        desired = 0.0f;
    if (desired > max_cam)
        desired = max_cam;
    game->cam_x = desired;

    return true;
}

/* Spawn enemies/mines/etc after the level reveal animation completes. */
static void spawn_level_entities_after_reveal(Game *game)
{
    /* Initialise enemies from level spawn points */
    game->enemy_count = game->level.enemy_count;
    for (int i = 0; i < game->enemy_count; ++i)
    {
        enemy_init(&game->enemies[i],
                   game->level.enemy_spawns[i].x,
                   game->level.enemy_spawns[i].y);
    }

    /* Initialise mines from level spawns */
    game->mine_count = game->level.mine_count;
    for (int i = 0; i < game->mine_count; ++i)
    {
        game->mines[i].x = game->level.mine_spawns[i].x;
        game->mines[i].y = game->level.mine_spawns[i].y;
        game->mines[i].active = true;
        game->mines[i].triggered = false;
        game->mines[i].timer = 0.0f;
    }
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

    /* Use a fixed logical presentation so the game's coordinate system stays
     * consistent when the window is resized or when toggling fullscreen.
     * This makes the game look identical but scaled when entering fullscreen. */
    SDL_SetRenderLogicalPresentation(game->renderer, 800, 552, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_SetRenderVSync(game->renderer, 1);
    srand((unsigned int)time(NULL));
    SDL_srand(SDL_GetTicksNS());

    game->lives = PLAYER_LIVES;
    game->score = 0;
    if (!load_level(game, 0))
    {
        return false;
    }

    /* Initialise particle system */
    particle_system_init(&game->particles);

    /* Load sprite sheets (falls back to procedural rendering if files missing) */
    sprites_load(&game->sprites, game->renderer);

    game->last_tick = SDL_GetTicksNS();
    return true;
}

void game_handle_event(Game *game, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
    {
        SDL_Keycode key = event->key.key;
        /* Toggle fullscreen on 'f' or Alt+Enter. Use scancode/modifier via
         * keyboard state to avoid dependency on deprecated keycode macros. */
        SDL_Scancode sc = event->key.scancode;
        const bool *kstate = SDL_GetKeyboardState(NULL);
        if (sc == SDL_SCANCODE_F ||
            (sc == SDL_SCANCODE_RETURN && (kstate[SDL_SCANCODE_LALT] || kstate[SDL_SCANCODE_RALT])))
        {
            if (!game->fullscreen)
            {
                SDL_SetWindowFullscreen(game->window, true);
                game->fullscreen = true;
            }
            else
            {
                SDL_SetWindowFullscreen(game->window, false);
                game->fullscreen = false;
            }
            return;
        }
        /* Shoot on Space only */
        if (key == SDLK_SPACE)
        {
            game->input.shoot = true;
        }
        /* Jump on Up arrow, but avoid interfering with ladders: only trigger
         * jump edge when player is on the ground and not overlapping a ladder.
         */
        if (key == SDLK_UP || event->key.scancode == SDL_SCANCODE_W)
        {
            /* Determine whether player box overlaps a ladder near center/feet */
            int col = (int)floorf((game->player.x + PLAYER_W * 0.5f) / TILE_SIZE);
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            int row_center = (int)floorf((game->player.y + ph * 0.5f) / TILE_SIZE);
            int row_feet = (int)floorf((game->player.y + ph - 1.0f) / TILE_SIZE);
            bool over_ladder = level_is_ladder(&game->level, col, row_center) ||
                               level_is_ladder(&game->level, col, row_feet);
            if (!over_ladder && !game->player.on_ladder && game->player.on_ground)
            {
                game->input.jump = true;
            }
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
    /* Start death animation using the particle system. Use a player-local
     * dying flag so other game state remains unchanged. */
    if (game->player.dying)
        return;

    float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    float cx = game->player.x + PLAYER_W * 0.5f;
    float cy = game->player.y + ph * 0.5f;
    particle_system_emit(&game->particles, cx, cy, 32, game->player.facing);
    /* mark player as dying; actual life loss / respawn happens after timer */
    game->player.dying = true;
    game->player.death_timer = 0.75f;
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

static void finish_player_death(Game *game)
{
    /* Apply the actual hit effects after the death animation */
    game->player.dying = false;
    game->player.death_timer = 0.0f;
    particle_system_clear(&game->particles);

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

void game_update(Game *game, float dt)
{
    read_input(game);

    /* Level start reveal animation: show tiles progressively, then spawn entities. */
    if (game->state == STATE_LEVEL_START)
    {
        /* Advance reveal; if not finished yet, skip the rest of update. */
        level_reveal_step(&game->level, (float)dt);
        if (!game->level.reveal_done)
        {
            game->input.jump = false;
            return;
        }
        /* Reveal finished: either start key-card intro animation or spawn entities. */
        if (game->level.card_count > 0 && game->level.active_card_index >= 0)
        {
            /* Determine active card position among card items. */
            int active_pos = 0;
            for (int i = 0; i < game->level.item_count; ++i)
            {
                if (game->level.items[i].type != ITEM_CARD)
                    continue;
                if (i == game->level.active_card_index)
                    break;
                active_pos++;
            }
            game->card_anim_count = game->level.card_count;
            game->card_anim_current = 0;
            game->card_anim_step = 0;
            game->card_anim_interval = 0.075f; /* 75ms per step (faster) */
            game->card_anim_timer = 0.0f;
            /* Run a few cycles before landing on the chosen card */
            int cycles = 3;
            game->card_anim_total_steps = cycles * game->card_anim_count + active_pos;
            game->state = STATE_SHOW_KEYCARD;
        }
        else
        {
            spawn_level_entities_after_reveal(game);
            game->state = STATE_PLAYING;
        }
    }

    /* Key-card intro animation: cycle highlight until target reached, then begin play */
    if (game->state == STATE_SHOW_KEYCARD)
    {
        game->card_anim_timer += dt;
        if (game->card_anim_timer >= game->card_anim_interval)
        {
            game->card_anim_timer -= game->card_anim_interval;
            game->card_anim_step++;
            if (game->card_anim_count > 0)
                game->card_anim_current = (game->card_anim_current + 1) % game->card_anim_count;
            if (game->card_anim_step >= game->card_anim_total_steps)
            {
                /* Animation finished: spawn entities and start playing. */
                spawn_level_entities_after_reveal(game);
                game->state = STATE_PLAYING;
            }
        }
        game->input.jump = false;
        return;
    }

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

    /* If player is in dying animation, update particles and wait */
    if (game->player.dying)
    {
        particle_system_update(&game->particles, dt);
        game->player.death_timer -= dt;
        if (game->player.death_timer <= 0.0f)
            finish_player_death(game);
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
    level_update_falling_platforms(&game->level, dt);
    level_update_moving_platforms(&game->level, dt);
    game->player_on_elevator = -1;
    for (int i = 0; i < game->level.elevator_count; ++i)
    {
        const Elevator *el = &game->level.elevators[i];
        float plat_x = el->col * (float)TILE_SIZE;
        float player_cx = game->player.x + PLAYER_W * 0.5f;
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_feet = game->player.y + ph;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            game->player.vy >= 0.0f &&
            player_feet >= el->y - 2.0f &&
            player_feet <= el->y + ELEVATOR_PLAT_H + 8.0f)
        {
            game->player.y = el->y - ph;
            game->player.vy = 0.0f;
            game->player.on_ground = true;
            game->player_on_elevator = i;
        }
    }

    /* Moving platforms: detect if player stands on one and snap/carry them. */
    game->player_on_moving_platform = -1;
    for (int i = 0; i < game->level.moving_platform_count; ++i)
    {
        const MovingPlatform *mp = &game->level.moving_platforms[i];
        float plat_x = mp->x;
        float player_cx = game->player.x + PLAYER_W * 0.5f;
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        float player_feet = game->player.y + ph;
        float plat_top = mp->row * (float)TILE_SIZE;
        if (player_cx > plat_x && player_cx < plat_x + TILE_SIZE &&
            game->player.vy >= 0.0f &&
            player_feet >= plat_top - 2.0f &&
            player_feet <= plat_top + MOVING_PLATFORM_H + 8.0f)
        {
            game->player.y = plat_top - ph;
            game->player.vy = 0.0f;
            game->player.on_ground = true;
            game->player_on_moving_platform = i;
            break;
        }
    }

    /* If riding a moving platform, carry the player horizontally. */
    if (game->player_on_moving_platform >= 0 && game->player_on_moving_platform < game->level.moving_platform_count)
    {
        const MovingPlatform *mp = &game->level.moving_platforms[game->player_on_moving_platform];
        game->player.x += mp->vx * dt;
    }

    if (game->teleport_cooldown > 0.0f)
        game->teleport_cooldown -= dt;

    /* Mines: stepping triggers a short countdown, then explosion */
    for (int i = 0; i < game->mine_count; ++i)
    {
        /* Mine stored in Game as an inline struct */
        Mine *m = &game->mines[i];
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        if (!m->active)
            continue;
        if (!m->triggered)
        {
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              m->x, m->y, (float)MINE_W, (float)MINE_H))
            {
                m->triggered = true;
                m->timer = MINE_TRIGGER_DELAY;
            }
        }
        else
        {
            m->timer -= dt;
            if (m->timer <= 0.0f)
            {
                m->active = false;
                float cx = m->x + MINE_W * 0.5f;
                float cy = m->y + MINE_H * 0.5f;
                particle_system_explosion(&game->particles, cx, cy, 48);
                /* Damage player if within radius and not invulnerable */
                float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
                float px = game->player.x + PLAYER_W * 0.5f;
                float py = game->player.y + ph * 0.5f;
                float dx = px - cx;
                float dy = py - cy;
                float dist2 = dx * dx + dy * dy;
                if (dist2 <= MINE_RADIUS * MINE_RADIUS && game->invuln_timer <= 0.0f)
                {
                    hit_player(game);
                }
            }
        }

        /* Grenades: update thrown grenades (physics + fuse) */
        for (int i = 0; i < game->grenade_count; ++i)
        {
            Grenade *g = &game->grenades[i];
            if (!g->active)
                continue;

            /* Apply gravity to grenade velocity, then integrate with collisions
             * so grenades arc and come to rest on floors. */
            g->vy += GRAVITY * dt;
            if (g->vy > MAX_FALL_SPEED)
                g->vy = MAX_FALL_SPEED;

            bool on_ground = false;
            level_move(&game->level, &g->x, &g->y, &g->vx, &g->vy,
                       GRENADE_W, GRENADE_H, dt, false, &on_ground, false);
            if (on_ground)
            {
                g->grounded = true;
                g->vx = 0.0f;
                g->vy = 0.0f;
            }

            g->timer -= dt;
            if (g->timer <= 0.0f)
            {
                /* Explode */
                g->active = false;
                float cx = g->x + GRENADE_W * 0.5f;
                float cy = g->y + GRENADE_H * 0.5f;
                particle_system_explosion(&game->particles, cx, cy, 64);
                /* Kill enemies in radius */
                for (int j = 0; j < game->enemy_count; ++j)
                {
                    Enemy *e = &game->enemies[j];
                    if (e->dead)
                        continue;
                    float ex = e->x + ENEMY_W * 0.5f;
                    float ey = e->y + ENEMY_H * 0.5f;
                    float dx = ex - cx;
                    float dy = ey - cy;
                    float dist2 = dx * dx + dy * dy;
                    if (dist2 <= GRENADE_RADIUS * GRENADE_RADIUS)
                    {
                        /* instant kill */
                        e->hp = 0;
                        float px = e->x + ENEMY_W * 0.5f;
                        float py = e->y + ENEMY_H * 0.5f;
                        particle_system_emit(&game->particles, px, py, 24, e->dir);
                        e->dead = true;
                        game->score += 150;
                    }
                }
                /* Damage player if within radius */
                float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
                float px = game->player.x + PLAYER_W * 0.5f;
                float py = game->player.y + ph * 0.5f;
                float dx = px - cx;
                float dy = py - cy;
                float dist2 = dx * dx + dy * dy;
                if (dist2 <= GRENADE_RADIUS * GRENADE_RADIUS && game->invuln_timer <= 0.0f)
                {
                    hit_player(game);
                }
            }
        }
    }

    if (game->input.use_door && game->player.on_ground && game->teleport_cooldown <= 0.0f)
    {
        int center_col = (int)floorf((game->player.x + PLAYER_W * 0.5f) / TILE_SIZE);
        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
        int center_row = (int)floorf((game->player.y + ph * 0.5f) / TILE_SIZE);
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
                    game->player.y = (dest->row + 1) * TILE_SIZE - ph;
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
        {
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              dx, dy, (float)TILE_SIZE, (float)TILE_SIZE))
            {
                game->door_timers[d] = 0.5f; /* retry soon */
                continue;
            }
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

    /* Shoot or throw grenade */
    if (game->input.shoot)
    {
        /* If player has a grenade, throw it */
        if (game->player.grenades > 0)
        {
            int slot = find_grenade_slot(game);
            if (slot >= 0)
            {
                Grenade *g = &game->grenades[slot];
                g->active = true;
                g->timer = GRENADE_FUSE_TIME;
                g->grounded = false;
                /* Spawn near player's hand */
                {
                    float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
                    g->y = game->player.y + ph * 0.45f;
                }
                /* Spawn slightly ahead of player and choose velocities so the
                 * grenade follows a parabolic arc landing some distance ahead. */
                {
                    const float desired_range = 160.0f;              /* px: target horizontal distance */
                    const float base_v = GRENADE_THROW_SPEED * 0.9f; /* calmer forward throw */
                    float vx = (game->player.facing > 0) ? base_v : -base_v;
                    float hand_offset = (game->player.facing > 0) ? (PLAYER_W + 6.0f) : -(GRENADE_W + 6.0f);
                    g->x = game->player.x + hand_offset;
                    g->vx = vx;
                    /* Compute required initial upward speed magnitude to reach approx range
                     * R = |vx| * (2*vy / g)  =>  vy = R * g / (2 * |vx|) */
                    float vy_mag = (desired_range * GRAVITY) / (2.0f * fabsf(vx));
                    /* clamp vy_mag to reasonable bounds */
                    if (vy_mag < 30.0f)
                        vy_mag = 30.0f;
                    if (vy_mag > 220.0f)
                        vy_mag = 220.0f;
                    g->vy = -vy_mag;
                }
                game->player.grenades = 0;
            }
        }
        else if (game->player.bullets > 0)
        {
            for (int i = 0; i < MAX_BULLETS; ++i)
            {
                if (!game->bullets[i].active)
                {
                    Bullet *b = &game->bullets[i];
                    b->active = true;
                    {
                        float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
                        b->y = game->player.y + ph * 0.35f;
                        b->vy = 0.0f;
                    }
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
    }
    game->input.shoot = false;

    for (int i = 0; i < game->enemy_count; ++i)
    {
        if (!game->enemies[i].dead)
            enemy_update(&game->enemies[i], &game->level, dt);
    }

    /* Enemy conversations: pair-only logic. Two nearby, grounded enemies may
     * stop and chat with some chance. They are nudged apart slightly to stand
     * facing each other; a partner link prevents third parties joining. */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *a = &game->enemies[i];
        if (a->dead || a->talking || a->climbing || !a->on_ground || a->talk_partner != -1 || a->talk_cooldown > 0.0f)
            continue;
        for (int j = i + 1; j < game->enemy_count; ++j)
        {
            Enemy *b = &game->enemies[j];
            if (b->dead || b->talking || b->climbing || !b->on_ground || b->talk_partner != -1 || b->talk_cooldown > 0.0f)
                continue;

            /* Must be approximately same vertical level */
            if (fabsf(a->y - b->y) > (float)TILE_SIZE * 0.5f)
                continue;

            /* Horizontal proximity check */
            float axc = a->x + ENEMY_W * 0.5f;
            float bxc = b->x + ENEMY_W * 0.5f;
            float absdx = fabsf(axc - bxc);
            if (absdx > (float)ENEMY_W + 16.0f)
                continue;

            /* Chance to start a conversation */
            if (SDL_rand(100) >= ENEMY_TALK_CHANCE)
                continue;

            /* Try to nudge them apart horizontally if overlapping so they stand
             * in front of each other rather than stacked. Compute desired shifts
             * and verify target tiles are free. */
            Enemy *left = (a->x < b->x) ? a : b;
            Enemy *right = (a->x < b->x) ? b : a;
            float overlap = (left->x + ENEMY_W) - right->x;
            float gap = 6.0f;
            float shift = 0.0f;
            if (overlap > 0.0f)
                shift = (overlap + gap) * 0.5f;
            float new_left_x = left->x - shift;
            float new_right_x = right->x + shift;

            /* Check that new positions don't collide with solid tiles */
            int top_row = (int)floorf(left->y / TILE_SIZE);
            int bot_row = (int)floorf((left->y + ENEMY_H - 1.0f) / TILE_SIZE);
            bool can_place = true;
            for (int r = top_row; r <= bot_row && can_place; ++r)
            {
                int lc = (int)floorf(new_left_x / TILE_SIZE);
                int rc = (int)floorf((new_right_x + ENEMY_W - 1.0f) / TILE_SIZE);
                int lc_end = (int)floorf((new_left_x + ENEMY_W - 1.0f) / TILE_SIZE);
                int rc_start = (int)floorf(new_right_x / TILE_SIZE);
                /* left enemy area */
                for (int c = lc; c <= lc_end; ++c)
                {
                    if (level_is_solid(&game->level, c, r))
                    {
                        can_place = false;
                        break;
                    }
                }
                /* right enemy area */
                for (int c = rc_start; c <= rc; ++c)
                {
                    if (level_is_solid(&game->level, c, r))
                    {
                        can_place = false;
                        break;
                    }
                }
            }
            if (!can_place)
                continue; /* cannot safely separate, skip conversation */

            /* Start conversation pair */
            left->x = new_left_x;
            right->x = new_right_x;
            left->vx = right->vx = 0.0f;
            left->aim_timer = right->aim_timer = 0.0f;
            left->talking = right->talking = true;
            left->talk_timer = right->talk_timer = ENEMY_TALK_DURATION;
            left->talk_partner = (int)(right - game->enemies);
            right->talk_partner = (int)(left - game->enemies);
            left->dir = 1;   /* face right */
            right->dir = -1; /* face left */
            break;           /* only pair with one partner */
        }
    }

    /* Cleanup partner links: if either partner finished talking, clear both
     * sides and start a cooldown so they won't immediately talk again. */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *e = &game->enemies[i];
        if (e->talk_partner < 0)
            continue;
        int p = e->talk_partner;
        if (p < 0 || p >= game->enemy_count)
        {
            e->talk_partner = -1;
            continue;
        }
        Enemy *pe = &game->enemies[p];
        if (!e->talking || !pe->talking)
        {
            /* Clear partner links on both sides */
            if (pe->talk_partner == i)
            {
                pe->talk_partner = -1;
                pe->talking = false;
                pe->talk_timer = 0.0f;
                pe->talk_cooldown = ENEMY_TALK_COOLDOWN;
            }
            e->talk_partner = -1;
            e->talking = false;
            e->talk_timer = 0.0f;
            e->talk_cooldown = ENEMY_TALK_COOLDOWN;
        }
    }

    /* Collect items. */
    for (int i = 0; i < game->level.item_count; ++i)
    {
        Item *it = &game->level.items[i];
        if (it->collected)
        {
            continue;
        }
        {
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              it->x - 8.0f, it->y - 8.0f, 16.0f, 16.0f))
            {
                it->collected = true;
                /* Start respawn timer for ammo and grenade pickups; cards do not respawn. */
                if (it->type == ITEM_GUN || it->type == ITEM_GRENADE)
                {
                    it->respawn_timer = ITEM_RESPAWN_TIME;
                }
                if (it->type == ITEM_CARD)
                {
                    game->score += 100;
                    if (i == game->level.active_card_index)
                    {
                        game->level.items_remaining = 0;
                    }
                }
                else if (it->type == ITEM_GUN)
                {
                    game->player.bullets = MAX_AMMO;
                }
                else if (it->type == ITEM_GRENADE)
                {
                    game->player.grenades = 1; /* pickup gives one grenade */
                }
            }
        }
    }

    /* Respawn timers for ammo/grenade pickups: decrement timers and un-collect when expired. */
    for (int i = 0; i < game->level.item_count; ++i)
    {
        Item *it = &game->level.items[i];
        if (!it->collected)
            continue;
        if (it->type == ITEM_CARD)
            continue; /* cards never respawn */
        it->respawn_timer -= dt;
        if (it->respawn_timer <= 0.0f)
        {
            it->collected = false;
            it->respawn_timer = 0.0f;
        }
    }

    /* Update bullets. */
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        Bullet *b = &game->bullets[i];
        if (!b->active)
            continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

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
                    /* Emit blood particles at enemy death, then mark dead */
                    float cx = e->x + ENEMY_W * 0.5f;
                    float cy = e->y + ENEMY_H * 0.5f;
                    particle_system_emit(&game->particles, cx, cy, 24, e->dir);
                    e->dead = true;
                    game->score += 150;
                }
                break;
            }
        }
    }

    /* Enemy reaction: if the player is close and the enemy is moving away
     * while the player is not crawling, there's a small chance the enemy
     * will turn to face the player and start aiming to attack. */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        Enemy *e = &game->enemies[i];
        if (e->dead || e->climbing)
            continue;
        if (game->player.crawling)
            continue; /* don't retaliate at lowered (crawling) player */
        float px = game->player.x + PLAYER_W * 0.5f;
        float ex = e->x + ENEMY_W * 0.5f;
        float dx = px - ex;

        /* If the enemy is currently talking, only notice the player when
         * the player is very close (ENEMY_TALK_NOTICE_RADIUS). In that case
         * interrupt the conversation so the enemy can retaliate. */
        if (e->talking)
        {
            if (fabsf(dx) > (float)ENEMY_TALK_NOTICE_RADIUS)
                continue;
            /* Interrupt talk and start per-enemy cooldown so they won't
             * immediately start another conversation. */
            e->talking = false;
            e->talk_timer = 0.0f;
            if (e->talk_cooldown <= 0.0f)
                e->talk_cooldown = ENEMY_TALK_COOLDOWN;
        }
        /* Only consider when enemy is moving away from player */
        if (dx * (float)e->dir >= 0.0f)
            continue;
        if (fabsf(dx) > (float)ENEMY_RETALIATE_RADIUS)
            continue;
        /* Check unobstructed horizontal line of sight (ignore current facing)
         * and reasonable vertical alignment. This mirrors `enemy_has_los`
         * but does not require the enemy to already face the player. */
        {
            const Player *p = &game->player;
            float ey = e->y + ENEMY_H * 0.5f;
            float ph = p->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            float py = p->y + ph * 0.5f;
            if (fabsf(py - ey) > (float)TILE_SIZE * 1.2f)
                continue;
            int row = (int)floorf(ey / TILE_SIZE);
            float ex = e->x + ENEMY_W * 0.5f;
            float px2 = p->x + PLAYER_W * 0.5f;
            int col0 = (int)floorf(fminf(ex, px2) / TILE_SIZE);
            int col1 = (int)floorf(fmaxf(ex, px2) / TILE_SIZE);
            bool blocked = false;
            for (int c = col0; c <= col1; ++c)
            {
                if (level_is_solid(&game->level, c, row))
                {
                    blocked = true;
                    break;
                }
            }
            if (blocked)
                continue;
        }
        if (SDL_rand(100) < ENEMY_RETALIATE_CHANCE)
        {
            e->dir = (dx > 0.0f) ? 1 : -1;
            e->aim_target_x = px;
            if (game->player.crawling)
                e->aim_target_y = game->player.y + (float)PLAYER_CRAWL_H * 0.45f;
            else
                e->aim_target_y = game->player.y + (float)PLAYER_H * 0.15f;
            e->aim_timer = ENEMY_AIM_TIME;
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
                    /* Use the stored target snapshot captured when aiming started. */
                    /* Fire horizontally toward the captured target X while
                     * keeping vertical speed zero so the bullet travels straight.
                     * Place the bullet vertically at the captured aim Y (head
                     * or knee level) so crouching affects impact height. */
                    float px = e->aim_target_x;
                    float sx = e->x + ENEMY_W * 0.5f;
                    int dir = (fabsf(px - sx) < 0.001f) ? (e->dir >= 0 ? 1 : -1) : ((px > sx) ? 1 : -1);
                    b->vx = (float)dir * ENEMY_BULLET_SPEED;
                    b->vy = 0.0f;
                    b->x = (dir > 0) ? e->x + ENEMY_W : e->x - BULLET_W;
                    /* center bullet vertically on the captured target Y, but
                     * clamp it to remain within the enemy's body so bullets
                     * don't spawn above the shooter. */
                    /* Simple: spawn bullet from enemy's head; if the player is
                     * crawling, spawn lower (approx knee level of the enemy). */
                    if (game->player.crawling)
                    {
                        b->y = e->y + ENEMY_H * 0.6f - BULLET_H * 0.5f; /* knee-ish */
                    }
                    else
                    {
                        b->y = e->y + ENEMY_H * 0.15f; /* head-ish */
                    }
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

        /* Start aiming – capture the player's position now and freeze for ENEMY_AIM_TIME before firing. */
        {
            /* Capture player's horizontal centre; aim near the head when
             * standing, but if the player is crawling aim lower (knee level)
             * so crouching changes the vertical aim point. */
            e->aim_target_x = game->player.x + PLAYER_W * 0.5f;
            if (game->player.crawling)
            {
                /* Aim a bit above the feet of the crawling box to approximate knees. */
                e->aim_target_y = game->player.y + (float)PLAYER_CRAWL_H * 0.45f;
            }
            else
            {
                /* Aim near the top of the standing player (head). */
                e->aim_target_y = game->player.y + (float)PLAYER_H * 0.15f;
            }
            e->aim_timer = ENEMY_AIM_TIME;
        }
    }

    /* Update enemy bullets: move, wall check, player hit. */
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        Bullet *b = &game->enemy_bullets[i];
        if (!b->active)
            continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        if (b->x + BULLET_W < 0.0f || b->x > game->level.width * (float)TILE_SIZE || b->y + BULLET_H < 0.0f)
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

        if (game->invuln_timer <= 0.0f)
        {
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(b->x, b->y, BULLET_W, BULLET_H,
                              game->player.x, game->player.y, PLAYER_W, ph))
            {
                b->active = false;
                hit_player(game);
            }
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
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
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
        {
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            if (boxes_overlap(game->player.x, game->player.y, PLAYER_W, ph,
                              ex, ey, TILE_SIZE, TILE_SIZE))
            {
                game->state = STATE_LEVEL_CLEARED;
                game->message_timer = 1.2f;
            }
        }
    }

    /* Update particle system every frame so non-player-triggered effects animate. */
    particle_system_update(&game->particles, dt);

    /* Camera: follow player horizontally, clamped to level bounds */
    {
        int win_w = 0, win_h = 0;
        get_view_size(game, &win_w, &win_h);
        float desired = game->player.x + PLAYER_W * 0.5f - (float)win_w * 0.5f;
        float max_cam = game->level.width * (float)TILE_SIZE - (float)win_w;
        if (max_cam < 0.0f)
            max_cam = 0.0f;
        if (desired < 0.0f)
            desired = 0.0f;
        if (desired > max_cam)
            desired = max_cam;
        /* Smoothly interpolate camera towards desired position (lerp). */
        float alpha = 8.0f * dt; /* responsiveness factor */
        if (alpha > 1.0f)
            alpha = 1.0f;
        game->cam_x += (desired - game->cam_x) * alpha;
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
    get_view_size(game, &win_w, &win_h);
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
    const Sprites *spr = &game->sprites;
    const float oy = HUD_HEIGHT;
    int win_w = 0, win_h = 0;
    get_view_size(game, &win_w, &win_h);
    const float cam_x = game->cam_x;

    /* Tiles */
    for (int row = 0; row < lvl->height; ++row)
    {
        for (int col = 0; col < lvl->width; ++col)
        {
            float x = col * (float)TILE_SIZE - cam_x;
            float y = row * (float)TILE_SIZE + oy;
            if (x + TILE_SIZE < 0.0f || x > (float)win_w)
                continue;
            /* Only draw tiles that have been revealed yet. */
            if (!lvl->tiles_visible[row][col])
                continue;
            TileType t = lvl->tiles[row][col];
            if (t == TILE_WALL)
            {
                if (spr->tileset)
                {
                    SDL_FRect src = {(float)(SPRITE_TILE_WALL * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
                    SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
                    SDL_RenderTexture(r, spr->tileset, &src, &dst);
                }
                else
                {
                    SDL_SetRenderDrawColor(r, 110, 70, 40, 255);
                    fill_rect(r, x, y, TILE_SIZE, TILE_SIZE);
                    SDL_SetRenderDrawColor(r, 140, 95, 60, 255);
                    fill_rect(r, x, y, TILE_SIZE, 4);
                }
            }
            else if (t == TILE_LADDER)
            {
                if (spr->tileset)
                {
                    SDL_FRect src = {(float)(SPRITE_TILE_LADDER * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
                    SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
                    SDL_RenderTexture(r, spr->tileset, &src, &dst);
                }
                else
                {
                    SDL_SetRenderDrawColor(r, 210, 200, 90, 255);
                    SDL_RenderLine(r, x + 6, y, x + 6, y + TILE_SIZE);
                    SDL_RenderLine(r, x + TILE_SIZE - 6, y, x + TILE_SIZE - 6, y + TILE_SIZE);
                    for (int rung = 4; rung < TILE_SIZE; rung += 10)
                    {
                        SDL_RenderLine(r, x + 6, y + rung, x + TILE_SIZE - 6, y + rung);
                    }
                }
            }
            else if (t == TILE_ELEVATOR_SHAFT)
            {
                if (spr->tileset)
                {
                    SDL_FRect src = {(float)(SPRITE_TILE_ELEVATOR_SHAFT * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
                    SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
                    SDL_RenderTexture(r, spr->tileset, &src, &dst);
                }
                else
                {
                    /* Draw two vertical guide rails */
                    SDL_SetRenderDrawColor(r, 100, 105, 115, 255);
                    SDL_RenderLine(r, x + 9, y, x + 9, y + TILE_SIZE);
                    SDL_RenderLine(r, x + TILE_SIZE - 9, y, x + TILE_SIZE - 9, y + TILE_SIZE);
                }
            }
        }
    }

    /* If reveal animation still in progress, don't draw other entities yet. */
    if (!lvl->reveal_done)
        return;

    /* Elevator moving platforms */
    for (int i = 0; i < lvl->elevator_count; ++i)
    {
        const Elevator *el = &lvl->elevators[i];
        float px = el->col * (float)TILE_SIZE - cam_x;
        float py = el->y + oy;
        if (spr->tileset)
        {
            SDL_FRect src = {(float)(SPRITE_PLAT_ELEV * SPRITE_CELL), (float)SPRITE_PLAT_ROW_Y, SPRITE_CELL, ELEVATOR_PLAT_H};
            SDL_FRect dst = {px, py, TILE_SIZE, ELEVATOR_PLAT_H};
            SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
            /* Platform body */
            SDL_SetRenderDrawColor(r, 160, 165, 175, 255);
            fill_rect(r, px, py, TILE_SIZE, ELEVATOR_PLAT_H);
            /* Bright top edge */
            SDL_SetRenderDrawColor(r, 220, 225, 235, 255);
            fill_rect(r, px, py, TILE_SIZE, 2);
        }
    }

    /* Falling platforms */
    for (int i = 0; i < lvl->fall_platform_count; ++i)
    {
        const FallPlatform *fp = &lvl->fall_platforms[i];
        if (fp->removed)
            continue;
        float px = fp->col * (float)TILE_SIZE - cam_x;
        float py = fp->y + oy;
        if (spr->tileset)
        {
            SDL_FRect src = {(float)(SPRITE_PLAT_FALL * SPRITE_CELL), (float)SPRITE_PLAT_ROW_Y, SPRITE_CELL, FALL_PLATFORM_H};
            SDL_FRect dst = {px, py, TILE_SIZE, FALL_PLATFORM_H};
            SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
            SDL_SetRenderDrawColor(r, 150, 90, 50, 255);
            fill_rect(r, px, py, TILE_SIZE, FALL_PLATFORM_H);
            SDL_SetRenderDrawColor(r, 210, 160, 120, 255);
            fill_rect(r, px, py, TILE_SIZE, 2);
        }
    }

    /* Moving horizontal platforms */
    for (int i = 0; i < lvl->moving_platform_count; ++i)
    {
        const MovingPlatform *mp = &lvl->moving_platforms[i];
        float px = mp->x - cam_x;
        float py = mp->row * (float)TILE_SIZE + oy;
        if (spr->tileset)
        {
            SDL_FRect src = {(float)(SPRITE_PLAT_MOVE * SPRITE_CELL), (float)SPRITE_PLAT_ROW_Y, SPRITE_CELL, MOVING_PLATFORM_H};
            SDL_FRect dst = {px, py, TILE_SIZE, MOVING_PLATFORM_H};
            SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
            SDL_SetRenderDrawColor(r, 140, 95, 60, 255);
            fill_rect(r, px, py, TILE_SIZE, MOVING_PLATFORM_H);
            SDL_SetRenderDrawColor(r, 210, 160, 120, 255);
            fill_rect(r, px, py, TILE_SIZE, 2);
        }
    }

    /* Doors (all identical — part of the puzzle) */
    for (int d = 0; d < lvl->door_count; ++d)
    {
        float x = lvl->doors[d].col * (float)TILE_SIZE - cam_x;
        float y = lvl->doors[d].row * (float)TILE_SIZE + oy;
        if (spr->tileset)
        {
            SDL_FRect src = {(float)(SPRITE_TILE_DOOR * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
            SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
            SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
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
    }

    /* Exit */
    if (lvl->has_exit)
    {
        float x = lvl->exit_col * (float)TILE_SIZE - cam_x;
        float y = lvl->exit_row * (float)TILE_SIZE + oy;
        if (spr->tileset)
        {
            int tile_idx = (lvl->items_remaining == 0) ? SPRITE_TILE_EXIT_ON : SPRITE_TILE_EXIT_OFF;
            SDL_FRect src = {(float)(tile_idx * SPRITE_CELL), SPRITE_TILE_ROW_Y, SPRITE_CELL, SPRITE_TILE_ROW_H};
            SDL_FRect dst = {x, y, TILE_SIZE, TILE_SIZE};
            SDL_RenderTexture(r, spr->tileset, &src, &dst);
        }
        else
        {
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
    }

    /* Items */
    /* Ensure renderer blending is enabled so per-item alpha is respected. */
    SDL_BlendMode _prev_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(r, &_prev_blend);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int card_pos = 0;
    for (int i = 0; i < lvl->item_count; ++i)
    {
        const Item *it = &lvl->items[i];
        if (it->collected)
            continue;
        float x = it->x - 7.0f - cam_x;
        float y = it->y - 9.0f + oy;

        if (it->type == ITEM_CARD)
        {
            /* Determine per-item alpha during intro animation */
            Uint8 alpha = 255;
            if (game->state == STATE_SHOW_KEYCARD)
                alpha = (card_pos == game->card_anim_current) ? 255 : 90;
            if (spr->items)
            {
                SDL_SetTextureAlphaMod(spr->items, alpha);
                SDL_FRect src = {(float)(SPRITE_ITEM_CARD * SPRITE_CELL), 0, 14, 18};
                SDL_FRect dst = {x, y, 14, 18};
                SDL_RenderTexture(r, spr->items, &src, &dst);
                SDL_SetTextureAlphaMod(spr->items, 255);
            }
            else
            {
                /* Card body */
                SDL_SetRenderDrawColor(r, 30, 190, 200, alpha);
                fill_rect(r, x, y, 14, 18);
                /* Card border */
                SDL_SetRenderDrawColor(r, 200, 255, 255, alpha);
                fill_rect(r, x, y, 14, 2);
                fill_rect(r, x, y + 16, 14, 2);
                fill_rect(r, x, y, 2, 18);
                fill_rect(r, x + 12, y, 2, 18);
                /* Small dot symbol */
                SDL_SetRenderDrawColor(r, 255, 255, 255, alpha);
                fill_rect(r, x + 5, y + 7, 4, 4);
            }
            card_pos++;
        }
        else if (it->type == ITEM_GUN)
        {
            if (spr->items)
            {
                SDL_FRect src = {(float)(SPRITE_ITEM_GUN * SPRITE_CELL), 0, 15, 12};
                SDL_FRect dst = {x + 4, y + 2, 15, 12};
                SDL_RenderTexture(r, spr->items, &src, &dst);
            }
            else
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
        else if (it->type == ITEM_GRENADE)
        {
            if (spr->items)
            {
                SDL_FRect src = {(float)(SPRITE_ITEM_GRENADE * SPRITE_CELL), 0, 13, 14};
                SDL_FRect dst = {x + 3.0f, y + 3.0f, 13, 14};
                SDL_RenderTexture(r, spr->items, &src, &dst);
            }
            else
            {
                /* draw grenade pickup — brown body with lighter pin */
                SDL_SetRenderDrawColor(r, 140, 90, 40, 255);
                fill_rect(r, x + 3.0f, y + 4.0f, GRENADE_W, GRENADE_H);
                SDL_SetRenderDrawColor(r, 220, 180, 110, 255);
                fill_rect(r, x + 5.0f, y + 3.0f, 2.0f, 2.0f); /* pin */
            }
        }
    }
    /* restore previous blend mode */
    SDL_SetRenderDrawBlendMode(r, _prev_blend);

    /* Mines */
    for (int i = 0; i < game->mine_count; ++i)
    {
        const Mine *m = &game->mines[i];
        if (!m->active)
            continue;
        float x = m->x - cam_x;
        float y = m->y + oy;
        if (spr->items)
        {
            SDL_FRect src = {(float)(SPRITE_ITEM_MINE * SPRITE_CELL), 0, MINE_W, MINE_H};
            SDL_FRect dst = {x, y, MINE_W, MINE_H};
            SDL_RenderTexture(r, spr->items, &src, &dst);
        }
        else
        {
            SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
            fill_rect(r, x, y, MINE_W, MINE_H);
            /* small red indicator */
            SDL_SetRenderDrawColor(r, 220, 50, 50, 255);
            fill_rect(r, x + MINE_W * 0.5f - 2.0f, y + 2.0f, 4.0f, 4.0f);
        }
    }

    /* Enemies */
    for (int i = 0; i < game->enemy_count; ++i)
    {
        const Enemy *e = &game->enemies[i];
        if (e->dead)
            continue;
        float x = e->x - cam_x;
        float y = e->y + oy;
        if (spr->enemy)
        {
            SDL_FRect src = {0, 0, ENEMY_W, ENEMY_H};
            SDL_FRect dst = {x, y, ENEMY_W, ENEMY_H};
            if (e->dir < 0)
                SDL_RenderTextureRotated(r, spr->enemy, &src, &dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
            else
                SDL_RenderTexture(r, spr->enemy, &src, &dst);
        }
        else
        {
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
        }
        /* HP dots above the enemy head */
        for (int h = 0; h < ENEMY_HP; ++h)
        {
            if (h < e->hp)
                SDL_SetRenderDrawColor(r, 60, 220, 60, 255);
            else
                SDL_SetRenderDrawColor(r, 70, 30, 30, 255);
            fill_rect(r, x + 2 + h * 7, y - 6, 5, 4);
        }

        /* Speech bubble when talking: a small white rectangle with three dots */
        if (e->talking)
        {
            float bw = 20.0f;
            float bh = 10.0f;
            float bx = x + (ENEMY_W - bw) * 0.5f;
            float by = y - 18.0f - bh; /* place above HP dots */
            if (by < oy)
                by = oy; /* clamp to not draw over HUD */
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            fill_rect(r, bx, by, bw, bh);
            /* small tail */
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            fill_rect(r, x + ENEMY_W * 0.5f - 2.0f, by + bh, 4.0f, 3.0f);
            /* dots */
            SDL_SetRenderDrawColor(r, 30, 30, 35, 255);
            fill_rect(r, bx + 4.0f, by + 3.0f, 3.0f, 3.0f);
            fill_rect(r, bx + 9.0f, by + 3.0f, 3.0f, 3.0f);
            fill_rect(r, bx + 14.0f, by + 3.0f, 3.0f, 3.0f);
        }
    }

    /* Thrown grenades */
    for (int i = 0; i < game->grenade_count; ++i)
    {
        const Grenade *g = &game->grenades[i];
        if (!g->active)
            continue;
        if (spr->items)
        {
            SDL_FRect src = {(float)(SPRITE_ITEM_THROWN * SPRITE_CELL), 0, GRENADE_W, GRENADE_H};
            SDL_FRect dst = {g->x - cam_x, g->y + oy, GRENADE_W, GRENADE_H};
            SDL_RenderTexture(r, spr->items, &src, &dst);
        }
        else
        {
            SDL_SetRenderDrawColor(r, 140, 90, 40, 255);
            fill_rect(r, g->x - cam_x, g->y + oy, GRENADE_W, GRENADE_H);
            SDL_SetRenderDrawColor(r, 220, 180, 110, 255);
            fill_rect(r, g->x - cam_x + 2.0f, g->y + oy + 1.0f, 2.0f, 2.0f);
        }
    }

    /* Bullets (player = yellow, enemy = orange-red) */
    SDL_SetRenderDrawColor(r, 255, 220, 0, 255);
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        const Bullet *b = &game->bullets[i];
        if (!b->active)
            continue;
        fill_rect(r, b->x - cam_x, b->y + oy, BULLET_W, BULLET_H);
    }
    SDL_SetRenderDrawColor(r, 255, 90, 30, 255);
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    {
        const Bullet *b = &game->enemy_bullets[i];
        if (!b->active)
            continue;
        fill_rect(r, b->x - cam_x, b->y + oy, BULLET_W, BULLET_H);
    }

    /* Particles (blood, etc.) */
    particle_system_render(&game->particles, r, oy, cam_x);

    /* Player (blink while invulnerable). Hide player while in dying animation. */
    if (!game->player.dying)
    {
        bool blink = game->invuln_timer > 0.0f &&
                     ((int)(game->invuln_timer * 12.0f) % 2 == 0);
        if (!blink)
        {
            float x = game->player.x - cam_x;
            float y = game->player.y + oy;
            float ph = game->player.crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
            int frame = game->player.crawling ? SPRITE_PLAYER_CRAWL : SPRITE_PLAYER_STAND;
            if (spr->player)
            {
                SDL_FRect src = {(float)(frame * SPRITE_CELL), 0, PLAYER_W, ph};
                SDL_FRect dst = {x, y, PLAYER_W, ph};
                if (game->player.facing < 0)
                    SDL_RenderTextureRotated(r, spr->player, &src, &dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
                else
                    SDL_RenderTexture(r, spr->player, &src, &dst);
            }
            else
            {
                SDL_SetRenderDrawColor(r, 60, 120, 230, 255);
                fill_rect(r, x, y, PLAYER_W, ph);
                SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
                float eye = (game->player.facing > 0) ? x + PLAYER_W - 8 : x + 4;
                fill_rect(r, eye, y + 6, 4, 4);
            }
        }
    }
    else
    {
        /* Draw a gore splat once (use particle[0] as the splat marker) */
        const Particle *s = &game->particles.particles[0];
        if (s->active && s->life > 0.0f)
        {
            SDL_SetRenderDrawColor(r, 90, 10, 10, 255);
            fill_rect(r, s->x - 6.0f, s->y + oy - 4.0f, 12.0f, 6.0f);
        }
    }
}

static void render_hud(Game *game)
{
    SDL_Renderer *r = game->renderer;

    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(game->window, &win_w, &win_h);
    SDL_SetRenderDrawColor(r, 20, 20, 28, 255);
    fill_rect(r, 0, 0, (float)win_w, HUD_HEIGHT);

    char buf[128];
    /* Restore original HUD layout and only append a small 'G' indicator when
     * player has a grenade so the status bar layout is unchanged. */
    if (game->player.grenades > 0)
    {
        SDL_snprintf(buf, sizeof(buf), "LIVES %d  AMMO %d  KEY: %s  LEVEL %d  SCORE %d  G",
                     game->lives,
                     game->player.bullets,
                     game->level.items_remaining == 0 ? "YES" : "NO",
                     game->current_level + 1,
                     game->score);
    }
    else
    {
        SDL_snprintf(buf, sizeof(buf), "LIVES %d  AMMO %d  KEY: %s  LEVEL %d  SCORE %d",
                     game->lives,
                     game->player.bullets,
                     game->level.items_remaining == 0 ? "YES" : "NO",
                     game->current_level + 1,
                     game->score);
    }
    draw_text(r, 8, 12, 2.0f, 240, 240, 240, buf);
}

void game_render(Game *game)
{
    SDL_Renderer *r = game->renderer;

    SDL_SetRenderDrawColor(r, 18, 18, 24, 255);
    SDL_RenderClear(r);

    render_world(game);
    render_hud(game);

    /* No overlay: during STATE_SHOW_KEYCARD we change item opacity when drawing items */

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
    sprites_free(&game->sprites);
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
