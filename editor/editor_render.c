#include "editor.h"

#include "fx.h"
#include "level_art.h"

#include <math.h>

/* ---- Small drawing helpers -------------------------------------------- */

static void tri(SDL_Renderer *r, SDL_Color c, float x1, float y1, float x2,
                float y2, float x3, float y3)
{
    SDL_FColor fc = fx_fcolor(c, (float)c.a / 255.0f);
    SDL_Vertex v[3] = {{{x1, y1}, fc, {0.0f, 0.0f}},
                       {{x2, y2}, fc, {0.0f, 0.0f}},
                       {{x3, y3}, fc, {0.0f, 0.0f}}};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 3, NULL, 0);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void outline(SDL_Renderer *r, SDL_Color c, float x, float y, float w,
                    float h, float thickness)
{
    fx_rect(r, c, x, y, w, thickness);
    fx_rect(r, c, x, y + h - thickness, w, thickness);
    fx_rect(r, c, x, y, thickness, h);
    fx_rect(r, c, x + w - thickness, y, thickness, h);
}

static void disc(SDL_Renderer *r, SDL_Color c, float cx, float cy, float radius)
{
    for (float dy = -radius; dy <= radius; dy += 1.0f)
    {
        float half = sqrtf(radius * radius - dy * dy);
        fx_rect(r, c, cx - half, cy + dy, half * 2.0f, 1.0f);
    }
}

SDL_Color ed_symbol_color(char symbol)
{
    const EdSymbol *entry = editor_symbol(symbol);
    if (entry == NULL)
        return FX_RED;
    return (SDL_Color){entry->r, entry->g, entry->b, 255};
}

/* ---- Icons ------------------------------------------------------------- */

/*
 * One drawing per legend character, sized to fit any box. They are not the
 * game's sprites — the point is to tell 44 characters apart at a glance while
 * a floor plan is being laid out, which a letter in a coloured square does not
 * do once a room is full of props.
 */
static void draw_person(SDL_Renderer *r, float x, float y, float s,
                        SDL_Color body, SDL_Color head)
{
    fx_rect(r, head, x + s * 0.36f, y + s * 0.12f, s * 0.28f, s * 0.24f);
    fx_rect(r, body, x + s * 0.30f, y + s * 0.36f, s * 0.40f, s * 0.40f);
    fx_rect(r, body, x + s * 0.32f, y + s * 0.76f, s * 0.12f, s * 0.20f);
    fx_rect(r, body, x + s * 0.56f, y + s * 0.76f, s * 0.12f, s * 0.20f);
}

static void draw_door(SDL_Renderer *r, float x, float y, float s, SDL_Color c)
{
    fx_rect(r, fx_dim(c, 0.35f), x + s * 0.16f, y + s * 0.06f, s * 0.68f,
            s * 0.94f);
    outline(r, c, x + s * 0.16f, y + s * 0.06f, s * 0.68f, s * 0.94f,
            SDL_max(1.0f, s * 0.06f));
    disc(r, c, x + s * 0.70f, y + s * 0.54f, SDL_max(1.0f, s * 0.05f));
}

void ed_draw_symbol(SDL_Renderer *r, char symbol, float x, float y, float s,
                    float time)
{
    SDL_Color c = ed_symbol_color(symbol);
    SDL_Color dark = fx_dim(c, 0.45f);
    SDL_Color light = fx_mix(c, FX_CREAM, 0.35f);

    switch (symbol)
    {
    case ' ':
    case '.':
        break;

    case '#':
        fx_rect(r, fx_dim(c, 0.55f), x, y, s, s);
        fx_rect(r, fx_dim(c, 0.8f), x, y, s, SDL_max(1.0f, s * 0.09f));
        break;

    case 'H':
        fx_rect(r, c, x + s * 0.22f, y, s * 0.09f, s);
        fx_rect(r, c, x + s * 0.69f, y, s * 0.09f, s);
        for (int i = 0; i < 3; ++i)
        {
            fx_rect(r, light, x + s * 0.22f, y + s * (0.18f + 0.28f * (float)i),
                    s * 0.56f, SDL_max(1.0f, s * 0.08f));
        }
        break;

    case 'V':
        fx_rect(r, fx_dim(c, 0.28f), x + s * 0.10f, y, s * 0.80f, s);
        fx_rect(r, c, x + s * 0.10f, y, s * 0.06f, s);
        fx_rect(r, c, x + s * 0.84f, y, s * 0.06f, s);
        fx_rect(r, light, x + s * 0.16f, y + s * 0.44f, s * 0.68f, s * 0.12f);
        break;

    case 'F':
        fx_rect(r, c, x + s * 0.06f, y + s * 0.10f, s * 0.88f, s * 0.26f);
        fx_rect(r, fx_dim(c, 0.4f), x + s * 0.40f, y + s * 0.10f, s * 0.08f,
                s * 0.26f);
        tri(r, dark, x + s * 0.32f, y + s * 0.52f, x + s * 0.68f, y + s * 0.52f,
            x + s * 0.50f, y + s * 0.82f);
        break;

    case 'P':
        fx_rect(r, c, x + s * 0.06f, y + s * 0.34f, s * 0.88f, s * 0.24f);
        tri(r, light, x + s * 0.04f, y + s * 0.46f, x + s * 0.22f, y + s * 0.28f,
            x + s * 0.22f, y + s * 0.64f);
        tri(r, light, x + s * 0.96f, y + s * 0.46f, x + s * 0.78f, y + s * 0.28f,
            x + s * 0.78f, y + s * 0.64f);
        break;

    case 'S':
        draw_person(r, x, y, s, FX_HERO, FX_SKIN);
        fx_rect(r, FX_HERO_LT, x + s * 0.30f, y + s * 0.36f, s * 0.40f,
                s * 0.10f);
        break;

    case 'E':
        draw_door(r, x, y, s, c);
        break;
    case 'D':
        draw_door(r, x, y, s, c);
        fx_rect(r, light, x + s * 0.30f, y + s * 0.24f, s * 0.20f, s * 0.06f);
        fx_rect(r, light, x + s * 0.30f, y + s * 0.70f, s * 0.20f, s * 0.06f);
        break;
    case 'U':
    case 'R':
        draw_door(r, x, y, s, c);
        if (symbol == 'R')
        {
            tri(r, light, x + s * 0.28f, y + s * 0.50f, x + s * 0.52f,
                y + s * 0.34f, x + s * 0.52f, y + s * 0.66f);
        }
        else
        {
            fx_rect(r, light, x + s * 0.30f, y + s * 0.40f, s * 0.10f, s * 0.22f);
            fx_rect(r, light, x + s * 0.48f, y + s * 0.40f, s * 0.10f, s * 0.22f);
        }
        break;

    case 'Y':
        fx_rect(r, fx_dim(c, 0.25f), x + s * 0.08f, y + s * 0.08f, s * 0.84f,
                s * 0.84f);
        outline(r, c, x + s * 0.08f, y + s * 0.08f, s * 0.84f, s * 0.84f,
                SDL_max(1.0f, s * 0.07f));
        fx_rect(r, c, x + s * 0.46f, y + s * 0.08f, s * 0.08f, s * 0.84f);
        fx_rect(r, c, x + s * 0.08f, y + s * 0.46f, s * 0.84f, s * 0.08f);
        break;

    case 'C':
        fx_rect(r, c, x + s * 0.12f, y + s * 0.28f, s * 0.76f, s * 0.44f);
        fx_rect(r, dark, x + s * 0.12f, y + s * 0.40f, s * 0.76f, s * 0.10f);
        fx_rect(r, light, x + s * 0.62f, y + s * 0.56f, s * 0.20f, s * 0.08f);
        break;

    case 'G':
        fx_rect(r, c, x + s * 0.14f, y + s * 0.36f, s * 0.66f, s * 0.16f);
        fx_rect(r, dark, x + s * 0.30f, y + s * 0.52f, s * 0.18f, s * 0.26f);
        break;

    case 'N':
        disc(r, c, x + s * 0.50f, y + s * 0.58f, s * 0.26f);
        fx_rect(r, dark, x + s * 0.44f, y + s * 0.18f, s * 0.14f, s * 0.18f);
        fx_rect(r, light, x + s * 0.56f, y + s * 0.18f, s * 0.18f, s * 0.06f);
        break;

    case 'K':
        fx_rect(r, FX_CREAM, x + s * 0.14f, y + s * 0.24f, s * 0.72f, s * 0.54f);
        fx_rect(r, c, x + s * 0.44f, y + s * 0.32f, s * 0.14f, s * 0.38f);
        fx_rect(r, c, x + s * 0.26f, y + s * 0.44f, s * 0.50f, s * 0.14f);
        break;

    case 'Z':
        fx_rect(r, dark, x + s * 0.10f, y + s * 0.38f, s * 0.72f, s * 0.20f);
        tri(r, c, x + s * 0.82f, y + s * 0.48f, x + s * 0.60f, y + s * 0.30f,
            x + s * 0.60f, y + s * 0.66f);
        fx_rect(r, light, x + s * 0.16f, y + s * 0.58f, s * 0.16f, s * 0.18f);
        break;

    case 'M':
    case 'W':
        draw_person(r, x, y, s, FX_GUARD, FX_SKIN_DK);
        fx_rect(r, c, x + s * 0.30f, y + s * 0.08f, s * 0.40f, s * 0.08f);
        if (symbol == 'W')
        {
            fx_rect(r, FX_WOOD, x + s * 0.04f, y + s * 0.66f, s * 0.26f,
                    s * 0.16f);
            fx_rect(r, FX_WOOD, x + s * 0.02f, y + s * 0.58f, s * 0.12f,
                    s * 0.12f);
        }
        break;

    case 'J':
        draw_person(r, x, y, s, FX_HERO_DK, FX_SKIN);
        fx_rect(r, FX_STEEL, x + s * 0.70f, y + s * 0.52f, s * 0.26f, s * 0.36f);
        break;
    case 'f':
        draw_person(r, x, y, s, FX_STEEL_LT, FX_SKIN);
        break;
    case 'k':
        draw_person(r, x, y, s, FX_PALE, FX_SKIN);
        fx_rect(r, FX_AMBER_DK, x + s * 0.26f, y + s * 0.72f, s * 0.48f,
                s * 0.10f);
        break;

    case 'X':
        fx_rect(r, dark, x + s * 0.20f, y + s * 0.56f, s * 0.60f, s * 0.28f);
        disc(r, c, x + s * 0.50f, y + s * 0.56f, s * 0.22f);
        fx_rect(r, light, x + s * 0.46f, y + s * 0.24f, s * 0.08f, s * 0.18f);
        break;

    case '^':
        for (int i = 0; i < 3; ++i)
        {
            float base = x + s * (0.08f + 0.30f * (float)i);
            tri(r, c, base, y + s, base + s * 0.28f, y + s, base + s * 0.14f,
                y + s * 0.30f);
        }
        break;

    case 'O':
    {
        float cx = x + s * 0.5f;
        float cy = y + s * 0.5f;
        fx_rect(r, FX_STEEL_DK, cx - s * 0.04f, y, s * 0.08f, s * 0.30f);
        for (int i = 0; i < 3; ++i)
        {
            float angle = time * 6.0f + (float)i * 2.0944f;
            float dx = cosf(angle) * s * 0.44f;
            float dy = sinf(angle) * s * 0.12f;
            tri(r, c, cx, cy, cx + dx, cy + dy - s * 0.06f,
                cx + dx, cy + dy + s * 0.06f);
        }
        disc(r, FX_STEEL, cx, cy, s * 0.09f);
        break;
    }

    case 'L':
        fx_rect(r, c, x + s * 0.30f, y + s * 0.34f, s * 0.40f, s * 0.52f);
        fx_rect(r, dark, x + s * 0.42f, y + s * 0.22f, s * 0.16f, s * 0.14f);
        fx_rect(r, light, x + s * 0.34f, y + s * 0.44f, s * 0.10f, s * 0.30f);
        break;

    case 'B':
        fx_rect(r, dark, x + s * 0.10f, y + s * 0.22f, s * 0.80f, s * 0.72f);
        outline(r, c, x + s * 0.10f, y + s * 0.22f, s * 0.80f, s * 0.72f,
                SDL_max(1.0f, s * 0.07f));
        fx_rect(r, c, x + s * 0.10f, y + s * 0.54f, s * 0.80f, s * 0.07f);
        break;

    case 'T':
        fx_rect(r, FX_STEEL_DK, x + s * 0.14f, y + s * 0.16f, s * 0.72f,
                s * 0.60f);
        fx_rect(r, c, x + s * 0.22f, y + s * 0.24f, s * 0.56f, s * 0.44f);
        fx_rect(r, FX_STEEL, x + s * 0.34f, y + s * 0.76f, s * 0.32f, s * 0.14f);
        break;

    case 'A':
        fx_rect(r, FX_STEEL_DK, x + s * 0.26f, y + s * 0.22f, s * 0.48f,
                s * 0.56f);
        disc(r, c, x + s * 0.50f, y + s * 0.42f, s * 0.13f);
        fx_rect(r, FX_RED, x + s * 0.38f, y + s * 0.58f, s * 0.24f, s * 0.10f);
        break;

    case 'c':
        fx_rect(r, c, x + s * 0.28f, y + s * 0.52f, s * 0.44f, s * 0.10f);
        fx_rect(r, c, x + s * 0.62f, y + s * 0.24f, s * 0.10f, s * 0.32f);
        fx_rect(r, dark, x + s * 0.46f, y + s * 0.62f, s * 0.08f, s * 0.32f);
        break;
    case 'd':
        fx_rect(r, c, x + s * 0.06f, y + s * 0.56f, s * 0.88f, s * 0.10f);
        fx_rect(r, dark, x + s * 0.12f, y + s * 0.66f, s * 0.08f, s * 0.28f);
        fx_rect(r, dark, x + s * 0.80f, y + s * 0.66f, s * 0.08f, s * 0.28f);
        fx_rect(r, FX_CYAN_DK, x + s * 0.40f, y + s * 0.30f, s * 0.32f,
                s * 0.24f);
        break;
    case 'i':
        fx_rect(r, c, x + s * 0.24f, y + s * 0.18f, s * 0.52f, s * 0.76f);
        for (int i = 0; i < 3; ++i)
        {
            fx_rect(r, dark, x + s * 0.30f, y + s * (0.28f + 0.22f * (float)i),
                    s * 0.40f, s * 0.08f);
        }
        break;

    case 'n':
        fx_rect(r, c, x, y + s * 0.42f, s, s * 0.16f);
        fx_rect(r, dark, x + s * 0.08f, y + s * 0.58f, s * 0.84f, s * 0.38f);
        fx_rect(r, FX_AMBER, x + s * 0.10f, y + s * 0.40f, s * 0.80f,
                SDL_max(1.0f, s * 0.05f));
        break;
    case 's':
        fx_rect(r, c, x + s * 0.10f, y + s * 0.58f, s * 0.80f, s * 0.14f);
        fx_rect(r, dark, x + s * 0.12f, y + s * 0.72f, s * 0.10f, s * 0.22f);
        fx_rect(r, dark, x + s * 0.78f, y + s * 0.72f, s * 0.10f, s * 0.22f);
        break;
    case 't':
        fx_rect(r, FX_STEEL_DK, x + s * 0.34f, y + s * 0.64f, s * 0.32f,
                s * 0.32f);
        tri(r, c, x + s * 0.50f, y + s * 0.10f, x + s * 0.14f, y + s * 0.62f,
            x + s * 0.86f, y + s * 0.62f);
        break;
    case 'g':
        fx_rect(r, FX_STEEL_DK, x + s * 0.12f, y + s * 0.40f, s * 0.16f,
                s * 0.56f);
        fx_rect(r, FX_STEEL_DK, x + s * 0.72f, y + s * 0.40f, s * 0.16f,
                s * 0.56f);
        fx_rect(r, c, x + s * 0.28f, y + s * 0.58f, s * 0.44f,
                SDL_max(1.0f, s * 0.06f));
        break;

    case 'q':
        fx_rect(r, c, x + s * 0.30f, y + s * 0.46f, s * 0.40f, s * 0.30f);
        fx_rect(r, dark, x + s * 0.34f, y + s * 0.20f, s * 0.32f, s * 0.26f);
        fx_rect(r, c, x + s * 0.26f, y + s * 0.76f, s * 0.48f, s * 0.14f);
        break;
    case 'b':
        fx_rect(r, c, x + s * 0.18f, y + s * 0.48f, s * 0.64f, s * 0.18f);
        fx_rect(r, dark, x + s * 0.44f, y + s * 0.66f, s * 0.12f, s * 0.24f);
        fx_rect(r, fx_dim(c, 0.6f), x + s * 0.26f, y + s * 0.12f, s * 0.48f,
                s * 0.28f);
        break;
    case 'u':
        fx_rect(r, c, x + s * 0.32f, y + s * 0.24f, s * 0.36f, s * 0.44f);
        fx_rect(r, dark, x + s * 0.38f, y + s * 0.68f, s * 0.24f, s * 0.14f);
        break;
    case 'p':
        fx_rect(r, c, x + s * 0.44f, y + s * 0.06f, s * 0.12f, s * 0.94f);
        break;
    case 'o':
        outline(r, c, x + s * 0.10f, y + s * 0.06f, s * 0.80f, s * 0.94f,
                SDL_max(1.0f, s * 0.07f));
        fx_rect(r, dark, x + s * 0.38f, y + s * 0.52f, s * 0.24f, s * 0.30f);
        break;
    case 'z':
        fx_rect(r, fx_dim(c, 0.6f), x + s * 0.10f, y + s * 0.06f, s * 0.80f,
                s * 0.94f);
        outline(r, c, x + s * 0.10f, y + s * 0.06f, s * 0.80f, s * 0.94f,
                SDL_max(1.0f, s * 0.07f));
        disc(r, light, x + s * 0.74f, y + s * 0.52f, SDL_max(1.0f, s * 0.05f));
        break;

    case 'r':
        fx_rect(r, fx_dim(c, 0.3f), x + s * 0.10f, y + s * 0.10f, s * 0.80f,
                s * 0.80f);
        outline(r, c, x + s * 0.10f, y + s * 0.10f, s * 0.80f, s * 0.80f,
                SDL_max(1.0f, s * 0.07f));
        disc(r, light, x + s * 0.62f, y + s * 0.36f, s * 0.10f);
        break;
    case 'v':
        tri(r, c, x + s * 0.06f, y + s * 0.30f, x + s * 0.50f, y + s * 0.56f,
            x + s * 0.48f, y + s * 0.34f);
        tri(r, c, x + s * 0.94f, y + s * 0.30f, x + s * 0.50f, y + s * 0.56f,
            x + s * 0.52f, y + s * 0.34f);
        break;

    default:
        fx_rect(r, FX_RED_DK, x + s * 0.14f, y + s * 0.14f, s * 0.72f,
                s * 0.72f);
        outline(r, FX_RED, x + s * 0.14f, y + s * 0.14f, s * 0.72f, s * 0.72f,
                SDL_max(1.0f, s * 0.08f));
        break;
    }
}

/* ---- The canvas -------------------------------------------------------- */

static bool ensure_texture(EditorApp *app, int w, int h)
{
    if (app->canvas_texture != NULL && app->canvas_texture_w == w &&
        app->canvas_texture_h == h)
    {
        return true;
    }
    if (app->canvas_texture != NULL)
        SDL_DestroyTexture(app->canvas_texture);
    app->canvas_texture =
        SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_TARGET, w, h);
    if (app->canvas_texture == NULL)
        return false;
    SDL_SetTextureScaleMode(app->canvas_texture, SDL_SCALEMODE_NEAREST);
    app->canvas_texture_w = w;
    app->canvas_texture_h = h;
    return true;
}

static void draw_route_overlay(EditorApp *app, float ox, float oy)
{
    const EdReport *report = &app->report;
    if (!report->route_valid)
        return;

    for (int row = 0; row < app->doc.grid.height; ++row)
    {
        for (int col = 0; col < app->doc.grid.width; ++col)
        {
            if (!report->route.seen[row][col])
                continue;
            float x = ox + (float)col * TILE_SIZE;
            float y = oy + (float)row * TILE_SIZE;
            bool stranded = !report->route.escapes[row][col];
            fx_rect_a(app->renderer, stranded ? FX_RED : FX_GREEN,
                      stranded ? 96 : 46, x + 2.0f, y + 2.0f, TILE_SIZE - 4.0f,
                      TILE_SIZE - 4.0f);
        }
    }

    float sx = ox + (float)report->start.col * TILE_SIZE;
    float sy = oy + (float)report->start.row * TILE_SIZE;
    outline(app->renderer, FX_CYAN, sx, sy, TILE_SIZE, TILE_SIZE, 2.0f);
    float gx = ox + (float)report->goal.col * TILE_SIZE;
    float gy = oy + (float)report->goal.row * TILE_SIZE;
    outline(app->renderer, report->goal_reached ? FX_GREEN : FX_RED, gx, gy,
            TILE_SIZE, TILE_SIZE, 2.0f);
}

static void draw_door_pairs(EditorApp *app, float ox, float oy)
{
    const LevelMap *map = &app->level.map;
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i + 1 < map->door_count; i += 2)
    {
        float ax = ox + (map->doors[i].col + 0.5f) * TILE_SIZE;
        float ay = oy + (map->doors[i].row + 0.5f) * TILE_SIZE;
        float bx = ox + (map->doors[i + 1].col + 0.5f) * TILE_SIZE;
        float by = oy + (map->doors[i + 1].row + 0.5f) * TILE_SIZE;
        SDL_SetRenderDrawColor(app->renderer, FX_AMBER.r, FX_AMBER.g,
                               FX_AMBER.b, 130);
        SDL_RenderLine(app->renderer, ax, ay, bx, by);
    }
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_NONE);
}

static void draw_preview_shape(EditorApp *app, float ox, float oy)
{
    if (!app->dragging)
        return;

    int c0 = SDL_min(app->drag_col, app->cursor_col);
    int c1 = SDL_max(app->drag_col, app->cursor_col);
    int r0 = SDL_min(app->drag_row, app->cursor_row);
    int r1 = SDL_max(app->drag_row, app->cursor_row);
    float x = ox + (float)c0 * TILE_SIZE;
    float y = oy + (float)r0 * TILE_SIZE;
    float w = (float)(c1 - c0 + 1) * TILE_SIZE;
    float h = (float)(r1 - r0 + 1) * TILE_SIZE;
    outline(app->renderer, FX_CYAN, x, y, w, h, 2.0f);
}

void ed_draw_canvas(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    int view_w = (int)ceilf(app->canvas.w / app->zoom);
    int view_h = (int)ceilf(app->canvas.h / app->zoom);
    if (view_w < 16)
        view_w = 16;
    if (view_h < 16)
        view_h = 16;
    if (!ensure_texture(app, view_w, view_h + HUD_HEIGHT))
        return;

    SDL_SetRenderTarget(r, app->canvas_texture);
    fx_rect(r, FX_INK, 0.0f, 0.0f, (float)view_w,
            (float)(view_h + HUD_HEIGHT));

    /* The game draws the world under a 40px HUD, and the backdrop's parallax
     * is composed around that. Reproducing the offset and cropping it off
     * again is what keeps the editor's picture the game's picture. */
    float ox = -app->cam_x;
    float oy = HUD_HEIGHT - app->cam_y;

    if (app->art_mode && app->level_ok)
    {
        LevelArtScene scene = {r,
                               &app->level,
                               app->cam_x,
                               app->cam_y,
                               view_w,
                               view_h + HUD_HEIGHT,
                               app->time,
                               app->current_file >= 0
                                   ? app->files[app->current_file].number - 1
                                   : 0};
        level_art_backdrop(&scene);
    }
    else
    {
        fx_rect(r, FX_NIGHT, 0.0f, 0.0f, (float)view_w,
                (float)(view_h + HUD_HEIGHT));
    }

    int first_col = (int)floorf(app->cam_x / TILE_SIZE) - 1;
    int last_col = (int)ceilf((app->cam_x + (float)view_w) / TILE_SIZE) + 1;
    int first_row = (int)floorf((app->cam_y - HUD_HEIGHT) / TILE_SIZE) - 1;
    int last_row = (int)ceilf((app->cam_y + (float)view_h) / TILE_SIZE) + 1;
    if (first_col < 0)
        first_col = 0;
    if (first_row < 0)
        first_row = 0;
    if (last_col > app->doc.grid.width)
        last_col = app->doc.grid.width;
    if (last_row > app->doc.grid.height)
        last_row = app->doc.grid.height;

    /* Everything outside the map, so its edges are unmistakable. */
    fx_rect_a(r, FX_INK, 200, ox - 4096.0f, oy - 4096.0f, 4096.0f, 8192.0f);
    fx_rect_a(r, FX_INK, 200, ox + (float)app->doc.grid.width * TILE_SIZE,
              oy - 4096.0f, 4096.0f, 8192.0f);
    fx_rect_a(r, FX_INK, 200, ox, oy - 4096.0f,
              (float)app->doc.grid.width * TILE_SIZE, 4096.0f);
    fx_rect_a(r, FX_INK, 200, ox, oy + (float)app->doc.grid.height * TILE_SIZE,
              (float)app->doc.grid.width * TILE_SIZE, 4096.0f);

    if (app->doc.grid.facade)
    {
        /* The outer 80px on each side is behind the inset. */
        fx_rect_a(r, FX_RED, 30, ox, oy, FACADE_BUILDING_SIDE_INSET,
                  (float)app->doc.grid.height * TILE_SIZE);
        fx_rect_a(r, FX_RED, 30,
                  ox + (float)app->doc.grid.width * TILE_SIZE -
                      FACADE_BUILDING_SIDE_INSET,
                  oy, FACADE_BUILDING_SIDE_INSET,
                  (float)app->doc.grid.height * TILE_SIZE);
    }

    for (int row = first_row; row < last_row; ++row)
    {
        for (int col = first_col; col < last_col; ++col)
        {
            char cell = editor_doc_get(&app->doc, col, row);
            float x = ox + (float)col * TILE_SIZE;
            float y = oy + (float)row * TILE_SIZE;
            if (cell == '#')
            {
                if (app->art_mode && app->level_ok)
                    level_art_wall_tile(r, &app->level, col, row, x, y);
                else
                    ed_draw_symbol(r, '#', x, y, TILE_SIZE, app->time);
                continue;
            }
            if (!app->art_mode)
            {
                fx_rect_a(r, cell == '.' ? FX_INK : FX_SHADOW, 150, x, y,
                          TILE_SIZE, TILE_SIZE);
            }
            ed_draw_symbol(r, cell, x, y, TILE_SIZE, app->time);
        }
    }

    if (app->show_grid)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int col = 0; col <= app->doc.grid.width; ++col)
        {
            Uint8 alpha = col % 4 == 0 ? 70 : 30;
            SDL_SetRenderDrawColor(r, 150, 180, 200, alpha);
            float x = ox + (float)col * TILE_SIZE;
            SDL_RenderLine(r, x, oy, x,
                           oy + (float)app->doc.grid.height * TILE_SIZE);
        }
        for (int row = 0; row <= app->doc.grid.height; ++row)
        {
            Uint8 alpha = row % 4 == 0 ? 70 : 30;
            SDL_SetRenderDrawColor(r, 150, 180, 200, alpha);
            float y = oy + (float)row * TILE_SIZE;
            SDL_RenderLine(r, ox, y,
                           ox + (float)app->doc.grid.width * TILE_SIZE, y);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    if (app->show_route)
        draw_route_overlay(app, ox, oy);
    if (app->level_ok)
        draw_door_pairs(app, ox, oy);

    if (app->has_selection)
    {
        float x = ox + (float)SDL_min(app->sel_col0, app->sel_col1) * TILE_SIZE;
        float y = oy + (float)SDL_min(app->sel_row0, app->sel_row1) * TILE_SIZE;
        float w = (float)(SDL_abs(app->sel_col1 - app->sel_col0) + 1) * TILE_SIZE;
        float h = (float)(SDL_abs(app->sel_row1 - app->sel_row0) + 1) * TILE_SIZE;
        fx_rect_a(r, FX_CYAN, 26, x, y, w, h);
        outline(r, FX_CYAN, x, y, w, h, 2.0f);
    }

    draw_preview_shape(app, ox, oy);

    /* The tile under the pointer, with the brush ghosted into it. */
    if (app->cursor_col >= 0 && app->cursor_row >= 0)
    {
        float x = ox + (float)app->cursor_col * TILE_SIZE;
        float y = oy + (float)app->cursor_row * TILE_SIZE;
        if (app->tool != ED_TOOL_SELECT && app->tool != ED_TOOL_PICK)
        {
            fx_rect_a(r, FX_CREAM, 24, x, y, TILE_SIZE, TILE_SIZE);
            ed_draw_symbol(r, app->brush, x, y, TILE_SIZE, app->time);
        }
        outline(r, FX_CREAM, x, y, TILE_SIZE, TILE_SIZE, 1.0f);
    }

    SDL_SetRenderTarget(r, NULL);
    SDL_FRect src = {0.0f, (float)HUD_HEIGHT, (float)view_w, (float)view_h};
    SDL_RenderTexture(r, app->canvas_texture, &src, &app->canvas);
}
