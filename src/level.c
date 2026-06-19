#include "level.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

static void place_item(Level *level, int col, int row, ItemType type)
{
    if (level->item_count >= MAX_ITEMS)
    {
        return;
    }
    Item *it = &level->items[level->item_count++];
    it->x = col * TILE_SIZE + TILE_SIZE * 0.5f;
    it->y = row * TILE_SIZE + TILE_SIZE * 0.5f;
    it->collected = false;
    it->respawn_timer = 0.0f;
    it->type = type;
}

static void place_enemy(Level *level, int col, int row, bool has_dog)
{
    if (level->enemy_count >= MAX_ENEMIES)
    {
        return;
    }
    EnemySpawn *e = &level->enemy_spawns[level->enemy_count++];
    /* Stand the enemy on top of the floor tile directly below this cell. */
    e->x = col * TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
    e->y = (row + 1) * TILE_SIZE - ENEMY_H;
    e->has_dog = has_dog;
}

static void place_mine(Level *level, int col, int row)
{
    if (level->mine_count >= MAX_MINES)
    {
        return;
    }
    MineSpawn *m = &level->mine_spawns[level->mine_count++];
    /* Place the mine resting on the floor of the tile */
    m->x = col * TILE_SIZE + (TILE_SIZE - MINE_W) * 0.5f;
    m->y = (row + 1) * TILE_SIZE - MINE_H;
}

static void place_spike(Level *level, int col, int row)
{
    if (level->spike_count >= MAX_SPIKES)
        return;
    SpikeSpawn *s = &level->spike_spawns[level->spike_count++];
    s->x = col * TILE_SIZE;
    s->y = row * TILE_SIZE;
}

static void place_crate(Level *level, int col, int row)
{
    if (level->crate_count >= MAX_CRATES)
        return;
    Crate *crate = &level->crates[level->crate_count++];
    crate->x = col * TILE_SIZE + (TILE_SIZE - CRATE_W) * 0.5f;
    crate->y = (row + 1) * TILE_SIZE - CRATE_H;
    crate->vx = 0.0f;
    crate->vy = 0.0f;
    crate->active = true;
    crate->on_ground = false;
}

bool level_load(Level *level, const char *path)
{
    size_t size = 0;
    char *data = (char *)SDL_LoadFile(path, &size);
    if (data == NULL)
    {
        SDL_Log("Failed to load level '%s': %s", path, SDL_GetError());
        return false;
    }

    SDL_zerop(level);

    int row = 0;
    int col = 0;
    int max_width = 0;
    int start_count = 0;
    int exit_count = 0;
    bool too_wide = false;
    bool too_tall = false;
    bool invalid_spawn_metadata = false;
    int spawn_value_count = 0;

    for (size_t i = 0; i < size; ++i)
    {
        char c = data[i];

        if (c == '\r')
        {
            continue;
        }
        if (c == '\n')
        {
            if (col == 0 && row > 0)
            {
                break; /* blank line marks end of grid section */
            }
            if (col > max_width)
            {
                max_width = col;
            }
            row += 1;
            col = 0;
            if (row >= MAX_LEVEL_HEIGHT)
            {
                size_t next = i + 1;
                if (next < size && data[next] == '\r')
                    next++;
                too_tall = next < size && data[next] != '\n';
                break;
            }
            continue;
        }
        if (col >= MAX_LEVEL_WIDTH)
        {
            too_wide = true;
            continue;
        }

        switch (c)
        {
        case '#':
            level->tiles[row][col] = TILE_WALL;
            break;
        case 'H':
            level->tiles[row][col] = TILE_LADDER;
            break;
        case 'C':
            level->tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_CARD);
            break;
        case 'G':
            level->tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_GUN);
            break;
        case 'N':
            level->tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_GRENADE);
            break;
        case 'K':
            level->tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_MEDKIT);
            break;
        case 'M':
            level->tiles[row][col] = TILE_EMPTY;
            place_enemy(level, col, row, false);
            break;
        case 'W':
            level->tiles[row][col] = TILE_EMPTY;
            place_enemy(level, col, row, true);
            break;
        case 'X':
            level->tiles[row][col] = TILE_EMPTY;
            place_mine(level, col, row);
            break;
        case '^':
            level->tiles[row][col] = TILE_EMPTY;
            place_spike(level, col, row);
            break;
        case 'B':
            level->tiles[row][col] = TILE_EMPTY;
            place_crate(level, col, row);
            break;
        case 'S':
            level->tiles[row][col] = TILE_EMPTY;
            start_count++;
            level->start_x = col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
            level->start_y = (row + 1) * TILE_SIZE - PLAYER_H;
            break;
        case 'E':
            level->tiles[row][col] = TILE_EMPTY;
            exit_count++;
            level->has_exit = true;
            level->exit_col = col;
            level->exit_row = row;
            break;
        case 'D':
            level->tiles[row][col] = TILE_DOOR;
            if (level->door_count < MAX_DOORS)
            {
                level->doors[level->door_count].col = col;
                level->doors[level->door_count].row = row;
                level->door_count++;
            }
            break;
        case 'V':
            level->tiles[row][col] = TILE_ELEVATOR_SHAFT;
            break;
        case 'F':
            level->tiles[row][col] = TILE_EMPTY;
            if (level->fall_platform_count < MAX_FALL_PLATFORMS)
            {
                FallPlatform *fp = &level->fall_platforms[level->fall_platform_count++];
                fp->col = col;
                fp->row = row;
                fp->y = row * (float)TILE_SIZE;
                fp->vy = 0.0f;
                fp->timer = 0.0f;
                fp->triggered = false;
                fp->removed = false;
            }
            break;
        case 'P':
            level->tiles[row][col] = TILE_EMPTY;
            if (level->moving_platform_count < MAX_MOVING_PLATFORMS)
            {
                MovingPlatform *mp = &level->moving_platforms[level->moving_platform_count++];
                mp->row = row;
                mp->x = col * (float)TILE_SIZE;
                mp->left_limit = mp->x;
                mp->right_limit = mp->x;
                mp->vx = MOVING_PLATFORM_SPEED; /* start moving right */
            }
            break;
        default:
            level->tiles[row][col] = TILE_EMPTY;
            break;
        }
        col += 1;
    }

    /* Account for a final line with no trailing newline. */
    if (col > 0)
    {
        if (col > max_width)
        {
            max_width = col;
        }
        row += 1;
    }

    level->width = max_width;
    level->height = row;
    level->card_count = 0;
    for (int i = 0; i < level->item_count; ++i)
    {
        if (level->items[i].type == ITEM_CARD)
            level->card_count++;
    }
    /* Randomly pick one card as the active key card that opens the exit */
    level->active_card_index = -1;
    if (level->card_count > 0)
    {
        int pick = (int)((unsigned)rand() % (unsigned)level->card_count);
        int found = 0;
        for (int i = 0; i < level->item_count; ++i)
        {
            if (level->items[i].type == ITEM_CARD)
            {
                if (found == pick)
                {
                    level->active_card_index = i;
                    break;
                }
                found++;
            }
        }
        level->items_remaining = 1;
    }
    else
    {
        level->items_remaining = 0;
    }

    /* Discover elevator shafts: scan each column for consecutive TILE_ELEVATOR_SHAFT runs. */
    for (int c = 0; c < level->width && level->elevator_count < MAX_ELEVATORS; ++c)
    {
        int r = 0;
        while (r < level->height)
        {
            if (level->tiles[r][c] == TILE_ELEVATOR_SHAFT)
            {
                int first = r;
                while (r < level->height && level->tiles[r][c] == TILE_ELEVATOR_SHAFT)
                    r++;
                int last = r - 1;
                if (last > first && level->elevator_count < MAX_ELEVATORS)
                {
                    Elevator *el = &level->elevators[level->elevator_count++];
                    el->col = c;
                    el->top_limit = first * (float)TILE_SIZE;
                    el->bot_limit = (last + 1) * (float)TILE_SIZE - ELEVATOR_PLAT_H;
                    el->y = el->bot_limit;    /* start at bottom of shaft */
                    el->vy = -ELEVATOR_SPEED; /* initially moving upward */
                }
            }
            else
            {
                r++;
            }
        }
    }

    /* Initialise moving platforms' left/right limits by scanning the row for
     * non-solid tiles. Platforms patrol between the leftmost and rightmost
     * contiguous empty tiles around their spawn column. */
    for (int i = 0; i < level->moving_platform_count; ++i)
    {
        MovingPlatform *mp = &level->moving_platforms[i];
        int col = (int)floorf(mp->x / TILE_SIZE);
        int row_mp = mp->row;
        int lc = col;
        while (lc - 1 >= 0)
        {
            TileType t = level->tiles[row_mp][lc - 1];
            if (t == TILE_WALL || t == TILE_DOOR || t == TILE_ELEVATOR_SHAFT)
                break;
            lc--;
        }
        int rc = col;
        while (rc + 1 < level->width)
        {
            TileType t = level->tiles[row_mp][rc + 1];
            if (t == TILE_WALL || t == TILE_DOOR || t == TILE_ELEVATOR_SHAFT)
                break;
            rc++;
        }
        mp->left_limit = lc * (float)TILE_SIZE;
        mp->right_limit = rc * (float)TILE_SIZE;
        /* Ensure vx has correct sign */
        if (mp->vx > 0.0f)
            mp->vx = MOVING_PLATFORM_SPEED;
        else
            mp->vx = -MOVING_PLATFORM_SPEED;
    }

    /* Parse optional SPAWNS metadata line: "SPAWNS n0 n1 n2 ..."
     * It must appear at the start of a line, after the tile grid. */
    {
        const char *sp = NULL;
        for (size_t k = 0; k + 7 <= size; ++k)
        {
            if (data[k] == '\n')
            {
                size_t j = k + 1;
                if (j < size && data[j] == '\r')
                    j++;
                if (j + 7 <= size && SDL_strncmp(data + j, "SPAWNS ", 7) == 0)
                {
                    sp = data + j + 7;
                    break;
                }
            }
        }
        if (sp)
        {
            const char *line_end = sp;
            const char *data_end = data + size;
            while (line_end < data_end && *line_end != '\r' && *line_end != '\n')
                line_end++;

            while (sp < line_end)
            {
                while (sp < line_end && (*sp == ' ' || *sp == '\t'))
                    sp++;
                if (sp >= line_end)
                    break;

                if (*sp < '0' || *sp > '9')
                {
                    invalid_spawn_metadata = true;
                    break;
                }

                int n = 0;
                while (sp < line_end && *sp >= '0' && *sp <= '9')
                {
                    int digit = *sp++ - '0';
                    if (n > (INT_MAX - digit) / 10)
                    {
                        invalid_spawn_metadata = true;
                        break;
                    }
                    n = n * 10 + digit;
                }
                if (invalid_spawn_metadata)
                    break;
                if (sp < line_end && *sp != ' ' && *sp != '\t')
                {
                    invalid_spawn_metadata = true;
                    break;
                }

                if (spawn_value_count < level->door_count)
                    level->door_spawn_counts[spawn_value_count] = n;
                spawn_value_count++;
            }

            if (spawn_value_count != level->door_count)
                invalid_spawn_metadata = true;
        }
    }

    SDL_free(data);

    if (level->width <= 0 || level->height <= 0)
    {
        SDL_Log("Level '%s' is empty", path);
        return false;
    }
    if (too_wide)
    {
        SDL_Log("Level '%s' exceeds max width of %d tiles", path, MAX_LEVEL_WIDTH);
        return false;
    }
    if (too_tall)
    {
        SDL_Log("Level '%s' exceeds max height of %d tiles", path, MAX_LEVEL_HEIGHT);
        return false;
    }
    if (start_count != 1)
    {
        SDL_Log("Level '%s' must contain exactly one player start 'S' (found %d)",
                path, start_count);
        return false;
    }
    if (exit_count != 1)
    {
        SDL_Log("Level '%s' must contain exactly one exit 'E' (found %d)",
                path, exit_count);
        return false;
    }
    if ((level->door_count % 2) != 0)
    {
        SDL_Log("Level '%s' has an unpaired door (found %d doors)",
                path, level->door_count);
        return false;
    }
    if (invalid_spawn_metadata)
    {
        SDL_Log("Level '%s' has invalid SPAWNS metadata "
                "(expected %d values, found %d)",
                path, level->door_count, spawn_value_count);
        return false;
    }
    return true;
}

TileType level_tile(const Level *level, int col, int row)
{
    if (col < 0 || col >= level->width || row < 0 || row >= level->height)
    {
        return TILE_WALL;
    }
    return level->tiles[row][col];
}

bool level_is_solid(const Level *level, int col, int row)
{
    return level_tile(level, col, row) == TILE_WALL;
}

bool level_is_ladder(const Level *level, int col, int row)
{
    if (col < 0 || col >= level->width || row < 0 || row >= level->height)
    {
        return false;
    }
    return level->tiles[row][col] == TILE_LADDER;
}

void level_move(Level *level, float *x, float *y, float *vx, float *vy,
                float w, float h, float dt, bool climbing, bool *on_ground,
                bool triggers_falling)
{
    *on_ground = false;
    float prev_y = *y;

    /* --- Horizontal movement --- */
    *x += *vx * dt;
    {
        int top = (int)floorf(*y / TILE_SIZE);
        int bottom = (int)floorf((*y + h - 1.0f) / TILE_SIZE);

        if (*vx > 0.0f)
        {
            int col = (int)floorf((*x + w) / TILE_SIZE);
            for (int r = top; r <= bottom; ++r)
            {
                if (level_is_solid(level, col, r))
                {
                    *x = col * (float)TILE_SIZE - w;
                    *vx = 0.0f;
                    break;
                }
            }
        }
        else if (*vx < 0.0f)
        {
            int col = (int)floorf(*x / TILE_SIZE);
            for (int r = top; r <= bottom; ++r)
            {
                if (level_is_solid(level, col, r))
                {
                    *x = (col + 1) * (float)TILE_SIZE;
                    *vx = 0.0f;
                    break;
                }
            }
        }
    }

    /* --- Vertical movement --- */
    *y += *vy * dt;
    {
        int left = (int)floorf(*x / TILE_SIZE);
        int right = (int)floorf((*x + w - 1.0f) / TILE_SIZE);

        if (*vy > 0.0f)
        {
            int row = (int)floorf((*y + h) / TILE_SIZE);
            float feet = *y + h;
            float prev_feet = prev_y + h;
            for (int c = left; c <= right; ++c)
            {
                if (level_is_solid(level, c, row))
                {
                    *y = row * (float)TILE_SIZE - h;
                    *vy = 0.0f;
                    *on_ground = true;
                    break;
                }
                if (!climbing && level_is_ladder(level, c, row))
                {
                    float tile_top = row * (float)TILE_SIZE;
                    /* One-way platform: only land when the feet cross the top. */
                    if (prev_feet <= tile_top + 1.0f && feet >= tile_top)
                    {
                        *y = tile_top - h;
                        *vy = 0.0f;
                        *on_ground = true;
                        break;
                    }
                }
                /* Falling platforms: treat as one-way platforms that can be triggered. */
                for (int p = 0; p < level->fall_platform_count; ++p)
                {
                    FallPlatform *fp = &level->fall_platforms[p];
                    if (fp->removed)
                        continue;
                    if (fp->col != c)
                        continue;
                    /* Use platform's current top position for landing tests. */
                    float plat_top = fp->y;
                    if (prev_feet <= plat_top + 1.0f && feet >= plat_top)
                    {
                        *y = plat_top - h;
                        *vy = 0.0f;
                        *on_ground = true;
                        /* Trigger platform to start falling after a short delay. */
                        if (triggers_falling && !fp->triggered)
                        {
                            fp->triggered = true;
                            fp->timer = 0.0f;
                        }
                        goto landed_check_done;
                    }
                }
                /* Moving platforms: treat like one-way platforms that patrol horizontally. */
                for (int m = 0; m < level->moving_platform_count; ++m)
                {
                    MovingPlatform *mp = &level->moving_platforms[m];
                    if (mp->row != row)
                        continue;
                    int plat_col = (int)floorf(mp->x / TILE_SIZE);
                    if (plat_col != c)
                        continue;
                    float plat_top = mp->row * (float)TILE_SIZE;
                    if (prev_feet <= plat_top + 1.0f && feet >= plat_top)
                    {
                        *y = plat_top - h;
                        *vy = 0.0f;
                        *on_ground = true;
                        goto landed_check_done;
                    }
                }
            }
        landed_check_done:;
        }
        else if (*vy < 0.0f)
        {
            int row = (int)floorf(*y / TILE_SIZE);
            for (int c = left; c <= right; ++c)
            {
                if (level_is_solid(level, c, row))
                {
                    *y = (row + 1) * (float)TILE_SIZE;
                    *vy = 0.0f;
                    break;
                }
            }
        }
    }
}

void level_update_falling_platforms(Level *level, float dt)
{
    for (int i = 0; i < level->fall_platform_count; ++i)
    {
        FallPlatform *fp = &level->fall_platforms[i];
        if (fp->removed || !fp->triggered)
            continue;
        fp->timer += dt;
        if (fp->timer < FALL_PLATFORM_TRIGGER_DELAY)
            continue;
        /* Start accelerating downward. */
        fp->vy += FALL_PLATFORM_ACCEL * dt;
        fp->y += fp->vy * dt;
        /* Remove platform once it has fallen entirely out of its tile. */
        if (fp->y >= (fp->row + 1) * (float)TILE_SIZE)
        {
            fp->removed = true;
        }
    }
}

void level_update_elevators(Level *level, float dt)
{
    for (int i = 0; i < level->elevator_count; ++i)
    {
        Elevator *el = &level->elevators[i];
        el->y += el->vy * dt;
        if (el->vy < 0.0f && el->y <= el->top_limit)
        {
            el->y = el->top_limit;
            el->vy = ELEVATOR_SPEED;
        }
        else if (el->vy > 0.0f && el->y >= el->bot_limit)
        {
            el->y = el->bot_limit;
            el->vy = -ELEVATOR_SPEED;
        }
    }
}

void level_update_moving_platforms(Level *level, float dt)
{
    for (int i = 0; i < level->moving_platform_count; ++i)
    {
        MovingPlatform *mp = &level->moving_platforms[i];
        mp->x += mp->vx * dt;
        if (mp->vx > 0.0f && mp->x >= mp->right_limit)
        {
            mp->x = mp->right_limit;
            mp->vx = -MOVING_PLATFORM_SPEED;
        }
        else if (mp->vx < 0.0f && mp->x <= mp->left_limit)
        {
            mp->x = mp->left_limit;
            mp->vx = MOVING_PLATFORM_SPEED;
        }
    }
}

void level_reveal_init(Level *level)
{
    /* Default: hide everything and start reveal from top-left. */
    for (int r = 0; r < MAX_LEVEL_HEIGHT; ++r)
    {
        for (int c = 0; c < MAX_LEVEL_WIDTH; ++c)
            level->tiles_visible[r][c] = false;
    }
    level->reveal_next_row = 0;
    level->reveal_next_col = 0;
    level->reveal_timer = 0.0f;
    /* Reduced interval and larger batch to make reveal noticeably faster. */
    level->reveal_interval = 0.004f; /* smaller delay between reveal steps */
    level->reveal_done = false;
}

bool level_reveal_step(Level *level, float dt)
{
    if (level->reveal_done)
        return false;

    level->reveal_timer += dt;
    const int batch = 12; /* reveal this many tiles per interval (larger = faster) */
    bool just_finished = false;
    while (level->reveal_timer >= level->reveal_interval && !level->reveal_done)
    {
        level->reveal_timer -= level->reveal_interval;
        for (int b = 0; b < batch && !level->reveal_done; ++b)
        {
            if (level->reveal_next_row >= level->height)
            {
                level->reveal_done = true;
                just_finished = true;
                break;
            }
            /* Only iterate up to the level width for each row. */
            if (level->reveal_next_col < level->width)
            {
                level->tiles_visible[level->reveal_next_row][level->reveal_next_col] = true;
            }
            level->reveal_next_col++;
            if (level->reveal_next_col >= level->width)
            {
                level->reveal_next_col = 0;
                level->reveal_next_row++;
            }
        }
    }
    return just_finished;
}
