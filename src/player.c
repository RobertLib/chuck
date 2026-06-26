#include "player.h"

#include <math.h>

void player_reset(Player *player, const Level *level)
{
    player->x = level->start_x;
    player->y = level->start_y;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->on_ground = false;
    player->on_ladder = false;
    player->facing = 1;
    player->bullets = MAX_AMMO;
    player->grenades = 0;
    player->dying = false;
    player->death_timer = 0.0f;
    player->crawling = false;
}

/* True when the player box overlaps a ladder near its center or feet. */
static bool player_over_ladder(const Player *player, const Level *level)
{
    int col = (int)floorf((player->x + PLAYER_W * 0.5f) / TILE_SIZE);
    float ph = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    int row_center = (int)floorf((player->y + ph * 0.5f) / TILE_SIZE);
    int row_feet = (int)floorf((player->y + ph - 1.0f) / TILE_SIZE);
    return level_is_ladder(level, col, row_center) ||
           level_is_ladder(level, col, row_feet);
}

void player_update(Player *player, Level *level, const Input *input, float dt)
{
    /* Horizontal intent */
    float move = 0.0f;
    if (input->left)
    {
        move -= 1.0f;
        player->facing = -1;
    }
    if (input->right)
    {
        move += 1.0f;
        player->facing = 1;
    }
    /* Horizontal speed depends on crawling state */
    /* Determine crawling intent: holding down while on ground and not on ladder */
    bool want_crawl = input->down && player->on_ground && !player->on_ladder;
    if (want_crawl && !player->crawling)
    {
        /* Enter crawling: lower the collision box while keeping feet in place */
        player->y += (float)(PLAYER_H - PLAYER_CRAWL_H);
        player->crawling = true;
    }
    else if (!want_crawl && player->crawling)
    {
        /* Try to stand up: check there's room above before rising */
        float new_y = player->y - (float)(PLAYER_H - PLAYER_CRAWL_H);
        int left_col = (int)floorf(player->x / TILE_SIZE);
        int right_col = (int)floorf((player->x + PLAYER_W - 1.0f) / TILE_SIZE);
        int top_row = (int)floorf(new_y / TILE_SIZE);
        int bottom_row = (int)floorf((new_y + PLAYER_H - 1.0f) / TILE_SIZE);
        bool blocked = false;
        for (int r = top_row; r <= bottom_row && !blocked; ++r)
        {
            for (int c = left_col; c <= right_col; ++c)
            {
                if (level_is_solid(level, c, r))
                {
                    blocked = true;
                    break;
                }
            }
        }
        if (!blocked)
        {
            player->y = new_y;
            player->crawling = false;
        }
        else
        {
            /* remain crawling if blocked */
            player->crawling = true;
        }
    }

    if (player->crawling)
        player->vx = move * PLAYER_CRAWL_SPEED;
    else
        player->vx = move * PLAYER_WALK_SPEED;

    bool over_ladder = player_over_ladder(player, level);

    /* Mount the ladder by pressing up/down; stay on it until we leave it. */
    if (over_ladder && (input->up || input->down))
    {
        player->on_ladder = true;
    }
    if (!over_ladder)
    {
        player->on_ladder = false;
    }

    if (player->on_ladder)
    {
        float climb = 0.0f;
        if (input->up)
        {
            climb -= 1.0f;
        }
        if (input->down)
        {
            climb += 1.0f;
        }
        player->vy = climb * PLAYER_CLIMB_SPEED;

        /* Jump off the ladder */
        if (input->jump)
        {
            player->vy = -PLAYER_JUMP_SPEED;
            player->on_ladder = false;
        }
    }
    else
    {
        player->vy += GRAVITY * dt;
        if (player->vy > MAX_FALL_SPEED)
        {
            player->vy = MAX_FALL_SPEED;
        }
        if (input->jump && player->on_ground)
        {
            player->vy = -PLAYER_JUMP_SPEED;
        }
    }

    float ph = player->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
    level_move(level, &player->x, &player->y, &player->vx, &player->vy,
               PLAYER_W, ph, dt, player->on_ladder, &player->on_ground,
               true);
}
