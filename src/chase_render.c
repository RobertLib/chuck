#include "chase_render.h"

#include "fx.h"

#include <math.h>

/*
 * The pursuit is drawn from above, so this file is a small top-down companion
 * to game_render.c: same procedural, no-assets approach, same fx.h palette and
 * lighting vocabulary, just a different camera. Nothing here reads or writes
 * simulation state; every animation is derived from `Chase`.
 */

/* Road space is drawn 1:1, so the visible strip of road is one view height. */
typedef struct
{
    float road_left;
    float view_top;
    float view_h;
    float camera_y;
    float shake_x;
    float shake_y;
} ChaseView;

static const SDL_Color COL_ASPHALT = {26, 30, 38, 255};
static const SDL_Color COL_ASPHALT_LT = {35, 40, 49, 255};
static const SDL_Color COL_PAVEMENT = {46, 52, 62, 255};
static const SDL_Color COL_KERB = {96, 104, 114, 255};
static const SDL_Color COL_PAINT = {198, 204, 194, 255};
static const SDL_Color COL_PAINT_MID = {150, 146, 96, 255};
static const SDL_Color COL_GLASS = {11, 20, 28, 255};

typedef struct
{
    SDL_Color body;
    SDL_Color body_lt;
    SDL_Color roof;
} CarPaint;

static const CarPaint TRAFFIC_PAINT[4] = {
    {{118, 60, 54, 255}, {152, 86, 74, 255}, {78, 40, 38, 255}},
    {{56, 74, 92, 255}, {86, 108, 126, 255}, {36, 50, 64, 255}},
    {{94, 96, 86, 255}, {128, 130, 116, 255}, {60, 64, 58, 255}},
    {{138, 130, 96, 255}, {174, 166, 124, 255}, {94, 88, 66, 255}}};

static const CarPaint PLAYER_PAINT = {
    {40, 108, 148, 255}, {70, 156, 180, 255}, {24, 66, 96, 255}};

static const CarPaint TARGET_PAINT = {
    {38, 44, 46, 255}, {62, 68, 66, 255}, {24, 28, 30, 255}};

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static float screen_x(const ChaseView *view, float x)
{
    return view->road_left + x + view->shake_x;
}

static float screen_y(const ChaseView *view, float y)
{
    return view->view_top + view->view_h - (y - view->camera_y) + view->shake_y;
}

static void draw_text(SDL_Renderer *r, float x, float y, float scale,
                      SDL_Color color, const char *text)
{
    SDL_SetRenderScale(r, scale, scale);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 255);
    SDL_RenderDebugText(r, x / scale, y / scale, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static float text_width(const char *text, float scale)
{
    return (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

static void draw_text_centered(SDL_Renderer *r, float center_x, float y,
                               float scale, SDL_Color color, const char *text)
{
    draw_text(r, center_x - text_width(text, scale) * 0.5f, y, scale, color, text);
}

/* ---- Cars ------------------------------------------------------------ */

/*
 * Every car is drawn in its own frame: `along` runs from tail (-0.5) to nose
 * (+0.5) and `across` from the left flank (-0.5) to the right (+0.5). The
 * frame's nose vector is axis-aligned, so one set of artwork serves the lanes
 * and the cross streets without any rotation maths.
 */
typedef struct
{
    float cx, cy;
    float ax, ay;
    float length;
    float width;
} CarFrame;

static void car_part(SDL_Renderer *r, const CarFrame *f,
                     float a0, float a1, float b0, float b1, SDL_Color color)
{
    float rx = -f->ay;
    float ry = f->ax;
    float x0 = f->cx + f->ax * a0 * f->length + rx * b0 * f->width;
    float y0 = f->cy + f->ay * a0 * f->length + ry * b0 * f->width;
    float x1 = f->cx + f->ax * a1 * f->length + rx * b1 * f->width;
    float y1 = f->cy + f->ay * a1 * f->length + ry * b1 * f->width;
    float x = fminf(x0, x1);
    float y = fminf(y0, y1);
    float w = fabsf(x1 - x0);
    float h = fabsf(y1 - y0);
    fx_rect(r, color, floorf(x), floorf(y),
            fmaxf(1.0f, ceilf(w)), fmaxf(1.0f, ceilf(h)));
}

static void draw_head_beam(SDL_Renderer *r, const CarFrame *f, float length,
                           float spread, SDL_Color color, float alpha)
{
    float rx = -f->ay;
    float ry = f->ax;
    float nose_x = f->cx + f->ax * f->length * 0.5f;
    float nose_y = f->cy + f->ay * f->length * 0.5f;
    float half = f->width * 0.34f;
    SDL_FColor near_color = fx_fcolor(color, alpha);
    SDL_FColor far_color = fx_fcolor(color, 0.0f);
    SDL_Vertex v[4] = {
        {{nose_x - rx * half, nose_y - ry * half}, near_color, {0.0f, 0.0f}},
        {{nose_x + rx * half, nose_y + ry * half}, near_color, {0.0f, 0.0f}},
        {{nose_x + f->ax * length + rx * spread,
          nose_y + f->ay * length + ry * spread},
         far_color,
         {0.0f, 0.0f}},
        {{nose_x + f->ax * length - rx * spread,
          nose_y + f->ay * length - ry * spread},
         far_color,
         {0.0f, 0.0f}}};
    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, NULL, v, 4, indices, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_car_body(SDL_Renderer *r, const CarFrame *f,
                          const CarPaint *paint, bool lights_on,
                          bool braking, float wreck)
{
    SDL_Color body = paint->body;
    SDL_Color body_lt = paint->body_lt;
    SDL_Color roof = paint->roof;
    if (wreck > 0.0f)
    {
        /* Scorched panels, and less of the crisp highlight. */
        float burn = clamp01(wreck * 0.9f);
        body = fx_mix(body, (SDL_Color){44, 40, 40, 255}, burn);
        body_lt = fx_mix(body_lt, (SDL_Color){62, 56, 54, 255}, burn);
        roof = fx_mix(roof, (SDL_Color){30, 28, 28, 255}, burn);
    }

    /* Contact shadow: the same box nudged down-right, so cars sit on the road. */
    CarFrame shadow = *f;
    shadow.cx += 3.0f;
    shadow.cy += 4.0f;
    car_part(r, &shadow, -0.5f, 0.5f, -0.5f, 0.5f, (SDL_Color){6, 8, 12, 255});

    car_part(r, f, -0.5f, 0.5f, -0.5f, 0.5f, FX_INK);
    car_part(r, f, -0.47f, 0.47f, -0.44f, 0.44f, body);
    car_part(r, f, -0.47f, 0.47f, -0.44f, -0.33f, body_lt);
    car_part(r, f, -0.30f, 0.12f, -0.37f, 0.37f, roof);
    car_part(r, f, 0.12f, 0.27f, -0.33f, 0.33f, COL_GLASS);
    car_part(r, f, -0.42f, -0.30f, -0.30f, 0.30f, COL_GLASS);
    car_part(r, f, 0.27f, 0.43f, -0.40f, 0.40f, body_lt);

    /* Wheels sit proud of the flanks, which reads as a car from above. */
    SDL_Color tyre = {14, 16, 20, 255};
    car_part(r, f, -0.36f, -0.19f, -0.55f, -0.44f, tyre);
    car_part(r, f, -0.36f, -0.19f, 0.44f, 0.55f, tyre);
    car_part(r, f, 0.19f, 0.36f, -0.55f, -0.44f, tyre);
    car_part(r, f, 0.19f, 0.36f, 0.44f, 0.55f, tyre);

    if (lights_on && wreck <= 0.0f)
    {
        car_part(r, f, 0.43f, 0.50f, -0.38f, -0.20f, FX_CREAM);
        car_part(r, f, 0.43f, 0.50f, 0.20f, 0.38f, FX_CREAM);
    }
    SDL_Color tail = braking ? (SDL_Color){255, 96, 76, 255} : FX_RED_DK;
    if (wreck <= 0.0f)
    {
        car_part(r, f, -0.50f, -0.44f, -0.40f, -0.18f, tail);
        car_part(r, f, -0.50f, -0.44f, 0.18f, 0.40f, tail);
    }
}

static void draw_wreck_debris(SDL_Renderer *r, const CarFrame *f, float wreck)
{
    /* A short burst of glass and sparks, then just a dead car in the road. */
    if (wreck > 0.7f)
        return;
    float fade = 1.0f - wreck / 0.7f;
    for (unsigned i = 0; i < 10u; ++i)
    {
        unsigned h = fx_hash(i * 2246822519u + (unsigned)(f->cx * 3.0f));
        float angle = (float)(h % 628u) * 0.01f;
        float distance = 8.0f + (float)((h >> 9) % 34u) * wreck * 3.0f;
        SDL_Color spark = (i & 1u) ? FX_AMBER : (SDL_Color){206, 216, 220, 255};
        fx_rect_a(r, spark, (Uint8)(fade * 210.0f),
                  f->cx + cosf(angle) * distance,
                  f->cy + sinf(angle) * distance, 2.0f, 2.0f);
    }
    fx_glow(r, f->cx, f->cy, 30.0f + wreck * 26.0f, FX_AMBER,
            (Uint8)(fade * 120.0f));
}

static void draw_traffic_car(SDL_Renderer *r, const ChaseView *view,
                             const ChaseCar *car)
{
    CarFrame frame;
    frame.cx = screen_x(view, car->x);
    frame.cy = screen_y(view, car->y);
    frame.length = CHASE_CAR_LENGTH;
    frame.width = CHASE_CAR_WIDTH;
    if (car->kind == CHASE_CAR_CROSSING)
    {
        frame.ax = car->vx >= 0.0f ? 1.0f : -1.0f;
        frame.ay = 0.0f;
    }
    else
    {
        frame.ax = 0.0f;
        frame.ay = car->kind == CHASE_CAR_ONCOMING ? 1.0f : -1.0f;
    }

    const CarPaint *paint = &TRAFFIC_PAINT[car->variant & 3];
    bool oncoming = car->kind != CHASE_CAR_TRAFFIC;
    if (oncoming && car->wreck_time <= 0.0f)
    {
        draw_head_beam(r, &frame, 132.0f, CHASE_CAR_WIDTH * 0.85f,
                       FX_CREAM, 0.20f);
    }
    draw_car_body(r, &frame, paint, oncoming, false, car->wreck_time);
    if (car->wreck_time > 0.0f)
        draw_wreck_debris(r, &frame, car->wreck_time);
}

static void draw_target_car(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase)
{
    CarFrame frame;
    frame.cx = screen_x(view, chase->target.x);
    frame.cy = screen_y(view, chase->target.y);
    frame.ax = 0.0f;
    frame.ay = -1.0f;
    frame.length = CHASE_SUV_LENGTH;
    frame.width = CHASE_SUV_WIDTH;

    bool braking = chase->phase == CHASE_PHASE_ARRIVAL;
    draw_head_beam(r, &frame, 150.0f, CHASE_SUV_WIDTH, FX_CREAM, 0.16f);
    draw_car_body(r, &frame, &TARGET_PAINT, true, braking, 0.0f);
    /* Roof rack and a spare on the back: the SUV has to be unmistakable. */
    car_part(r, &frame, -0.22f, 0.06f, -0.30f, 0.30f, (SDL_Color){52, 58, 56, 255});
    car_part(r, &frame, -0.22f, 0.06f, -0.30f, -0.24f, (SDL_Color){78, 84, 80, 255});
    car_part(r, &frame, -0.56f, -0.50f, -0.22f, 0.22f, (SDL_Color){28, 30, 32, 255});

    /* Pursuit bracket, borrowed from the cutscene's target framing. */
    float pulse = 0.5f + 0.5f * sinf(chase->time * 5.0f);
    SDL_Color mark = fx_dim(FX_CYAN, 0.55f + pulse * 0.45f);
    float half_w = CHASE_SUV_WIDTH * 0.5f + 9.0f;
    float half_h = CHASE_SUV_LENGTH * 0.5f + 9.0f;
    float arm = 12.0f;
    float left = frame.cx - half_w;
    float right = frame.cx + half_w;
    float top = frame.cy - half_h;
    float bottom = frame.cy + half_h;
    fx_rect(r, mark, left, top, arm, 2.0f);
    fx_rect(r, mark, left, top, 2.0f, arm);
    fx_rect(r, mark, right - arm, top, arm, 2.0f);
    fx_rect(r, mark, right - 2.0f, top, 2.0f, arm);
    fx_rect(r, mark, left, bottom - 2.0f, arm, 2.0f);
    fx_rect(r, mark, left, bottom - arm, 2.0f, arm);
    fx_rect(r, mark, right - arm, bottom - 2.0f, arm, 2.0f);
    fx_rect(r, mark, right - 2.0f, bottom - arm, 2.0f, arm);
}

static void draw_player_car(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase)
{
    const ChasePlayerCar *car = &chase->player;
    CarFrame frame;
    frame.cx = screen_x(view, car->x);
    frame.cy = screen_y(view, car->y);
    frame.ax = 0.0f;
    frame.ay = -1.0f;
    frame.length = CHASE_CAR_LENGTH;
    frame.width = CHASE_CAR_WIDTH;

    if (car->engine_running)
        draw_head_beam(r, &frame, 190.0f, CHASE_CAR_WIDTH * 1.1f, FX_CREAM, 0.22f);

    CarPaint paint = PLAYER_PAINT;
    /* Damage is legible on the car itself, not only in the HUD. */
    float wear = 1.0f - (float)car->integrity / (float)CHASE_INTEGRITY;
    if (wear > 0.0f)
    {
        paint.body = fx_mix(paint.body, (SDL_Color){60, 58, 58, 255}, wear * 0.55f);
        paint.body_lt = fx_mix(paint.body_lt, (SDL_Color){86, 84, 80, 255}, wear * 0.55f);
    }
    draw_car_body(r, &frame, &paint, car->engine_running, false, 0.0f);

    if (car->invuln_timer > 0.0f && fmodf(car->invuln_timer, 0.24f) > 0.12f)
    {
        fx_rect_a(r, FX_CREAM, 90,
                  frame.cx - CHASE_CAR_WIDTH * 0.5f,
                  frame.cy - CHASE_CAR_LENGTH * 0.5f,
                  CHASE_CAR_WIDTH, CHASE_CAR_LENGTH);
    }
    if (car->scrape_timer > 0.0f)
    {
        float side = car->x < CHASE_ROAD_WIDTH * 0.5f ? -1.0f : 1.0f;
        for (int i = 0; i < 4; ++i)
        {
            fx_rect_a(r, FX_AMBER, (Uint8)(150 - i * 30),
                      frame.cx + side * CHASE_CAR_WIDTH * 0.5f,
                      frame.cy + (float)i * 7.0f, 3.0f, 5.0f);
        }
    }
}

/* ---- Chuck on foot, during the opening beat -------------------------- */

static void draw_chuck_on_foot(SDL_Renderer *r, const ChaseView *view,
                               const Chase *chase)
{
    const float run_start = CHASE_DEPARTURE_CHUCK_RUN;
    const float run_end = CHASE_DEPARTURE_CAR_DOOR;
    if (chase->phase != CHASE_PHASE_DEPARTURE ||
        chase->phase_time > run_end + 0.05f)
    {
        return;
    }

    float progress = clamp01((chase->phase_time - run_start) /
                             (run_end - run_start));
    /*
     * He runs up the pavement past his parked car, then cuts around its nose to
     * the driver's door. Two straight legs are enough to read as "he gets in"
     * at this scale, and they keep him clear of his own bodywork.
     */
    const float pavement_x = CHASE_ROAD_WIDTH + CHASE_PAVEMENT_WIDTH * 0.5f;
    float door_x = chase->player.x - CHASE_CAR_WIDTH * 0.5f - 11.0f;
    float road_x = pavement_x;
    float road_y = chase->player.y - 86.0f;
    if (progress < 0.62f)
    {
        float leg = progress / 0.62f;
        road_y = chase->player.y - 86.0f + 138.0f * leg;
    }
    else
    {
        /* Straight across the front of the car, never over its bodywork. */
        float leg = (progress - 0.62f) / 0.38f;
        road_x = pavement_x + (door_x - pavement_x) * leg;
        road_y = chase->player.y + 52.0f;
    }
    float x = screen_x(view, road_x);
    float y = screen_y(view, road_y);
    bool running = progress > 0.0f && progress < 1.0f;
    float stride = running ? sinf(chase->phase_time * 15.0f) : 0.0f;

    /* A pool of light under him so a 16-pixel figure still reads at night. */
    fx_glow(r, x, y, 34.0f, FX_AMBER, 44);
    fx_rect_a(r, FX_INK, 140, x - 6.0f, y + 7.0f, 14.0f, 5.0f);
    /* Seen from above: shoulders, jacket, head, and swinging arms. */
    fx_rect(r, FX_HERO_DK, x - 8.0f, y - 8.0f, 16.0f, 16.0f);
    fx_rect(r, FX_HERO, x - 7.0f, y - 7.0f, 14.0f, 13.0f);
    fx_rect(r, FX_HERO_LT, x - 7.0f, y - 7.0f, 14.0f, 4.0f);
    fx_rect(r, FX_HERO_DK, x - 12.0f, y - 4.0f + stride * 3.0f, 5.0f, 7.0f);
    fx_rect(r, FX_HERO_DK, x + 7.0f, y - 4.0f - stride * 3.0f, 5.0f, 7.0f);
    fx_rect(r, FX_HAIR, x - 5.0f, y - 4.0f, 10.0f, 9.0f);
    fx_rect(r, FX_SKIN, x - 4.0f, y - 3.0f, 8.0f, 5.0f);
}

/* ---- Street ---------------------------------------------------------- */

static bool inside_junction(const Chase *chase, float y, float margin)
{
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        if (fabsf(junction->y - y) < CHASE_JUNCTION_HALF + margin)
            return true;
    }
    return false;
}

static void draw_rooftop_block(SDL_Renderer *r, float x, float y,
                               float w, float h, unsigned seed)
{
    SDL_Color roof = fx_mix(FX_BASE, FX_STEEL_DK, (float)(seed % 100u) * 0.01f);
    fx_rect(r, FX_NIGHT, x, y, w, h);
    fx_rect(r, roof, x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f);
    /* A parapet edge catches the city glow and separates roof from street. */
    fx_rect(r, fx_dim(roof, 1.45f), x + 3.0f, y + 3.0f, w - 6.0f, 2.0f);
    fx_rect(r, FX_SHADOW, x + 3.0f, y + h - 6.0f, w - 6.0f, 3.0f);

    if ((seed & 3u) == 0u)
    {
        fx_rect(r, FX_STEEL_DK, x + 14.0f, y + 16.0f, 26.0f, 18.0f);
        fx_rect(r, FX_STEEL, x + 16.0f, y + 18.0f, 22.0f, 5.0f);
    }
    if ((seed & 7u) == 3u)
    {
        fx_rect(r, FX_STEEL_DK, x + w - 44.0f, y + 22.0f, 20.0f, 20.0f);
        fx_rect(r, FX_MID, x + w - 41.0f, y + 25.0f, 14.0f, 14.0f);
    }
    if ((seed & 5u) == 1u)
    {
        SDL_Color lit = (seed & 8u) ? FX_AMBER_DK : FX_CYAN_DK;
        fx_rect(r, lit, x + 20.0f, y + h - 26.0f, 32.0f, 6.0f);
        fx_glow(r, x + 36.0f, y + h - 23.0f, 26.0f, lit, 60);
    }
}

static void render_blocks(SDL_Renderer *r, const ChaseView *view, int win_w)
{
    const float block = 220.0f;
    float road_right = view->road_left + CHASE_ROAD_WIDTH;
    float first = floorf((view->camera_y - block) / block);
    float last = ceilf((view->camera_y + view->view_h + block) / block);

    for (float index = first; index <= last; index += 1.0f)
    {
        float world_y = index * block;
        float top = screen_y(view, world_y + block);
        float height = block - 8.0f;
        unsigned seed = fx_hash((unsigned)(int)index * 2654435761u);
        draw_rooftop_block(r, -20.0f, top,
                           view->road_left - CHASE_PAVEMENT_WIDTH + 20.0f,
                           height, seed);
        draw_rooftop_block(r, road_right + CHASE_PAVEMENT_WIDTH, top,
                           (float)win_w - road_right - CHASE_PAVEMENT_WIDTH +
                               20.0f,
                           height, fx_hash(seed + 77u));
    }

    /* Pavements run continuously between the blocks and the kerb. */
    fx_rect(r, COL_PAVEMENT, view->road_left - CHASE_PAVEMENT_WIDTH,
            view->view_top, CHASE_PAVEMENT_WIDTH, view->view_h);
    fx_rect(r, COL_PAVEMENT, road_right, view->view_top, CHASE_PAVEMENT_WIDTH,
            view->view_h);
}

static void render_road(SDL_Renderer *r, const ChaseView *view,
                        const Chase *chase)
{
    float road_right = view->road_left + CHASE_ROAD_WIDTH;
    fx_rect(r, COL_ASPHALT, view->road_left, view->view_top, CHASE_ROAD_WIDTH,
            view->view_h);

    /* Repair patches keep a big flat surface from looking like a flat surface. */
    float patch_span = 160.0f;
    float first = floorf(view->camera_y / patch_span);
    float last = ceilf((view->camera_y + view->view_h) / patch_span);
    for (float index = first; index <= last; index += 1.0f)
    {
        unsigned seed = fx_hash((unsigned)(int)index * 40503u + 17u);
        if ((seed & 3u) != 0u)
            continue;
        float x = view->road_left + (float)(seed % (unsigned)CHASE_ROAD_WIDTH);
        float y = screen_y(view, index * patch_span);
        float w = 40.0f + (float)((seed >> 7) % 70u);
        if (x + w > road_right)
            w = road_right - x;
        fx_rect(r, COL_ASPHALT_LT, x, y, w, 20.0f + (float)((seed >> 3) % 26u));
    }

    /* Lane paint: dashes between same-direction lanes, a double centre line. */
    const float dash_span = 96.0f;
    float dash_first = floorf(view->camera_y / dash_span) - 1.0f;
    float dash_last = ceilf((view->camera_y + view->view_h) / dash_span);
    for (int lane = 1; lane < CHASE_LANE_COUNT; ++lane)
    {
        float x = view->road_left + (float)lane * CHASE_LANE_WIDTH;
        if (lane == CHASE_FIRST_FORWARD_LANE)
            continue;
        for (float index = dash_first; index <= dash_last; index += 1.0f)
        {
            float world_y = index * dash_span;
            if (inside_junction(chase, world_y, 0.0f))
                continue;
            fx_rect(r, COL_PAINT, x - 1.5f, screen_y(view, world_y + 52.0f),
                    3.0f, 52.0f);
        }
    }
    float center = view->road_left +
                   (float)CHASE_FIRST_FORWARD_LANE * CHASE_LANE_WIDTH;
    fx_rect(r, COL_PAINT_MID, center - 4.0f, view->view_top, 3.0f, view->view_h);
    fx_rect(r, COL_PAINT_MID, center + 1.0f, view->view_top, 3.0f, view->view_h);

    /* Kerbs, with the edge line drivers are supposed to respect. */
    fx_rect(r, COL_KERB, view->road_left - 3.0f, view->view_top, 3.0f,
            view->view_h);
    fx_rect(r, COL_KERB, road_right, view->view_top, 3.0f, view->view_h);
    fx_rect(r, fx_dim(COL_PAINT, 0.72f), view->road_left + 6.0f, view->view_top,
            2.0f, view->view_h);
    fx_rect(r, fx_dim(COL_PAINT, 0.72f), road_right - 8.0f, view->view_top, 2.0f,
            view->view_h);
}

static void draw_traffic_signal(SDL_Renderer *r, float x, float y,
                                bool cross_green)
{
    fx_rect(r, FX_INK, x - 5.0f, y - 8.0f, 10.0f, 17.0f);
    fx_rect(r, FX_STEEL_DK, x - 4.0f, y - 7.0f, 8.0f, 15.0f);
    SDL_Color stop = cross_green ? FX_RED : FX_RED_DK;
    SDL_Color go = cross_green ? FX_GREEN_DK : FX_GREEN;
    fx_rect(r, stop, x - 2.0f, y - 5.0f, 4.0f, 4.0f);
    fx_rect(r, go, x - 2.0f, y + 1.0f, 4.0f, 4.0f);
    fx_glow(r, x, cross_green ? y - 3.0f : y + 3.0f, 16.0f,
            cross_green ? FX_RED : FX_GREEN, 96);
}

static void render_junctions(SDL_Renderer *r, const ChaseView *view,
                             const Chase *chase, int win_w)
{
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        float top = screen_y(view, junction->y + CHASE_JUNCTION_HALF);
        float height = CHASE_JUNCTION_HALF * 2.0f;
        if (top > view->view_top + view->view_h || top + height < view->view_top)
            continue;

        /* The cross street runs edge to edge, over blocks and lane paint. */
        fx_rect(r, COL_ASPHALT, -20.0f, top, (float)win_w + 40.0f, height);
        fx_rect(r, COL_ASPHALT_LT, -20.0f, top, (float)win_w + 40.0f, 3.0f);
        fx_rect(r, COL_ASPHALT_LT, -20.0f, top + height - 3.0f,
                (float)win_w + 40.0f, 3.0f);

        /* Cross-street lane paint, interrupted where our road passes through. */
        float middle = screen_y(view, junction->y);
        for (float x = -20.0f; x < (float)win_w + 20.0f; x += 62.0f)
        {
            if (x + 34.0f > view->road_left - 6.0f &&
                x < view->road_left + CHASE_ROAD_WIDTH + 6.0f)
            {
                continue;
            }
            fx_rect(r, COL_PAINT_MID, x, middle - 1.5f, 34.0f, 3.0f);
        }

        /* Crosswalks mark where the pursuit is crossing traffic. */
        for (int side = 0; side < 2; ++side)
        {
            float stripe_y = side == 0
                                 ? screen_y(view, junction->y + CHASE_JUNCTION_HALF - 4.0f)
                                 : screen_y(view, junction->y - CHASE_JUNCTION_HALF + 20.0f);
            for (int stripe = 0; stripe < 9; ++stripe)
            {
                fx_rect_a(r, COL_PAINT, 150,
                          view->road_left + 10.0f + (float)stripe * 52.0f,
                          stripe_y, 26.0f, 16.0f);
            }
        }

        /* Stop line and hazard chevrons on the approach side: the junction has
         * to announce itself before it scrolls into view, or crossing it is a
         * coin toss. */
        float stop_line = screen_y(view, junction->y - CHASE_JUNCTION_HALF - 8.0f);
        fx_rect_a(r, COL_PAINT, 190, view->road_left + 6.0f, stop_line,
                  CHASE_ROAD_WIDTH - 12.0f, 5.0f);
        for (int chevron = 0; chevron < 3; ++chevron)
        {
            float y = screen_y(view, junction->y - CHASE_JUNCTION_HALF - 70.0f -
                                         (float)chevron * 46.0f);
            for (int wing = 0; wing < 2; ++wing)
            {
                float x = view->road_left + CHASE_ROAD_WIDTH * 0.5f +
                          (wing == 0 ? -46.0f : 18.0f);
                fx_rect_a(r, FX_AMBER, 120, x, y, 28.0f, 6.0f);
            }
        }

        bool cross_green = chase_cross_has_green(junction, chase->time);
        float road_right = view->road_left + CHASE_ROAD_WIDTH;
        draw_traffic_signal(r, view->road_left - 8.0f, top - 12.0f, cross_green);
        draw_traffic_signal(r, road_right + 8.0f, top - 12.0f, cross_green);
        draw_traffic_signal(r, view->road_left - 8.0f, top + height + 12.0f,
                            cross_green);
        draw_traffic_signal(r, road_right + 8.0f, top + height + 12.0f,
                            cross_green);
    }
}

static void render_streetlights(SDL_Renderer *r, const ChaseView *view,
                                const Chase *chase)
{
    const float span = 260.0f;
    float first = floorf(view->camera_y / span);
    float last = ceilf((view->camera_y + view->view_h) / span);
    float road_right = view->road_left + CHASE_ROAD_WIDTH;

    for (float index = first; index <= last; index += 1.0f)
    {
        float world_y = index * span;
        if (inside_junction(chase, world_y, 20.0f))
            continue;
        float y = screen_y(view, world_y);
        for (int side = 0; side < 2; ++side)
        {
            float x = side == 0 ? view->road_left - 7.0f : road_right + 7.0f;
            float reach = side == 0 ? 34.0f : -34.0f;
            fx_glow(r, x + reach, y, 96.0f, FX_AMBER, 46);
            fx_rect(r, FX_STEEL_DK, x - 2.0f, y - 2.0f, 4.0f, 4.0f);
            fx_rect(r, FX_STEEL, x, y - 1.0f, reach, 2.0f);
            fx_rect(r, FX_AMBER, x + reach - 4.0f, y - 3.0f, 8.0f, 6.0f);
        }
    }
}

static void render_destination(SDL_Renderer *r, const ChaseView *view,
                               const Chase *chase, int win_w)
{
    if (chase->building_y <= 0.0f)
        return;
    float face = screen_y(view, chase->building_y);
    if (face > view->view_top + view->view_h)
        return;

    /* Forecourt paving with parking bays, then the tower itself. */
    fx_rect(r, COL_PAVEMENT, -20.0f, face, (float)win_w + 40.0f, 30.0f);
    fx_rect(r, COL_KERB, -20.0f, face + 28.0f, (float)win_w + 40.0f, 3.0f);
    for (int bay = 0; bay < 5; ++bay)
    {
        fx_rect_a(r, COL_PAINT, 90, 60.0f + (float)bay * 74.0f, face + 4.0f,
                  2.0f, 22.0f);
    }

    float top = view->view_top - 20.0f;
    float depth = face - top;
    fx_rect(r, FX_NIGHT, -20.0f, top, (float)win_w + 40.0f, depth);
    fx_rect(r, FX_BASE, 26.0f, top, (float)win_w - 52.0f, depth - 5.0f);
    /* Parapet: the same edge treatment as every other roof in the scene. */
    fx_rect(r, FX_MID, 26.0f, face - 12.0f, (float)win_w - 52.0f, 5.0f);
    fx_rect(r, FX_STEEL_DK, 26.0f, face - 7.0f, (float)win_w - 52.0f, 2.0f);

    /* Rooftop hardware, and the helipad the rescue ends on. */
    fx_rect(r, FX_STEEL_DK, 74.0f, face - 66.0f, 44.0f, 26.0f);
    fx_rect(r, FX_STEEL, 78.0f, face - 62.0f, 36.0f, 7.0f);
    fx_rect(r, FX_STEEL_DK, (float)win_w - 150.0f, face - 58.0f, 30.0f, 22.0f);
    float pad_x = (float)win_w * 0.5f - 44.0f;
    float pad_y = face - 96.0f;
    for (int i = 0; i < 4; ++i)
    {
        float inset = (float)i * 22.0f;
        fx_rect_a(r, COL_PAINT, 70, pad_x + inset, pad_y, 14.0f, 3.0f);
        fx_rect_a(r, COL_PAINT, 70, pad_x + inset, pad_y + 61.0f, 14.0f, 3.0f);
    }
    fx_rect_a(r, COL_PAINT, 120, pad_x + 20.0f, pad_y + 14.0f, 6.0f, 36.0f);
    fx_rect_a(r, COL_PAINT, 120, pad_x + 62.0f, pad_y + 14.0f, 6.0f, 36.0f);
    fx_rect_a(r, COL_PAINT, 120, pad_x + 26.0f, pad_y + 29.0f, 36.0f, 6.0f);

    /* Lit entrance canopy, on the kerb side the SUV pulled up to. */
    float door_x = view->road_left + CHASE_ROAD_WIDTH - 118.0f;
    fx_rect(r, FX_SHADOW, door_x - 9.0f, face - 40.0f, 118.0f, 38.0f);
    fx_rect(r, FX_STEEL_DK, door_x - 9.0f, face - 40.0f, 118.0f, 4.0f);
    fx_rect(r, FX_AMBER_DK, door_x, face - 30.0f, 100.0f, 26.0f);
    fx_rect(r, FX_AMBER, door_x + 7.0f, face - 24.0f, 86.0f, 14.0f);
    fx_glow(r, door_x + 50.0f, face - 12.0f, 140.0f, FX_AMBER, 96);
}

static void render_speed_streaks(SDL_Renderer *r, const ChaseView *view,
                                 const Chase *chase)
{
    float fast = clamp01((chase->player.speed - CHASE_CRUISE_SPEED * 0.6f) /
                         (CHASE_MAX_SPEED - CHASE_CRUISE_SPEED * 0.6f));
    if (fast <= 0.02f)
        return;

    Uint8 alpha = (Uint8)(fast * 70.0f);
    for (unsigned i = 0; i < 26u; ++i)
    {
        unsigned h = fx_hash(i * 374761393u);
        float x = view->road_left + (float)(h % (unsigned)CHASE_ROAD_WIDTH);
        float phase = fmodf(chase->time * chase->player.speed * 1.6f +
                                (float)((h >> 8) % 700u),
                            view->view_h + 120.0f);
        float y = view->view_top + phase - 60.0f;
        fx_rect_a(r, FX_PALE, alpha, x, y, 2.0f, 26.0f + fast * 34.0f);
    }
}

/* ---- Overlays and HUD ------------------------------------------------ */

static void draw_car_pip(SDL_Renderer *r, float x, float y, bool intact)
{
    SDL_Color body = intact ? PLAYER_PAINT.body_lt : (SDL_Color){44, 48, 56, 255};
    SDL_Color roof = intact ? PLAYER_PAINT.roof : (SDL_Color){32, 36, 42, 255};
    fx_rect(r, FX_INK, x, y, 9.0f, 13.0f);
    fx_rect(r, body, x + 1.0f, y + 1.0f, 7.0f, 11.0f);
    fx_rect(r, roof, x + 2.0f, y + 4.0f, 5.0f, 5.0f);
}

static void render_hud(SDL_Renderer *r, const Chase *chase, int win_w,
                       bool gamepad_active)
{
    const SDL_Color label = {108, 128, 148, 255};

    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, 37.0f,
             (SDL_Color){31, 39, 52, 255}, 255,
             (SDL_Color){11, 17, 28, 255}, 255);
    fx_rect(r, (SDL_Color){60, 76, 98, 255}, 0.0f, 0.0f, (float)win_w, 1.0f);
    fx_rect(r, FX_INK, 0.0f, 37.0f, (float)win_w, 1.0f);
    fx_rect(r, (SDL_Color){181, 132, 56, 255}, 0.0f, 38.0f, (float)win_w, 2.0f);

    fx_rect(r, FX_RED, 0.0f, 0.0f, 3.0f, 37.0f);
    draw_text(r, 12.0f, 8.0f, 1.35f, FX_CREAM, "PURSUIT");
    fx_rect(r, (SDL_Color){170, 52, 46, 255}, 12.0f, 23.0f, 74.0f, 2.0f);
    draw_text(r, 12.0f, 27.0f, 0.65f, label,
              gamepad_active ? "LEFT STICK / DPAD  DRIVE" :
                               "ARROWS / WASD  DRIVE");

    draw_text(r, 196.0f, 8.0f, 0.7f, label, "CAR");
    for (int i = 0; i < CHASE_INTEGRITY; ++i)
        draw_car_pip(r, 196.0f + (float)i * 13.0f, 19.0f,
                     i < chase->player.integrity);

    draw_text(r, 250.0f, 8.0f, 0.7f, label, "ROUTE");
    fx_rect(r, (SDL_Color){10, 15, 24, 255}, 250.0f, 20.0f, 244.0f, 11.0f);
    float route = chase_route_progress(chase) * 240.0f;
    fx_rect(r, (SDL_Color){54, 128, 128, 255}, 252.0f, 22.0f, route, 7.0f);
    fx_rect(r, (SDL_Color){123, 226, 204, 255}, 252.0f, 22.0f, route, 2.0f);

    /* The gap meter is the whole game: it fills as the SUV pulls away. */
    float gap = fmaxf(chase_gap(chase), 0.0f);
    float pressure = clamp01(gap / CHASE_LOSE_GAP);
    SDL_Color gap_color = fx_mix(FX_CYAN, FX_RED, pressure);
    char gap_text[24];
    SDL_snprintf(gap_text, sizeof(gap_text), "%03dM", (int)(gap * 0.1f));
    draw_text(r, 512.0f, 8.0f, 0.7f, label, "GAP TO SUV");
    draw_text(r, 600.0f, 8.0f, 0.7f, gap_color, gap_text);
    fx_rect(r, (SDL_Color){10, 15, 24, 255}, 512.0f, 20.0f, 176.0f, 11.0f);
    fx_rect(r, fx_dim(gap_color, 0.55f), 514.0f, 22.0f, 172.0f * pressure, 7.0f);
    fx_rect(r, gap_color, 514.0f, 22.0f, 172.0f * pressure, 2.0f);

    char speed_text[24];
    SDL_snprintf(speed_text, sizeof(speed_text), "%03d",
                 (int)(chase->player.speed * 0.5f));
    draw_text(r, 706.0f, 8.0f, 0.7f, label, "SPEED");
    draw_text(r, 706.0f, 19.0f, 1.5f, (SDL_Color){226, 232, 220, 255},
              speed_text);
    draw_text(r, 758.0f, 25.0f, 0.65f, label, "KMH");
}

/*
 * Cross traffic enters from off-screen, so the junction ahead gets an early
 * readout: how far it is and whether the cars crossing it have the green.
 */
static void render_junction_warning(SDL_Renderer *r, const ChaseView *view,
                                    const Chase *chase, int win_w)
{
    if (chase->phase != CHASE_PHASE_PURSUIT)
        return;

    const ChaseIntersection *nearest = NULL;
    float nearest_distance = CHASE_CROSS_ALERT_RANGE;
    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        const ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        float distance = junction->y - CHASE_JUNCTION_HALF - chase->player.y;
        if (distance < 0.0f || distance > nearest_distance)
            continue;
        nearest_distance = distance;
        nearest = junction;
    }
    if (nearest == NULL)
        return;

    bool cross_green = chase_cross_has_green(nearest, chase->time);
    SDL_Color accent = cross_green ? FX_RED : FX_GREEN;
    char text[40];
    SDL_snprintf(text, sizeof(text), "JUNCTION %03dM  %s",
                 (int)(nearest_distance * 0.1f),
                 cross_green ? "CROSS TRAFFIC" : "CLEAR");

    float width = text_width(text, 1.0f) + 26.0f;
    float x = ((float)win_w - width) * 0.5f;
    float y = view->view_top + 60.0f;
    Uint8 alpha = (Uint8)(170.0f * clamp01(2.0f - nearest_distance / 450.0f));
    fx_rect_a(r, FX_INK, alpha, x, y, width, 20.0f);
    fx_rect_a(r, accent, alpha, x, y, 3.0f, 20.0f);
    float pulse = cross_green ? 0.55f + 0.45f * sinf(chase->time * 8.0f) : 1.0f;
    draw_text(r, x + 14.0f, y + 6.0f, 1.0f, fx_dim(accent, pulse), text);
}

static void render_overlays(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase, int win_w, int win_h,
                            bool gamepad_active)
{
    float center_x = (float)win_w * 0.5f;
    render_junction_warning(r, view, chase, win_w);

    /* Off-screen target: the player still needs to know where the SUV went. */
    float target_screen_y = screen_y(view, chase->target.y);
    if (target_screen_y < view->view_top - CHASE_SUV_LENGTH * 0.5f &&
        chase->phase != CHASE_PHASE_FAILED)
    {
        float pulse = 0.5f + 0.5f * sinf(chase->time * 6.0f);
        SDL_Color mark = fx_dim(FX_AMBER, 0.5f + pulse * 0.5f);
        draw_text_centered(r, center_x, view->view_top + 10.0f, 1.8f, mark, "^");
        draw_text_centered(r, center_x, view->view_top + 30.0f, 0.9f, mark,
                           "SUV AHEAD");
    }

    if (chase->phase == CHASE_PHASE_DEPARTURE)
    {
        const char *caption = "THEY HAVE HER";
        if (chase->phase_time >= CHASE_DEPARTURE_IGNITION)
            caption = "STAY ON THAT SUV";
        else if (chase->phase_time >= CHASE_DEPARTURE_CHUCK_RUN - 0.05f)
            caption = "GET TO THE CAR";
        float fade = clamp01(chase->phase_time / 0.6f);
        fx_rect_a(r, FX_INK, (Uint8)(fade * 200.0f), 0.0f,
                  view->view_top + 6.0f, (float)win_w, 30.0f);
        draw_text_centered(r, center_x, view->view_top + 14.0f, 1.6f,
                           fx_dim(FX_CREAM, fade), caption);
        float blink = 0.45f + 0.55f * sinf(chase->time * 2.0f);
        draw_text(r, (float)win_w - 180.0f, (float)win_h - 31.0f, 0.75f,
                  fx_dim(FX_STEEL_LT, blink),
                  gamepad_active ? "A / START TO SKIP" :
                                   "ENTER / SPACE TO SKIP");
    }

    if (chase->phase == CHASE_PHASE_FAILED)
    {
        const char *headline = chase->failure == CHASE_FAILURE_WRECKED
                                   ? "CAR WRECKED"
                                   : "TRAIL LOST";
        fx_rect_a(r, FX_INK, 210, 0.0f, 232.0f, (float)win_w, 76.0f);
        fx_rect(r, FX_RED, 0.0f, 232.0f, (float)win_w, 2.0f);
        draw_text_centered(r, center_x, 248.0f, 2.6f, FX_RED, headline);
        draw_text_centered(r, center_x, 286.0f, 1.0f, FX_STEEL_LT,
                           "CUTTING THROUGH THE BLOCKS TO GET BACK ON THEM");
    }

    if (chase->phase == CHASE_PHASE_ARRIVAL && chase->phase_time > 1.4f)
    {
        float fade = clamp01((chase->phase_time - 1.4f) / 0.5f);
        fx_rect_a(r, FX_INK, (Uint8)(fade * 205.0f), 0.0f, 340.0f,
                  (float)win_w, 74.0f);
        fx_rect(r, fx_dim(FX_CYAN, fade), 0.0f, 340.0f, (float)win_w, 2.0f);
        draw_text_centered(r, center_x, 356.0f, 2.4f, fx_dim(FX_CYAN, fade),
                           "THEY STOPPED HERE");
        draw_text_centered(r, center_x, 392.0f, 1.0f,
                           fx_dim(FX_STEEL_LT, fade),
                           "SHE IS INSIDE THAT BUILDING");
    }
}

void chase_render(SDL_Renderer *r, const Chase *chase, int win_w, int win_h,
                  float shake_x, float shake_y, bool gamepad_active)
{
    ChaseView view;
    view.road_left = ((float)win_w - CHASE_ROAD_WIDTH) * 0.5f;
    view.view_top = (float)HUD_HEIGHT;
    view.view_h = (float)win_h - (float)HUD_HEIGHT;
    view.camera_y = chase->camera_y;
    view.shake_x = shake_x;
    view.shake_y = shake_y;

    fx_rect(r, FX_NIGHT, 0.0f, 0.0f, (float)win_w, (float)win_h);
    render_blocks(r, &view, win_w);
    render_road(r, &view, chase);
    render_junctions(r, &view, chase, win_w);
    render_streetlights(r, &view, chase);

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < CHASE_MAX_CARS; ++i)
        {
            const ChaseCar *car = &chase->cars[i];
            if (!car->active)
                continue;
            if ((car->wreck_time > 0.0f) != (pass == 0))
                continue;
            draw_traffic_car(r, &view, car);
        }
    }
    draw_target_car(r, &view, chase);
    draw_player_car(r, &view, chase);
    /* Drawn after the traffic so the building hides whatever has driven past
     * its front and turned off the road. */
    render_destination(r, &view, chase, win_w);
    draw_chuck_on_foot(r, &view, chase);
    render_speed_streaks(r, &view, chase);

    render_overlays(r, &view, chase, win_w, win_h, gamepad_active);
    render_hud(r, chase, win_w, gamepad_active);

    /* Same finishing pass as every other screen in the game. */
    fx_vignette(r, win_w, win_h, 64);
    fx_scanlines(r, win_w, win_h, 11);
}
