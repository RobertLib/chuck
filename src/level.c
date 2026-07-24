#include "level.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void place_item(Level *level, int col, int row, ItemType type)
{
    if (level->runtime.item_count >= MAX_ITEMS)
    {
        return;
    }
    Item *it = &level->runtime.items[level->runtime.item_count++];
    it->x = col * TILE_SIZE + TILE_SIZE * 0.5f;
    it->y = row * TILE_SIZE + TILE_SIZE * 0.5f;
    it->collected = false;
    it->respawn_timer = 0.0f;
    it->type = type;
}

static void place_enemy(Level *level, int col, int row, bool has_dog)
{
    if (level->map.enemy_count >= MAX_ENEMIES)
    {
        return;
    }
    EnemySpawn *e = &level->map.enemy_spawns[level->map.enemy_count++];
    /* Stand the enemy on top of the floor tile directly below this cell. */
    e->x = col * TILE_SIZE + (TILE_SIZE - ENEMY_W) * 0.5f;
    e->y = (row + 1) * TILE_SIZE - ENEMY_H;
    e->has_dog = has_dog;
}

static void place_janitor(Level *level, int col, int row)
{
    if (level->map.janitor_count >= MAX_JANITORS)
        return;
    JanitorSpawn *janitor =
        &level->map.janitor_spawns[level->map.janitor_count++];
    janitor->x = col * TILE_SIZE + (TILE_SIZE - JANITOR_W) * 0.5f;
    janitor->y = (row + 1) * TILE_SIZE - JANITOR_H;
}

static void place_mine(Level *level, int col, int row)
{
    if (level->map.mine_count >= MAX_MINES)
    {
        return;
    }
    MineSpawn *m = &level->map.mine_spawns[level->map.mine_count++];
    /* Place the mine resting on the floor of the tile */
    m->x = col * TILE_SIZE + (TILE_SIZE - MINE_W) * 0.5f;
    m->y = (row + 1) * TILE_SIZE - MINE_H;
}

static void place_spike(Level *level, int col, int row)
{
    if (level->map.spike_count >= MAX_SPIKES)
        return;
    SpikeSpawn *s = &level->map.spike_spawns[level->map.spike_count++];
    s->x = col * TILE_SIZE;
    s->y = row * TILE_SIZE;
}

static void place_ceiling_fan(Level *level, int col, int row)
{
    if (level->map.ceiling_fan_count >= MAX_CEILING_FANS)
        return;
    CeilingFan *fan = &level->map.ceiling_fans[level->map.ceiling_fan_count++];
    fan->x = col * TILE_SIZE + TILE_SIZE * 0.5f;
    fan->y = row * TILE_SIZE + CEILING_FAN_CENTER_Y;
}

static void place_crate(Level *level, int col, int row)
{
    if (level->runtime.crate_count >= MAX_CRATES)
        return;
    Crate *crate = &level->runtime.crates[level->runtime.crate_count++];
    crate->x = col * TILE_SIZE + (TILE_SIZE - CRATE_W) * 0.5f;
    crate->y = (row + 1) * TILE_SIZE - CRATE_H;
    crate->vx = 0.0f;
    crate->vy = 0.0f;
    crate->active = true;
    crate->on_ground = false;
}

static void place_gas_canister(Level *level, int col, int row)
{
    if (level->runtime.gas_canister_count >= MAX_GAS_CANISTERS)
        return;
    GasCanister *canister =
        &level->runtime.gas_canisters[level->runtime.gas_canister_count++];
    canister->x = col * TILE_SIZE +
                  (TILE_SIZE - GAS_CANISTER_W) * 0.5f;
    canister->y = (row + 1) * TILE_SIZE - GAS_CANISTER_H;
    canister->active = true;
}

static void place_terminal(Level *level, int col, int row)
{
    if (level->map.terminal_count >= MAX_TERMINALS)
        return;
    Terminal *terminal = &level->map.terminals[level->map.terminal_count++];
    terminal->col = col;
    terminal->row = row;
}

static void place_alarm_switch(Level *level, int col, int row)
{
    if (level->map.alarm_switch_count >= MAX_ALARM_SWITCHES)
        return;
    AlarmSwitch *alarm_switch =
        &level->map.alarm_switches[level->map.alarm_switch_count++];
    alarm_switch->col = col;
    alarm_switch->row = row;
}

static void place_decoration(Level *level, int col, int row,
                             DecorationType type)
{
    if (level->map.decoration_count >= MAX_DECORATIONS)
        return;
    Decoration *decoration = &level->map.decorations[level->map.decoration_count++];
    decoration->col = col;
    decoration->row = row;
    decoration->type = type;
}

bool level_load_data(Level *level, const char *name,
                     const char *data, size_t size, Rng *rng)
{
    if (data == NULL)
    {
        fprintf(stderr, "Embedded level '%s' has no data\n", name);
        return false;
    }

    memset(level, 0, sizeof(*level));

    int row = 0;
    int col = 0;
    int max_width = 0;
    int start_count = 0;
    int exit_count = 0;
    int sublevel_entrance_count = 0;
    int sublevel_return_count = 0;
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
        case '.':
            level->map.tiles[row][col] = TILE_EMPTY;
            break;
        case '#':
            level->map.tiles[row][col] = TILE_WALL;
            break;
        case 'H':
            level->map.tiles[row][col] = TILE_LADDER;
            break;
        case 'C':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_CARD);
            break;
        case 'G':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_GUN);
            break;
        case 'N':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_GRENADE);
            break;
        case 'K':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_MEDKIT);
            break;
        case 'Z':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_item(level, col, row, ITEM_BAZOOKA);
            break;
        case 'M':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_enemy(level, col, row, false);
            break;
        case 'W':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_enemy(level, col, row, true);
            break;
        case 'J':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_janitor(level, col, row);
            break;
        case 'X':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_mine(level, col, row);
            break;
        case '^':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_spike(level, col, row);
            break;
        case 'O':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_ceiling_fan(level, col, row);
            break;
        case 'B':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_crate(level, col, row);
            break;
        case 'L':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_gas_canister(level, col, row);
            break;
        case 'T':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_terminal(level, col, row);
            break;
        case 'A':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_alarm_switch(level, col, row);
            break;
        case 'c':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_OFFICE_CHAIR);
            break;
        case 'd':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_OFFICE_DESK);
            break;
        case 'i':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_OFFICE_EQUIPMENT);
            break;
        case 'q':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_RESTROOM_TOILET);
            level->map.restroom_theme = true;
            break;
        case 'b':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_RESTROOM_BASIN);
            level->map.restroom_theme = true;
            break;
        case 'p':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_RESTROOM_PARTITION);
            level->map.restroom_theme = true;
            break;
        case 'o':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_RESTROOM_STALL_OPEN);
            level->map.restroom_theme = true;
            break;
        case 'z':
            level->map.tiles[row][col] = TILE_EMPTY;
            place_decoration(level, col, row, DECOR_RESTROOM_STALL_CLOSED);
            level->map.restroom_theme = true;
            break;
        case 'S':
            level->map.tiles[row][col] = TILE_EMPTY;
            start_count++;
            level->map.start_x = col * TILE_SIZE + (TILE_SIZE - PLAYER_W) * 0.5f;
            level->map.start_y = (row + 1) * TILE_SIZE - PLAYER_H;
            break;
        case 'E':
            level->map.tiles[row][col] = TILE_EMPTY;
            exit_count++;
            level->map.has_exit = true;
            level->map.exit_col = col;
            level->map.exit_row = row;
            break;
        case 'D':
            level->map.tiles[row][col] = TILE_DOOR;
            if (level->map.door_count < MAX_DOORS)
            {
                level->map.doors[level->map.door_count].col = col;
                level->map.doors[level->map.door_count].row = row;
                level->map.door_count++;
            }
            break;
        case 'U':
            level->map.tiles[row][col] = TILE_SUBLEVEL_DOOR;
            sublevel_entrance_count++;
            level->map.has_sublevel_entrance = true;
            level->map.sublevel_entrance_col = col;
            level->map.sublevel_entrance_row = row;
            break;
        case 'R':
            level->map.tiles[row][col] = TILE_SUBLEVEL_DOOR;
            sublevel_return_count++;
            level->map.has_sublevel_return = true;
            level->map.sublevel_return_col = col;
            level->map.sublevel_return_row = row;
            break;
        case 'V':
            level->map.tiles[row][col] = TILE_ELEVATOR_SHAFT;
            break;
        case 'F':
            level->map.tiles[row][col] = TILE_EMPTY;
            if (level->runtime.fall_platform_count < MAX_FALL_PLATFORMS)
            {
                FallPlatform *fp = &level->runtime.fall_platforms[level->runtime.fall_platform_count++];
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
            level->map.tiles[row][col] = TILE_EMPTY;
            if (level->runtime.moving_platform_count < MAX_MOVING_PLATFORMS)
            {
                MovingPlatform *mp = &level->runtime.moving_platforms[level->runtime.moving_platform_count++];
                mp->row = row;
                mp->x = col * (float)TILE_SIZE;
                mp->left_limit = mp->x;
                mp->right_limit = mp->x;
                mp->vx = MOVING_PLATFORM_SPEED; /* start moving right */
            }
            break;
        default:
            level->map.tiles[row][col] = TILE_EMPTY;
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

    level->map.width = max_width;
    level->map.height = row;

    /* Office props are floor-standing and visual only. Keep them off ladders,
     * shafts, falling/moving platforms and open air so they can never be left
     * hovering after level geometry moves. */
    int supported_decoration_count = 0;
    for (int i = 0; i < level->map.decoration_count; ++i)
    {
        Decoration *decoration = &level->map.decorations[i];
        int floor_row = decoration->row + 1;
        if (floor_row >= level->map.height ||
            level->map.tiles[floor_row][decoration->col] != TILE_WALL)
        {
            continue;
        }
        level->map.decorations[supported_decoration_count++] = *decoration;
    }
    level->map.decoration_count = supported_decoration_count;

    level->runtime.card_count = 0;
    for (int i = 0; i < level->runtime.item_count; ++i)
    {
        if (level->runtime.items[i].type == ITEM_CARD)
            level->runtime.card_count++;
    }
    /* Randomly pick one card as the active key card that opens the exit */
    level->runtime.active_card_index = -1;
    if (level->runtime.card_count > 0)
    {
        int pick = rng_range(rng, level->runtime.card_count);
        int found = 0;
        for (int i = 0; i < level->runtime.item_count; ++i)
        {
            if (level->runtime.items[i].type == ITEM_CARD)
            {
                if (found == pick)
                {
                    level->runtime.active_card_index = i;
                    break;
                }
                found++;
            }
        }
        level->runtime.items_remaining = 1;
    }
    else
    {
        level->runtime.items_remaining = 0;
    }

    /* Only one of the visually identical terminals is connected to security.
     * Prefer terminals well away from the player spawn so the alternative
     * route still requires infiltrating a meaningful part of the level. */
    level->runtime.active_terminal_index = -1;
    if (level->map.terminal_count > 0)
    {
        int start_col =
            (int)floorf((level->map.start_x + PLAYER_W * 0.5f) / TILE_SIZE);
        int start_row =
            (int)floorf((level->map.start_y + PLAYER_H * 0.5f) / TILE_SIZE);
        int eligible_count = 0;
        for (int i = 0; i < level->map.terminal_count; ++i)
        {
            const Terminal *terminal = &level->map.terminals[i];
            int tile_distance =
                abs(terminal->col - start_col) +
                abs(terminal->row - start_row);
            if (tile_distance >= TERMINAL_MIN_START_TILES)
                eligible_count++;
        }

        if (eligible_count > 0)
        {
            int pick = rng_range(rng, eligible_count);
            for (int i = 0; i < level->map.terminal_count; ++i)
            {
                const Terminal *terminal = &level->map.terminals[i];
                int tile_distance =
                    abs(terminal->col - start_col) +
                    abs(terminal->row - start_row);
                if (tile_distance < TERMINAL_MIN_START_TILES)
                    continue;
                if (pick-- == 0)
                {
                    level->runtime.active_terminal_index = i;
                    break;
                }
            }
        }
        else
        {
            /* Keep custom maps playable even if all their terminals are close. */
            level->runtime.active_terminal_index =
                rng_range(rng, level->map.terminal_count);
        }
    }
    level->runtime.terminal_hacked = false;
    /* Preserve the old behavior for maps with neither an access card nor a
     * terminal. Maps containing either route start with a locked exit. */
    level->runtime.exit_unlocked =
        level->runtime.card_count == 0 && level->map.terminal_count == 0;

    /* Discover elevator shafts: scan each column for consecutive TILE_ELEVATOR_SHAFT runs. */
    for (int c = 0; c < level->map.width && level->runtime.elevator_count < MAX_ELEVATORS; ++c)
    {
        int r = 0;
        while (r < level->map.height)
        {
            if (level->map.tiles[r][c] == TILE_ELEVATOR_SHAFT)
            {
                int first = r;
                while (r < level->map.height && level->map.tiles[r][c] == TILE_ELEVATOR_SHAFT)
                    r++;
                int last = r - 1;
                if (last > first && level->runtime.elevator_count < MAX_ELEVATORS)
                {
                    Elevator *el = &level->runtime.elevators[level->runtime.elevator_count++];
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
    for (int i = 0; i < level->runtime.moving_platform_count; ++i)
    {
        MovingPlatform *mp = &level->runtime.moving_platforms[i];
        int col = (int)floorf(mp->x / TILE_SIZE);
        int row_mp = mp->row;
        int lc = col;
        while (lc - 1 >= 0)
        {
            TileType t = level->map.tiles[row_mp][lc - 1];
            if (t == TILE_WALL || t == TILE_DOOR || t == TILE_ELEVATOR_SHAFT)
                break;
            lc--;
        }
        int rc = col;
        while (rc + 1 < level->map.width)
        {
            TileType t = level->map.tiles[row_mp][rc + 1];
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
                if (j + 7 <= size && strncmp(data + j, "SPAWNS ", 7) == 0)
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

                if (spawn_value_count < level->map.door_count)
                    level->map.door_spawn_counts[spawn_value_count] = n;
                spawn_value_count++;
            }

            if (spawn_value_count != level->map.door_count)
                invalid_spawn_metadata = true;
        }
    }

    if (level->map.width <= 0 || level->map.height <= 0)
    {
        fprintf(stderr, "Level '%s' is empty\n", name);
        return false;
    }
    if (too_wide)
    {
        fprintf(stderr, "Level '%s' exceeds max width of %d tiles\n",
                name, MAX_LEVEL_WIDTH);
        return false;
    }
    if (too_tall)
    {
        fprintf(stderr, "Level '%s' exceeds max height of %d tiles\n",
                name, MAX_LEVEL_HEIGHT);
        return false;
    }
    if (start_count != 1)
    {
        fprintf(stderr,
                "Level '%s' must contain exactly one player start 'S' (found %d)\n",
                name, start_count);
        return false;
    }
    if (exit_count + sublevel_return_count != 1)
    {
        fprintf(stderr,
                "Level '%s' must contain exactly one campaign exit 'E' or "
                "sublevel return 'R' (found %d)\n",
                name, exit_count + sublevel_return_count);
        return false;
    }
    if (sublevel_entrance_count > 1)
    {
        fprintf(stderr,
                "Level '%s' may contain at most one sublevel entrance 'U' "
                "(found %d)\n",
                name, sublevel_entrance_count);
        return false;
    }
    if (sublevel_return_count > 1)
    {
        fprintf(stderr,
                "Level '%s' may contain at most one sublevel return 'R' "
                "(found %d)\n",
                name, sublevel_return_count);
        return false;
    }
    if ((level->map.door_count % 2) != 0)
    {
        fprintf(stderr, "Level '%s' has an unpaired door (found %d doors)\n",
                name, level->map.door_count);
        return false;
    }
    if (invalid_spawn_metadata)
    {
        fprintf(stderr,
                "Level '%s' has invalid SPAWNS metadata "
                "(expected %d values, found %d)\n",
                name, level->map.door_count, spawn_value_count);
        return false;
    }
    return true;
}

TileType level_tile(const Level *level, int col, int row)
{
    if (col < 0 || col >= level->map.width || row < 0 || row >= level->map.height)
    {
        return TILE_WALL;
    }
    return level->map.tiles[row][col];
}

bool level_is_solid(const Level *level, int col, int row)
{
    return level_tile(level, col, row) == TILE_WALL;
}

bool level_is_ladder(const Level *level, int col, int row)
{
    if (col < 0 || col >= level->map.width || row < 0 || row >= level->map.height)
    {
        return false;
    }
    return level->map.tiles[row][col] == TILE_LADDER;
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
                for (int p = 0; p < level->runtime.fall_platform_count; ++p)
                {
                    FallPlatform *fp = &level->runtime.fall_platforms[p];
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
                for (int m = 0; m < level->runtime.moving_platform_count; ++m)
                {
                    MovingPlatform *mp = &level->runtime.moving_platforms[m];
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
    for (int i = 0; i < level->runtime.fall_platform_count; ++i)
    {
        FallPlatform *fp = &level->runtime.fall_platforms[i];
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
    for (int i = 0; i < level->runtime.elevator_count; ++i)
    {
        Elevator *el = &level->runtime.elevators[i];
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
    for (int i = 0; i < level->runtime.moving_platform_count; ++i)
    {
        MovingPlatform *mp = &level->runtime.moving_platforms[i];
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
            level->reveal.tiles_visible[r][c] = false;
    }
    level->reveal.next_row = 0;
    level->reveal.next_col = 0;
    level->reveal.timer = 0.0f;
    /* Reduced interval and larger batch to make reveal noticeably faster. */
    level->reveal.interval = 0.004f; /* smaller delay between reveal steps */
    level->reveal.done = false;
}

bool level_reveal_step(Level *level, float dt)
{
    if (level->reveal.done)
        return false;

    level->reveal.timer += dt;
    const int batch = 12; /* reveal this many tiles per interval (larger = faster) */
    bool just_finished = false;
    while (level->reveal.timer >= level->reveal.interval && !level->reveal.done)
    {
        level->reveal.timer -= level->reveal.interval;
        for (int b = 0; b < batch && !level->reveal.done; ++b)
        {
            if (level->reveal.next_row >= level->map.height)
            {
                level->reveal.done = true;
                just_finished = true;
                break;
            }
            /* Only iterate up to the level width for each row. */
            if (level->reveal.next_col < level->map.width)
            {
                level->reveal.tiles_visible[level->reveal.next_row][level->reveal.next_col] = true;
            }
            level->reveal.next_col++;
            if (level->reveal.next_col >= level->map.width)
            {
                level->reveal.next_col = 0;
                level->reveal.next_row++;
            }
        }
    }
    return just_finished;
}
