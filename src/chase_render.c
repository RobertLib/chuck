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

/* The chase's material table: the road surfaces, markings and glass this
 * scene alone is made of, named once here the way level_art.c names walls. */
static const SDL_Color COL_ASPHALT = {26, 30, 38, 255};
static const SDL_Color COL_ASPHALT_LT = {35, 40, 49, 255};
static const SDL_Color COL_PAVEMENT = {46, 52, 62, 255};
static const SDL_Color COL_KERB = {96, 104, 114, 255};
static const SDL_Color COL_PAINT = {198, 204, 194, 255};
static const SDL_Color COL_PAINT_MID = {150, 146, 96, 255};
static const SDL_Color COL_GLASS = {11, 20, 28, 255};

/* Wreck soot: warm neutral chars for burnt-out panels, deliberately off the
 * blue-slate ramp so a burnt car reads as burnt rather than repainted. */
static const SDL_Color SOOT = {44, 40, 40, 255};
static const SDL_Color SOOT_LT = {62, 56, 54, 255};
static const SDL_Color SOOT_DK = {30, 28, 28, 255};

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

/* Chuck's car wears his jacket: the FX_HERO ramp, not a fourth blue. A
 * function because the fx.h colours are const objects, which C17 will not
 * accept inside a static initializer. */
static CarPaint player_paint(void)
{
    return (CarPaint){FX_HERO, FX_HERO_LT, FX_HERO_DK};
}

static const CarPaint TARGET_PAINT = {
    {38, 44, 46, 255}, {62, 68, 66, 255}, {24, 28, 30, 255}};

/* Cordon livery: near-black with a white flank stripe implied by the light
 * roof. Kept well away from TARGET_PAINT's charcoal — the one car in this
 * scene the player must never mistake for another is the SUV. */
static const CarPaint CORDON_PAINT = {
    {30, 38, 56, 255}, {188, 194, 198, 255}, {22, 28, 42, 255}};

/* Emergency-beacon blue. Named here for the same reason the facade names its
 * own: FX_CYAN is the game's technology accent and FX_LAMP is a fluorescent
 * tube, and a light bar is neither. The red half of the bar is FX_RED, which
 * is precisely what the palette's danger red is for. */
static const SDL_Color COL_BEACON_BLUE = {60, 116, 236, 255};

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

static void car_part_a(SDL_Renderer *r, const CarFrame *f,
                       float a0, float a1, float b0, float b1,
                       SDL_Color color, Uint8 alpha)
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
    fx_rect_a(r, color, alpha, floorf(x), floorf(y),
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
        body = fx_mix(body, SOOT, burn);
        body_lt = fx_mix(body_lt, SOOT_LT, burn);
        roof = fx_mix(roof, SOOT_DK, burn);
    }

    /* Contact shadow: a translucent pool set down-right and inset a pixel,
     * so it reads as light the car blocks rather than a second, black car. */
    CarFrame shadow = *f;
    shadow.cx += 3.0f;
    shadow.cy += 4.0f;
    car_part_a(r, &shadow, -0.48f, 0.48f, -0.46f, 0.46f, FX_INK, 150);

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
    /* A brake lamp is FX_RED lit, not a new red: the ramp's own bright step. */
    SDL_Color tail = braking ? fx_ramp(FX_RED).lit : FX_RED_DK;
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
        SDL_Color spark = (i & 1u) ? FX_AMBER : fx_mix(FX_PALE, FX_CREAM, 0.7f);
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
    /* Roof rack and a spare on the back: the SUV has to be unmistakable.
     * All three fittings come out of the SUV's own paint so a fourth grey
     * never joins the table. */
    car_part(r, &frame, -0.22f, 0.06f, -0.30f, 0.30f,
             fx_mix(TARGET_PAINT.body, TARGET_PAINT.body_lt, 0.6f));
    car_part(r, &frame, -0.22f, 0.06f, -0.30f, -0.24f,
             fx_mix(TARGET_PAINT.body_lt, FX_PALE, 0.15f));
    car_part(r, &frame, -0.56f, -0.50f, -0.22f, 0.22f, TARGET_PAINT.roof);

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

    CarPaint paint = player_paint();
    /* Damage is legible on the car itself, not only in the HUD: worn paint
     * dulls toward the same soot the wrecks burn to. */
    float wear = 1.0f - (float)car->integrity / (float)CHASE_INTEGRITY;
    if (wear > 0.0f)
    {
        paint.body = fx_mix(paint.body, SOOT_LT, wear * 0.55f);
        paint.body_lt = fx_mix(paint.body_lt, fx_mix(SOOT_LT, FX_PALE, 0.25f),
                               wear * 0.55f);
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

/*
 * One limb's place in a two-beat stride, -1 (fully back) to +1 (fully
 * forward). The first half of the cycle is stance: the limb tracks straight
 * back under the body at a constant rate, carrying it. The second half is
 * the swing, eased so it is quick through the middle and slow where the limb
 * takes or gives up the load. A sine is slowest exactly where the stride
 * should be fastest, which reads as skating even seen from above; the other
 * limb runs the same cycle half a turn along.
 */
static float stride_offset(float cycle)
{
    cycle -= floorf(cycle);
    if (cycle < 0.5f)
        return 1.0f - 4.0f * cycle;
    float t = (cycle - 0.5f) * 2.0f;
    float ease = t * t * (3.0f - 2.0f * t);
    return -1.0f + 2.0f * ease;
}

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
    /* Two limbs half a cycle apart, ~2.4 strides a second at a flat run. */
    float cycle = chase->phase_time * 2.4f;
    float near_limb = running ? stride_offset(cycle) : 0.0f;
    float far_limb = running ? stride_offset(cycle + 0.5f) : 0.0f;

    /* A pool of light under him so a 16-pixel figure still reads at night. */
    fx_glow(r, x, y, 34.0f, FX_AMBER, 44);
    fx_contact_shadow(r, x + 1.0f, y + 8.0f, 7.0f, 0.0f, 140);
    /* Seen from above: shoulders, jacket, head, and swinging arms. Forward
     * is screen-up, so a limb at +1 sits above the shoulder line. */
    fx_rect(r, FX_HERO_DK, x - 8.0f, y - 8.0f, 16.0f, 16.0f);
    fx_rect(r, FX_HERO, x - 7.0f, y - 7.0f, 14.0f, 13.0f);
    fx_rect(r, FX_HERO_LT, x - 7.0f, y - 7.0f, 14.0f, 4.0f);
    fx_rect(r, FX_HERO_DK, x - 12.0f, y - 4.0f - near_limb * 3.0f, 5.0f, 7.0f);
    fx_rect(r, FX_HERO_DK, x + 7.0f, y - 4.0f - far_limb * 3.0f, 5.0f, 7.0f);
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

/*
 * A squad car holding the side street at a junction.
 *
 * This is the cover story made visible. The demand went out at 00:20 and put
 * the whole city's night shift on a ring around one building; the drive in
 * runs through that ring from the outside, so the junctions fill up with cars
 * facing the wrong way while Chuck goes past them in the one direction nobody
 * is watching. He cannot stop and he cannot be helped by them, which is the
 * point of drawing them at all.
 *
 * It stands in the cross street beyond the pavement, never in a lane: nothing
 * here is part of the simulation, and a car in a lane that the player's own
 * car drives straight through would be a bug rather than a detail. Parked
 * nose-in to the main road, because that is how a road gets closed.
 */
static void draw_cordon_car(SDL_Renderer *r, float cx, float cy, int facing,
                            float time, unsigned seed)
{
    CarFrame frame;
    frame.cx = cx;
    frame.cy = cy;
    frame.ax = (float)facing;
    frame.ay = 0.0f;
    frame.length = CHASE_CAR_LENGTH;
    frame.width = CHASE_CAR_WIDTH;

    draw_car_body(r, &frame, &CORDON_PAINT, false, false, 0.0f);

    /* The bar itself: two halves alternating on their own beat, salted per car
     * so a street of them never flashes in unison. The bar sits across the
     * roof, so it runs along the car's width — which from above is the axis
     * the body is not pointing down. */
    float rx = -frame.ay;
    float ry = frame.ax;
    float phase = fmodf(time * CHASE_CORDON_STROBE_HZ +
                            (float)(seed % 100u) * 0.01f,
                        1.0f);
    bool blue_half = phase < 0.5f;
    float flash = blue_half ? 1.0f - phase * 2.0f : 1.0f - (phase - 0.5f) * 2.0f;
    flash = 0.35f + 0.65f * flash;

    for (int half = 0; half < 2; ++half)
    {
        bool lit = (half == 0) == blue_half;
        SDL_Color c = half == 0 ? COL_BEACON_BLUE : FX_RED;
        float offset = (half == 0 ? -0.19f : 0.19f);
        float bx = cx + rx * offset * frame.width;
        float by = cy + ry * offset * frame.width;
        car_part(r, &frame, -0.10f, 0.02f, offset - 0.15f, offset + 0.15f,
                 lit ? fx_mix(c, FX_CREAM, 0.35f) : fx_dim(c, 0.32f));
        if (lit)
            fx_glow(r, bx, by, 46.0f, c, (Uint8)(150.0f * flash));
    }
    /* And what the bar throws on the road it is standing on. A beacon that
     * lights nothing is a sticker on a roof. */
    fx_glow(r, cx, cy, 96.0f, blue_half ? COL_BEACON_BLUE : FX_RED,
            (Uint8)(46.0f * flash));
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

        if (junction->cordon_side != 0)
        {
            float edge = junction->cordon_side < 0
                             ? view->road_left - CHASE_PAVEMENT_WIDTH
                             : road_right + CHASE_PAVEMENT_WIDTH;
            float cx = edge + (float)junction->cordon_side *
                                  (CHASE_CORDON_KERB_INSET +
                                   CHASE_CAR_LENGTH * 0.5f);
            draw_cordon_car(r, cx, middle, -junction->cordon_side, chase->time,
                            fx_hash((unsigned)i * 2654435761u +
                                    (unsigned)(junction->y * 0.5f)));
        }
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
    CarPaint hero = player_paint();
    /* A lost pip is the car gone grey: slate steps off the shared ramp, so
     * it recedes instead of reading as a second paint option. */
    SDL_Color body = intact ? hero.body_lt : fx_mix(FX_SHADOW, FX_STEEL, 0.4f);
    SDL_Color roof = intact ? hero.roof : fx_mix(FX_SHADOW, FX_STEEL, 0.2f);
    fx_rect(r, FX_INK, x, y, 9.0f, 13.0f);
    fx_rect(r, body, x + 1.0f, y + 1.0f, 7.0f, 11.0f);
    fx_rect(r, roof, x + 2.0f, y + 4.0f, 5.0f, 5.0f);
}

static void render_hud(SDL_Renderer *r, const Chase *chase, int win_w,
                       const PadHints *pad)
{
    fx_vgrad(r, 0.0f, 0.0f, (float)win_w, 37.0f,
             fx_mix(FX_BASE, FX_MID, 0.30f), 255,
             fx_mix(FX_NIGHT, FX_SHADOW, 0.40f), 255);
    fx_rect(r, fx_mix(FX_MID, FX_STEEL_LT, 0.35f), 0.0f, 0.0f, (float)win_w,
            1.0f);
    fx_rect(r, FX_INK, 0.0f, 37.0f, (float)win_w, 1.0f);
    fx_rect(r, fx_dim(FX_AMBER, 0.73f), 0.0f, 38.0f, (float)win_w, 2.0f);

    fx_rect(r, FX_RED, 0.0f, 0.0f, 3.0f, 37.0f);
    draw_text(r, 12.0f, 4.0f, 2.0f, FX_CREAM, "PURSUIT");
    fx_rect(r, fx_dim(FX_RED, 0.73f), 12.0f, 22.0f, 112.0f, 2.0f);
    /* The readout under the title names the pedals rather than the hardware:
     * "STICK / DPAD DRIVE" told a player holding a pad everything except the
     * one thing they needed, which is which button makes the car go. */
    char pedals[40];
    draw_text(r, 12.0f, 27.0f, 1.0f, FX_LABEL,
              pad_hint(pad, pedals, sizeof(pedals), "$A GAS   $B BRAKE",
                       "UP GAS   DOWN BRAKE"));

    draw_text(r, 196.0f, 8.0f, 1.0f, FX_LABEL, "CAR");
    for (int i = 0; i < CHASE_INTEGRITY; ++i)
        draw_car_pip(r, 196.0f + (float)i * 13.0f, 19.0f,
                     i < chase->player.integrity);

    draw_text(r, 250.0f, 8.0f, 1.0f, FX_LABEL, "ROUTE");
    fx_rect(r, FX_NIGHT, 250.0f, 20.0f, 244.0f, 11.0f);
    float route = chase_route_progress(chase) * 240.0f;
    fx_rect(r, fx_mix(FX_CYAN_DK, FX_CYAN, 0.2f), 252.0f, 22.0f, route, 7.0f);
    fx_rect(r, FX_CYAN, 252.0f, 22.0f, route, 2.0f);

    /* The gap meter is the whole game: it fills as the SUV pulls away. */
    float gap = fmaxf(chase_gap(chase), 0.0f);
    float pressure = clamp01(gap / CHASE_LOSE_GAP);
    SDL_Color gap_color = fx_mix(FX_CYAN, FX_RED, pressure);
    char gap_text[24];
    SDL_snprintf(gap_text, sizeof(gap_text), "%03dM", (int)(gap * 0.1f));
    draw_text(r, 512.0f, 8.0f, 1.0f, FX_LABEL, "GAP TO SUV");
    draw_text(r, 600.0f, 8.0f, 1.0f, gap_color, gap_text);
    fx_rect(r, FX_NIGHT, 512.0f, 20.0f, 176.0f, 11.0f);
    fx_rect(r, fx_dim(gap_color, 0.55f), 514.0f, 22.0f, 172.0f * pressure, 7.0f);
    fx_rect(r, gap_color, 514.0f, 22.0f, 172.0f * pressure, 2.0f);

    char speed_text[24];
    SDL_snprintf(speed_text, sizeof(speed_text), "%03d",
                 (int)(chase->player.speed * 0.5f));
    draw_text(r, 706.0f, 8.0f, 1.0f, FX_LABEL, "SPEED");
    draw_text(r, 706.0f, 19.0f, 2.0f, FX_CREAM, speed_text);
    draw_text(r, 758.0f, 25.0f, 1.0f, FX_LABEL, "KMH");
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

/*
 * The pedals, spelled out on the road.
 *
 * The drive is the one beat of the game that is not a platformer, and the
 * platformer never asks for a throttle: told nothing, a player holds a
 * direction and watches the SUV pull away without ever learning that the car
 * had to be driven. So the two pedals are named at the head of every attempt —
 * a crash is exactly when someone needs to read them again — and they fade out
 * once the drive is under way, because a prompt that never leaves is a prompt
 * nobody reads. What stays is the HUD line under PURSUIT, which names the same
 * two buttons for anyone who arrives late.
 */
static void render_control_hint(SDL_Renderer *r, const Chase *chase, int win_w,
                                int win_h, const PadHints *pad)
{
    /* Only over the drive itself: the departure is watched rather than driven,
     * and its own beat already carries a caption and a skip prompt. */
    if (chase->phase != CHASE_PHASE_PURSUIT)
        return;
    float fade = fminf(clamp01(chase->phase_time / 0.4f),
                       clamp01(CHASE_CONTROL_HINT_TIME - chase->phase_time));
    if (fade <= 0.0f)
        return;

    char pedals[64];
    pad_hint(pad, pedals, sizeof(pedals), "$A ACCELERATE    $B BRAKE",
             "UP ACCELERATE    DOWN BRAKE");
    const char *steer = pad != NULL ? "STICK OR DPAD STEERS"
                                    : "LEFT AND RIGHT STEER";

    float width = fmaxf(text_width(pedals, 2.0f), text_width(steer, 1.0f)) +
                  40.0f;
    float x = ((float)win_w - width) * 0.5f;
    /* Under the car, not over it: the camera keeps Chuck's bonnet a fixed
     * CHASE_CAMERA_LEAD off the bottom edge, so this band is the one strip of
     * road the player never drives through. */
    float y = (float)win_h - 88.0f;
    Uint8 alpha = (Uint8)(fade * 200.0f);
    fx_rect_a(r, FX_INK, alpha, x, y, width, 44.0f);
    fx_rect_a(r, FX_AMBER, alpha, x, y, 3.0f, 44.0f);
    float center_x = (float)win_w * 0.5f;
    draw_text_centered(r, center_x, y + 7.0f, 2.0f, fx_dim(FX_CREAM, fade),
                       pedals);
    draw_text_centered(r, center_x, y + 29.0f, 1.0f, fx_dim(FX_LABEL, fade),
                       steer);
}

static void render_overlays(SDL_Renderer *r, const ChaseView *view,
                            const Chase *chase, int win_w, int win_h,
                            const PadHints *pad)
{
    float center_x = (float)win_w * 0.5f;
    render_junction_warning(r, view, chase, win_w);
    render_control_hint(r, chase, win_w, win_h, pad);

    /* Off-screen target: the player still needs to know where the SUV went. */
    float target_screen_y = screen_y(view, chase->target.y);
    if (target_screen_y < view->view_top - CHASE_SUV_LENGTH * 0.5f &&
        chase->phase != CHASE_PHASE_FAILED)
    {
        float pulse = 0.5f + 0.5f * sinf(chase->time * 6.0f);
        SDL_Color mark = fx_dim(FX_AMBER, 0.5f + pulse * 0.5f);
        draw_text_centered(r, center_x, view->view_top + 10.0f, 2.0f, mark, "^");
        draw_text_centered(r, center_x, view->view_top + 30.0f, 1.0f, mark,
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
        draw_text_centered(r, center_x, view->view_top + 14.0f, 2.0f,
                           fx_dim(FX_CREAM, fade), caption);
        float blink = 0.45f + 0.55f * sinf(chase->time * 2.0f);
        char hint[32];
        draw_text(r, (float)win_w - 180.0f, (float)win_h - 31.0f, 1.0f,
                  fx_dim(FX_STEEL_LT, blink),
                  pad_hint(pad, hint, sizeof(hint), "$Y TO SKIP",
                           "ENTER / SPACE TO SKIP"));
    }

    if (chase->phase == CHASE_PHASE_FAILED)
    {
        const char *headline = chase->failure == CHASE_FAILURE_WRECKED
                                   ? "CAR WRECKED"
                                   : "TRAIL LOST";
        fx_rect_a(r, FX_INK, 210, 0.0f, 232.0f, (float)win_w, 76.0f);
        fx_rect(r, FX_RED, 0.0f, 232.0f, (float)win_w, 2.0f);
        draw_text_centered(r, center_x, 248.0f, 3.0f, FX_RED, headline);
        draw_text_centered(r, center_x, 286.0f, 1.0f, FX_STEEL_LT,
                           "CUTTING THROUGH THE BLOCKS TO GET BACK ON THEM");
    }

    /* After enough failed attempts the drive stops insisting on itself. */
    if (chase->phase == CHASE_PHASE_PURSUIT &&
        chase->attempts >= CHASE_SKIP_AFTER_ATTEMPTS)
    {
        float blink = 0.45f + 0.55f * sinf(chase->time * 2.0f);
        char hint[32];
        draw_text(r, (float)win_w - 196.0f, (float)win_h - 31.0f, 1.0f,
                  fx_dim(FX_STEEL_LT, blink),
                  pad_hint(pad, hint, sizeof(hint), "$Y: SKIP THE DRIVE",
                           "ENTER: SKIP THE DRIVE"));
    }

    if (chase->phase == CHASE_PHASE_ARRIVAL && chase->phase_time > 1.4f)
    {
        float fade = clamp01((chase->phase_time - 1.4f) / 0.5f);
        fx_rect_a(r, FX_INK, (Uint8)(fade * 205.0f), 0.0f, 340.0f,
                  (float)win_w, 74.0f);
        fx_rect(r, fx_dim(FX_CYAN, fade), 0.0f, 340.0f, (float)win_w, 2.0f);
        draw_text_centered(r, center_x, 356.0f, 2.0f, fx_dim(FX_CYAN, fade),
                           "THEY STOPPED HERE");
        draw_text_centered(r, center_x, 392.0f, 1.0f,
                           fx_dim(FX_STEEL_LT, fade),
                           "THEY WALKED HER INTO KESSLER TOWER");
    }
}

void chase_render(SDL_Renderer *r, const Chase *chase, int win_w, int win_h,
                  float shake_x, float shake_y, const PadHints *pad)
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

    render_overlays(r, &view, chase, win_w, win_h, pad);
    render_hud(r, chase, win_w, pad);

    /* Finishing (vignette, scanlines) belongs to game_render's one shared
       pass — the pause overlay has to sit under it, and it is drawn by the
       shell after this function returns. */
}
