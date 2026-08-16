#include "chase.h"

#include <math.h>
#include <string.h>

/*
 * Prologue pursuit simulation. See chase.h for the coordinate system.
 *
 * Everything here is driven by the seeded `Rng` and the frame delta, so a given
 * seed plus a given input sequence always produces the same drive. The module
 * emits sounds and camera shake into its own event buffer; the shell turns
 * those into audio and screen shake exactly as it does for the platformer.
 */

static float clampf(float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static float approach(float value, float goal, float rate, float dt)
{
    float step = rate * dt;
    if (value < goal)
    {
        value += step;
        return value > goal ? goal : value;
    }
    value -= step;
    return value < goal ? goal : value;
}

static bool crossed(float previous, float current, float cue)
{
    return previous < cue && current >= cue;
}

static float rng_between(Rng *rng, float low, float high)
{
    return low + rng_unit(rng) * (high - low);
}

float chase_lane_center(int lane)
{
    if (lane < 0)
        lane = 0;
    if (lane >= CHASE_LANE_COUNT)
        lane = CHASE_LANE_COUNT - 1;
    return ((float)lane + 0.5f) * CHASE_LANE_WIDTH;
}

float chase_gap(const Chase *chase)
{
    return chase->target.y - chase->player.y;
}

bool chase_cross_has_green(const ChaseIntersection *junction, float time)
{
    float cycle = fmodf(time + junction->signal_offset, CHASE_SIGNAL_PERIOD);
    if (cycle < 0.0f)
        cycle += CHASE_SIGNAL_PERIOD;
    return cycle < CHASE_SIGNAL_CROSS_GREEN;
}

float chase_route_progress(const Chase *chase)
{
    if (chase->phase == CHASE_PHASE_ARRIVAL || chase->phase == CHASE_PHASE_DONE)
        return 1.0f;
    return clampf(chase->pursuit_time / CHASE_PURSUIT_DURATION, 0.0f, 1.0f);
}

/* ---- Layout ---------------------------------------------------------- */

static void clear_road(Chase *chase)
{
    memset(chase->cars, 0, sizeof(chase->cars));
    memset(chase->intersections, 0, sizeof(chase->intersections));
}

/*
 * Places both cars and the road generator at the start of a pursuit attempt.
 *
 * `resume_y` is where on the route the attempt picks up, and it is a parameter
 * rather than zero because the cordon is a *spatial* ramp: how likely a
 * junction is to be held is read off the block it is generated in, so a retry
 * that put the player back on block zero rebuilt the ring from its thinnest
 * end with only a fraction of the clock left to cross it. Measured before this,
 * a crash near the end left the tower standing behind an empty street — the
 * exact opposite of what the drive is there to show.
 */
static void reset_pursuit_layout(Chase *chase, float resume_y)
{
    clear_road(chase);
    chase->player.x = chase_lane_center(CHASE_LANE_COUNT - 1);
    chase->player.y = resume_y;
    chase->player.speed = CHASE_CRUISE_SPEED;
    chase->player.integrity = CHASE_INTEGRITY;
    chase->player.invuln_timer = CHASE_HIT_INVULN;
    chase->player.scrape_timer = 0.0f;
    chase->player.engine_running = true;

    chase->target.x = chase_lane_center(CHASE_LANE_COUNT - 1);
    chase->target.lane_target_x = chase->target.x;
    chase->target.y = resume_y + CHASE_START_GAP;
    chase->target.speed = CHASE_TARGET_SPEED;
    chase->target.lane_timer = CHASE_TARGET_LANE_TIME_MIN;
    chase->target.boost_timer = 0.0f;

    chase->camera_y = chase->player.y - CHASE_CAMERA_LEAD;
    chase->generated_y = chase->player.y;
    chase->building_y = 0.0f;
    chase->pursuit_time = 0.0f;
    chase->failure = CHASE_FAILURE_NONE;
}

static void begin_phase(Chase *chase, ChasePhase phase)
{
    chase->phase = phase;
    chase->phase_time = 0.0f;
}

void chase_init(Chase *chase, uint64_t seed)
{
    memset(chase, 0, sizeof(*chase));
    rng_seed(&chase->rng, seed);

    chase->player.x = CHASE_KERB_X;
    chase->player.y = 0.0f;
    chase->player.integrity = CHASE_INTEGRITY;
    chase->target.x = CHASE_KERB_X;
    chase->target.lane_target_x = chase_lane_center(CHASE_LANE_COUNT - 1);
    chase->target.y = CHASE_DEPARTURE_TARGET_OFFSET;
    chase->target.lane_timer = CHASE_TARGET_LANE_TIME_MIN;
    chase->camera_y = chase->player.y - CHASE_CAMERA_LEAD;
    chase->generated_y = 0.0f;
    begin_phase(chase, CHASE_PHASE_DEPARTURE);
}

/* ---- Road generation ------------------------------------------------- */

static ChaseCar *free_car_slot(Chase *chase)
{
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        if (!chase->cars[i].active)
            return &chase->cars[i];
    }
    return NULL;
}

/* Collision extents in road space: a car crossing a junction lies sideways, so
 * its two axes are swapped compared with a car in a lane. */
static float car_half_x(const ChaseCar *car)
{
    return (car->kind == CHASE_CAR_CROSSING ? CHASE_CAR_LENGTH
                                            : CHASE_CAR_WIDTH) * 0.5f;
}

static float car_half_y(const ChaseCar *car)
{
    return (car->kind == CHASE_CAR_CROSSING ? CHASE_CAR_WIDTH
                                            : CHASE_CAR_LENGTH) * 0.5f;
}

/*
 * Traffic is only fair if the player always has somewhere to go, so a lane slot
 * is refused when it would put more than CHASE_MAX_CARS_ABREAST cars across the
 * same stretch of road, or drop a car on top of another one.
 */
static bool lane_slot_is_free(const Chase *chase, float x, float y)
{
    int abreast = 0;
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        const ChaseCar *other = &chase->cars[i];
        if (!other->active || other->kind == CHASE_CAR_CROSSING)
            continue;
        if (fabsf(other->y - y) > CHASE_CAR_LENGTH * 1.7f)
            continue;
        if (fabsf(other->x - x) < CHASE_LANE_WIDTH * 0.9f)
            return false;
        abreast++;
    }
    return abreast < CHASE_MAX_CARS_ABREAST;
}

static void generate_block(Chase *chase)
{
    float start = chase->generated_y;
    float end = start + CHASE_BLOCK_LENGTH;
    float junction_y = end - CHASE_JUNCTION_HALF - 40.0f;

    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        ChaseIntersection *junction = &chase->intersections[i];
        if (junction->active)
            continue;
        junction->active = true;
        junction->y = junction_y;
        junction->signal_offset = rng_between(&chase->rng, 0.0f, CHASE_SIGNAL_PERIOD);
        junction->cross_spawn_timer = rng_between(&chase->rng, CHASE_CROSS_GAP_MIN,
                                                  CHASE_CROSS_GAP_MAX);
        /* The cordon closing in. The first couple of blocks are an ordinary
         * night out on the ring road; from there the odds of a junction being
         * held climb toward near-certainty, so by the time the tower is in
         * frame the player has driven past most of a city's night shift and
         * every one of them is facing the wrong way. */
        int block = (int)(start / CHASE_BLOCK_LENGTH);
        junction->cordon_side = 0;
        if (block >= CHASE_CORDON_FIRST_BLOCK)
        {
            int ramp = block - CHASE_CORDON_FIRST_BLOCK;
            if (ramp > CHASE_CORDON_RAMP_BLOCKS)
                ramp = CHASE_CORDON_RAMP_BLOCKS;
            int chance = CHASE_CORDON_CHANCE_START +
                         (CHASE_CORDON_CHANCE_END - CHASE_CORDON_CHANCE_START) *
                             ramp / CHASE_CORDON_RAMP_BLOCKS;
            if (rng_range(&chase->rng, 100) < chance)
                junction->cordon_side = rng_range(&chase->rng, 2) == 0 ? -1 : 1;
        }
        break;
    }

    int wanted = 2 + rng_range(&chase->rng, 3);
    for (int i = 0; i < wanted; ++i)
    {
        int lane = rng_range(&chase->rng, CHASE_LANE_COUNT);
        float x = chase_lane_center(lane);
        float y = start + rng_between(&chase->rng, 140.0f,
                                      CHASE_BLOCK_LENGTH - 200.0f);
        /* Junctions stay clear: stopped traffic there would be unreadable. */
        if (fabsf(y - junction_y) < CHASE_JUNCTION_HALF + CHASE_CAR_LENGTH)
            continue;
        if (!lane_slot_is_free(chase, x, y))
            continue;

        ChaseCar *car = free_car_slot(chase);
        if (car == NULL)
            break;

        bool oncoming = lane < CHASE_FIRST_FORWARD_LANE;
        memset(car, 0, sizeof(*car));
        car->active = true;
        car->kind = oncoming ? CHASE_CAR_ONCOMING : CHASE_CAR_TRAFFIC;
        car->x = x;
        car->y = y;
        car->vx = 0.0f;
        car->vy = oncoming
                      ? -rng_between(&chase->rng, CHASE_ONCOMING_SPEED_MIN,
                                     CHASE_ONCOMING_SPEED_MAX)
                      : rng_between(&chase->rng, CHASE_TRAFFIC_SPEED_MIN,
                                    CHASE_TRAFFIC_SPEED_MAX);
        car->variant = rng_range(&chase->rng, 4);
    }

    chase->generated_y = end;
}

static void generate_road_ahead(Chase *chase)
{
    /* The destination closes the route: nothing new is laid down past it. */
    if (chase->building_y > 0.0f)
        return;
    while (chase->generated_y < chase->camera_y + CHASE_SPAWN_MARGIN)
        generate_block(chase);
}

static void spawn_crossing_car(Chase *chase, ChaseIntersection *junction)
{
    ChaseCar *car = free_car_slot(chase);
    if (car == NULL)
        return;

    bool eastbound = rng_range(&chase->rng, 2) == 0;
    float speed = rng_between(&chase->rng, CHASE_CROSS_SPEED_MIN,
                              CHASE_CROSS_SPEED_MAX);
    memset(car, 0, sizeof(*car));
    car->active = true;
    car->kind = CHASE_CAR_CROSSING;
    /* Cross traffic keeps right as well, so the two directions never share a
     * line and the player can read which way a car is travelling. */
    car->y = junction->y + (eastbound ? -CHASE_CROSS_LANE_OFFSET
                                      : CHASE_CROSS_LANE_OFFSET);
    car->x = eastbound ? -CHASE_CAR_LENGTH : CHASE_ROAD_WIDTH + CHASE_CAR_LENGTH;
    car->vx = eastbound ? speed : -speed;
    car->vy = 0.0f;
    car->variant = rng_range(&chase->rng, 4);
}

static void update_junctions(Chase *chase, float dt)
{
    bool traffic_flows = chase->phase == CHASE_PHASE_DEPARTURE ||
                         chase->phase == CHASE_PHASE_PURSUIT;

    for (int i = 0; i < CHASE_MAX_INTERSECTIONS; ++i)
    {
        ChaseIntersection *junction = &chase->intersections[i];
        if (!junction->active)
            continue;
        if (junction->y < chase->camera_y - CHASE_CULL_MARGIN)
        {
            junction->active = false;
            continue;
        }
        if (!traffic_flows)
            continue;

        float distance = junction->y - chase->player.y;
        if (distance < -CHASE_JUNCTION_HALF || distance > CHASE_CROSS_ALERT_RANGE)
            continue;

        if (!chase_cross_has_green(junction, chase->time))
        {
            /* Waiting cars pull away shortly after the light turns. */
            junction->cross_spawn_timer = 0.25f;
            continue;
        }
        junction->cross_spawn_timer -= dt;
        if (junction->cross_spawn_timer <= 0.0f)
        {
            spawn_crossing_car(chase, junction);
            junction->cross_spawn_timer = rng_between(&chase->rng,
                                                      CHASE_CROSS_GAP_MIN,
                                                      CHASE_CROSS_GAP_MAX);
        }
    }
}

static void update_cars(Chase *chase, float dt)
{
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        ChaseCar *car = &chase->cars[i];
        if (!car->active)
            continue;

        if (car->wreck_time > 0.0f)
        {
            car->wreck_time += dt;
            /* A wrecked car slews to a halt and stops being a threat. */
            car->vy = approach(car->vy, 0.0f, 260.0f, dt);
            car->vx = approach(car->vx, 0.0f, 190.0f, dt);
        }

        car->x += car->vx * dt;
        car->y += car->vy * dt;

        /* Past the destination the road is closed: traffic has turned off. */
        if ((chase->building_y > 0.0f && car->y > chase->building_y - 40.0f) ||
            car->y < chase->camera_y - CHASE_CULL_MARGIN ||
            car->x < -CHASE_CAR_LENGTH * 2.0f ||
            car->x > CHASE_ROAD_WIDTH + CHASE_CAR_LENGTH * 2.0f)
        {
            car->active = false;
        }
    }
}

/* ---- Collisions ------------------------------------------------------ */

static bool boxes_overlap(float ax, float ay, float ahw, float ahh,
                          float bx, float by, float bhw, float bhh)
{
    /* A little forgiveness on both boxes: a paint scrape is not a crash. */
    const float slack_x = 3.0f;
    const float slack_y = 4.0f;
    return fabsf(ax - bx) < (ahw + bhw - slack_x * 2.0f) &&
           fabsf(ay - by) < (ahh + bhh - slack_y * 2.0f);
}

/* Knocks a car out of the traffic flow. The sound belongs to whoever hit it. */
static void wreck_car(ChaseCar *car, float push_dir)
{
    if (car->wreck_time > 0.0f)
        return;
    car->wreck_time = 0.0001f;
    car->vx = push_dir * CHASE_WRECK_DRIFT;
    if (car->kind == CHASE_CAR_ONCOMING)
        car->vy *= 0.35f;
}

static void fail_pursuit(Chase *chase, ChaseFailure failure)
{
    chase->failure = failure;
    begin_phase(chase, CHASE_PHASE_FAILED);
    if (failure == CHASE_FAILURE_WRECKED)
    {
        game_events_sound(&chase->events, SFX_EXPLOSION);
        game_events_camera_shake(&chase->events, 13.0f, 0.75f);
    }
    else
    {
        game_events_sound(&chase->events, SFX_CARD_WRONG);
    }
}

static void hit_player(Chase *chase, float push_dir)
{
    chase->player.integrity--;
    chase->player.invuln_timer = CHASE_HIT_INVULN;
    chase->player.speed = CHASE_CRASH_SPEED;
    chase->player.x += push_dir * 9.0f;
    game_events_sound(&chase->events, SFX_CHASE_CRASH);
    game_events_camera_shake(&chase->events, 9.0f, 0.45f);
    if (chase->player.integrity <= 0)
        fail_pursuit(chase, CHASE_FAILURE_WRECKED);
}

static void check_player_collisions(Chase *chase)
{
    const float player_half_w = CHASE_CAR_WIDTH * 0.5f;
    const float player_half_h = CHASE_CAR_LENGTH * 0.5f;
    bool near_miss = false;

    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        ChaseCar *car = &chase->cars[i];
        if (!car->active)
            continue;

        float half_w = car_half_x(car);
        float half_h = car_half_y(car);
        if (chase->player.invuln_timer <= 0.0f && car->wreck_time <= 0.0f &&
            boxes_overlap(chase->player.x, chase->player.y, player_half_w,
                          player_half_h, car->x, car->y, half_w, half_h))
        {
            float push = chase->player.x < car->x ? -1.0f : 1.0f;
            wreck_car(car, -push);
            hit_player(chase, push);
            return;
        }

        if (car->kind == CHASE_CAR_TRAFFIC || car->wreck_time > 0.0f)
            continue;
        float ahead = car->y - chase->player.y;
        if (ahead > 0.0f && ahead < CHASE_NEAR_MISS_AHEAD &&
            fabsf(car->x - chase->player.x) < CHASE_NEAR_MISS_SIDE)
        {
            near_miss = true;
        }
    }

    /* Ramming the SUV is not a way to stop them, only a way to lose the car. */
    if (chase->player.invuln_timer <= 0.0f &&
        boxes_overlap(chase->player.x, chase->player.y, player_half_w,
                      player_half_h, chase->target.x, chase->target.y,
                      CHASE_SUV_WIDTH * 0.5f, CHASE_SUV_LENGTH * 0.5f))
    {
        float push = chase->player.x < chase->target.x ? -1.0f : 1.0f;
        hit_player(chase, push);
        chase->target.boost_timer = CHASE_TARGET_BOOST_TIME;
        return;
    }

    if (near_miss && chase->horn_timer <= 0.0f)
    {
        game_events_sound(&chase->events, SFX_CHASE_HORN);
        chase->horn_timer = CHASE_HORN_INTERVAL;
    }
}

/* ---- The player's car ------------------------------------------------ */

static void steer_and_clamp(Chase *chase, const Input *input, float dt)
{
    if (input->left && !input->right)
        chase->player.x -= CHASE_STEER_SPEED * dt;
    else if (input->right && !input->left)
        chase->player.x += CHASE_STEER_SPEED * dt;

    float half = CHASE_CAR_WIDTH * 0.5f;
    float low = half + CHASE_KERB_MARGIN;
    float high = CHASE_ROAD_WIDTH - half - CHASE_KERB_MARGIN;
    bool scraping = false;
    if (chase->player.x < low)
    {
        chase->player.x = low;
        scraping = true;
    }
    else if (chase->player.x > high)
    {
        chase->player.x = high;
        scraping = true;
    }

    if (scraping)
    {
        /* Kerbs bleed speed instead of costing integrity: the road edge should
         * punish sloppy lines without ending an otherwise clean run. */
        chase->player.speed -= CHASE_SCRAPE_DRAG * dt;
        chase->player.scrape_timer -= dt;
        if (chase->player.scrape_timer <= 0.0f)
        {
            game_events_sound(&chase->events, SFX_CHASE_TIRES);
            chase->player.scrape_timer = CHASE_SCRAPE_SOUND_INTERVAL;
        }
    }
    else
    {
        chase->player.scrape_timer = 0.0f;
    }
}

static void drive_player_car(Chase *chase, const Input *input, float dt)
{
    if (input->gas && !input->brake)
        chase->player.speed += CHASE_ACCEL * dt;
    else if (input->brake && !input->gas)
        chase->player.speed -= CHASE_BRAKE * dt;
    else
        chase->player.speed = approach(chase->player.speed, CHASE_CRUISE_SPEED,
                                       CHASE_COAST, dt);

    chase->player.speed = clampf(chase->player.speed, CHASE_MIN_SPEED,
                                 CHASE_MAX_SPEED);
    steer_and_clamp(chase, input, dt);
    chase->player.y += chase->player.speed * dt;
}

/* ---- The hunted SUV -------------------------------------------------- */

static bool target_lane_is_blocked(const Chase *chase, float x)
{
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        const ChaseCar *car = &chase->cars[i];
        if (!car->active)
            continue;
        float ahead = car->y - chase->target.y;
        if (ahead < -CHASE_SUV_LENGTH || ahead > CHASE_TARGET_LOOKAHEAD)
            continue;
        if (fabsf(car->x - x) < CHASE_LANE_WIDTH * 0.72f)
            return true;
    }
    return false;
}

static void pick_target_lane(Chase *chase)
{
    /* They favour the two lanes running with traffic and only cut into the
     * oncoming side when their own side is blocked. */
    for (int attempt = 0; attempt < CHASE_LANE_COUNT; ++attempt)
    {
        int lane = CHASE_FIRST_FORWARD_LANE +
                   rng_range(&chase->rng, CHASE_LANE_COUNT - CHASE_FIRST_FORWARD_LANE);
        float x = chase_lane_center(lane);
        if (!target_lane_is_blocked(chase, x))
        {
            chase->target.lane_target_x = x;
            return;
        }
    }
    for (int lane = 0; lane < CHASE_LANE_COUNT; ++lane)
    {
        float x = chase_lane_center(lane);
        if (!target_lane_is_blocked(chase, x))
        {
            chase->target.lane_target_x = x;
            return;
        }
    }
}

static void update_target(Chase *chase, float dt)
{
    ChaseTargetCar *target = &chase->target;

    target->lane_timer -= dt;
    if (target->lane_timer <= 0.0f ||
        target_lane_is_blocked(chase, target->lane_target_x))
    {
        pick_target_lane(chase);
        target->lane_timer = rng_between(&chase->rng, CHASE_TARGET_LANE_TIME_MIN,
                                         CHASE_TARGET_LANE_TIME_MAX);
    }
    target->x = approach(target->x, target->lane_target_x,
                         CHASE_TARGET_STEER_SPEED, dt);

    if (chase_gap(chase) < CHASE_MIN_GAP)
        target->boost_timer = CHASE_TARGET_BOOST_TIME;
    if (target->boost_timer > 0.0f)
        target->boost_timer -= dt;

    /*
     * Once they have made the tail they refuse to be caught: instead of a fixed
     * boost they hold whatever keeps Chuck at arm's length. Holding the
     * accelerator therefore settles into a stable tailgate rather than a
     * pointless collision with the car his wife is in.
     */
    float boost = 0.0f;
    if (target->boost_timer > 0.0f)
    {
        boost = fmaxf(CHASE_TARGET_BOOST,
                      chase->player.speed + 15.0f - CHASE_TARGET_SPEED);
        boost = fminf(boost, CHASE_MAX_SPEED + 20.0f - CHASE_TARGET_SPEED);
    }
    target->speed = CHASE_TARGET_SPEED + boost +
                    sinf(chase->time * 0.7f) * CHASE_TARGET_SPEED_SWING;
    target->y += target->speed * dt;

    /* The crew drive through anything they cannot get around, which
     * leaves the wreck spinning in the road for Chuck to deal with. */
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        ChaseCar *car = &chase->cars[i];
        if (!car->active || car->wreck_time > 0.0f)
            continue;
        if (boxes_overlap(target->x, target->y, CHASE_SUV_WIDTH * 0.5f,
                          CHASE_SUV_LENGTH * 0.5f, car->x, car->y,
                          car_half_x(car), car_half_y(car)))
        {
            wreck_car(car, car->x < target->x ? -1.0f : 1.0f);
            game_events_sound(&chase->events, SFX_CHASE_CRASH);
            game_events_camera_shake(&chase->events, 5.0f, 0.30f);
        }
    }
}

/* ---- Phases ---------------------------------------------------------- */

static void update_camera(Chase *chase, float lead, float dt)
{
    float desired = chase->player.y - lead;
    if (desired > chase->camera_y)
    {
        chase->camera_y = desired;
        return;
    }
    /* Only ever eases backwards, so braking never yanks the view forward. */
    chase->camera_y = approach(chase->camera_y, desired, 240.0f, dt);
}

static void update_engine_sound(Chase *chase, float dt)
{
    if (chase->horn_timer > 0.0f)
        chase->horn_timer -= dt;
    if (!chase->player.engine_running || chase->phase == CHASE_PHASE_DONE)
        return;

    chase->engine_timer -= dt;
    if (chase->engine_timer > 0.0f)
        return;

    /* The cached engine sample has a fixed pitch, so the revs are sold by how
     * often it retriggers: the faster the car, the tighter the loop. */
    float interval = CHASE_ENGINE_INTERVAL * CHASE_CRUISE_SPEED /
                     fmaxf(chase->player.speed, 90.0f);
    chase->engine_timer = clampf(interval, 0.78f, 1.55f);
    game_events_sound(&chase->events, SFX_CHASE_ENGINE);
}

/*
 * The press that means "get me past this". It is two inputs rather than one
 * because the pad and the keyboard cannot agree on a single button here: on a
 * pad A is the accelerator, so the shell reports the skip on Y (`use_door`),
 * while the keyboard's Space and Enter still arrive as an ordinary confirm.
 */
static bool skip_pressed(const Input *input)
{
    return input->confirm || input->use_door;
}

static void update_departure(Chase *chase, const Input *input, float dt)
{
    float previous = chase->phase_time;
    float now = previous + dt;
    chase->phase_time = now;

    if (crossed(previous, now, CHASE_DEPARTURE_SUV_DOOR))
    {
        game_events_sound(&chase->events, SFX_OPENING_CAR_DOOR);
        game_events_sound(&chase->events, SFX_OPENING_SUV_ENGINE);
    }
    if (crossed(previous, now, CHASE_DEPARTURE_CAR_DOOR))
        game_events_sound(&chase->events, SFX_OPENING_CAR_DOOR);
    if (crossed(previous, now, CHASE_DEPARTURE_IGNITION))
    {
        chase->player.engine_running = true;
        chase->engine_timer = 0.0f;
    }
    if (crossed(previous, now, CHASE_DEPARTURE_PULL_OUT))
        game_events_sound(&chase->events, SFX_CHASE_TIRES);

    if (now >= CHASE_DEPARTURE_SUV_START)
    {
        chase->target.speed = approach(chase->target.speed,
                                       CHASE_DEPARTURE_TARGET_SPEED,
                                       CHASE_DEPARTURE_TARGET_ACCEL, dt);
        chase->target.y += chase->target.speed * dt;
        chase->target.x = approach(chase->target.x, chase->target.lane_target_x,
                                   CHASE_TARGET_STEER_SPEED * 0.5f, dt);
    }

    if (now >= CHASE_DEPARTURE_PULL_OUT)
    {
        chase->player.speed = approach(chase->player.speed, CHASE_CRUISE_SPEED,
                                       CHASE_ACCEL, dt);
        chase->player.y += chase->player.speed * dt;
        chase->player.x = approach(chase->player.x,
                                   chase_lane_center(CHASE_LANE_COUNT - 1),
                                   CHASE_STEER_SPEED * 0.45f, dt);
    }

    if (now >= CHASE_DEPARTURE_DURATION || skip_pressed(input))
    {
        /*
         * Chuck's late start would otherwise leave the SUV a whole block ahead.
         * The handoff pulls it back to a fixed opening gap; it is off-screen at
         * this point, so the correction is never visible.
         */
        float gap = chase_gap(chase);
        if (gap > CHASE_START_GAP)
            chase->target.y = chase->player.y + CHASE_START_GAP;
        chase->player.speed = CHASE_CRUISE_SPEED;
        chase->player.engine_running = true;
        chase->player.integrity = CHASE_INTEGRITY;
        chase->player.invuln_timer = CHASE_HIT_INVULN;
        chase->target.speed = CHASE_TARGET_SPEED;
        begin_phase(chase, CHASE_PHASE_PURSUIT);
    }
}

static void begin_arrival(Chase *chase)
{
    chase->building_y = chase->player.y + CHASE_ARRIVAL_DISTANCE;
    chase->arrival_player_from_y = chase->player.y;
    chase->arrival_target_from_y = chase->target.y;

    /*
     * Clear the road beyond the SUV. Everything up there is off-screen at this
     * point, and leaving it running would let a car drive through the parked
     * SUV, or come to rest in the forecourt the cutscene opens on.
     */
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        if (chase->cars[i].y > chase->target.y)
            chase->cars[i].active = false;
    }

    begin_phase(chase, CHASE_PHASE_ARRIVAL);
    game_events_sound(&chase->events, SFX_LEVEL_CLEAR);
}

static void update_pursuit(Chase *chase, const Input *input, float dt)
{
    chase->phase_time += dt;
    chase->pursuit_time += dt;

    /* After a couple of failed attempts the drive stops insisting: confirm
     * jumps straight to the arrival. The prologue is a curtain-raiser, and a
     * curtain-raiser must never be the wall someone quits the game on. */
    if (skip_pressed(input) && chase->attempts >= CHASE_SKIP_AFTER_ATTEMPTS)
    {
        begin_arrival(chase);
        return;
    }

    if (chase->player.invuln_timer > 0.0f)
        chase->player.invuln_timer -= dt;

    drive_player_car(chase, input, dt);
    update_target(chase, dt);
    check_player_collisions(chase);

    if (chase->phase != CHASE_PHASE_PURSUIT)
        return; /* the collision above ended the attempt */

    if (chase_gap(chase) > CHASE_LOSE_GAP)
    {
        fail_pursuit(chase, CHASE_FAILURE_LOST);
        return;
    }

    if (chase->pursuit_time >= CHASE_PURSUIT_DURATION)
        begin_arrival(chase);
}

static void update_failed(Chase *chase, float dt)
{
    chase->phase_time += dt;
    chase->player.speed = approach(chase->player.speed, 0.0f, 420.0f, dt);
    chase->player.y += chase->player.speed * dt;
    chase->target.y += chase->target.speed * dt;

    if (chase->phase_time >= CHASE_FAILED_DURATION)
    {
        chase->attempts++;
        /*
         * A failure costs a beat of the drive, not the whole drive: the
         * pursuit resumes a stretch back from where it went wrong.
         *
         * But only while the drive is still asking to be driven. A player who
         * crashes more often than every `CHASE_FAIL_REWIND` seconds hands back
         * more road than they make, and the pursuit clock then never reaches
         * `CHASE_PURSUIT_DURATION` at all — measured, a pad held on the
         * throttle without steering never arrived in three minutes across five
         * seeds while an idle one always did. So the rewind stops at exactly
         * the attempt where the skip prompt appears: from there the drive
         * stops insisting on itself, and it stops taking itself back too, so
         * the pursuit clock only ever grows and the prologue always ends —
         * whether or not anybody presses the skip it is now offering.
         *
         * The clock and the road are handed back together, a beat of each, so
         * the two never disagree about how far along the route this attempt
         * is. That is what keeps the cordon thickening: the ring is read off
         * the block a junction is generated in, and a retry that kept the
         * clock but reset the road drove the last of the route through the
         * thinnest part of the ring.
         */
        float resume_time = chase->pursuit_time;
        float resume_y = chase->player.y;
        if (chase->attempts < CHASE_SKIP_AFTER_ATTEMPTS)
        {
            resume_time -= CHASE_FAIL_REWIND;
            resume_y -= CHASE_FAIL_REWIND * CHASE_CRUISE_SPEED;
        }
        if (resume_time < 0.0f)
            resume_time = 0.0f;
        if (resume_y < 0.0f)
            resume_y = 0.0f;
        reset_pursuit_layout(chase, resume_y);
        chase->pursuit_time = resume_time;
        begin_phase(chase, CHASE_PHASE_PURSUIT);
        game_events_sound(&chase->events, SFX_RESPAWN);
    }
}

/*
 * Both cars roll to a halt on a fixed profile rather than on a physical brake,
 * so the arrival always lands on its marks (and at a dead stop) no matter what
 * speed the pursuit ended at. Speed is read back from the movement so the HUD
 * and the engine sound still follow the deceleration.
 */
static void brake_to_marker(float *y, float *speed, float from_y, float stop_y,
                            float ease, float dt)
{
    float previous = *y;
    *y = from_y + (stop_y - from_y) * ease;
    *speed = dt > 0.0f ? (*y - previous) / dt : 0.0f;
    if (*speed < 0.0f)
        *speed = 0.0f;
}

static void update_arrival(Chase *chase, float dt)
{
    chase->phase_time += dt;

    float progress = clampf(chase->phase_time / CHASE_ARRIVAL_BRAKE_TIME,
                            0.0f, 1.0f);
    float remaining = 1.0f - progress;
    float ease = 1.0f - remaining * remaining * remaining;

    brake_to_marker(&chase->target.y, &chase->target.speed,
                    chase->arrival_target_from_y,
                    chase->building_y - CHASE_ARRIVAL_TARGET_STOP, ease, dt);
    chase->target.x = approach(chase->target.x, CHASE_KERB_X,
                               CHASE_TARGET_STEER_SPEED * 0.5f, dt);

    brake_to_marker(&chase->player.y, &chase->player.speed,
                    chase->arrival_player_from_y,
                    chase->building_y - CHASE_ARRIVAL_PLAYER_STOP, ease, dt);
    chase->player.x = approach(chase->player.x,
                               chase_lane_center(CHASE_LANE_COUNT - 1),
                               CHASE_STEER_SPEED * 0.45f, dt);

    if (chase->phase_time >= CHASE_ARRIVAL_DURATION)
        begin_phase(chase, CHASE_PHASE_DONE);
}

ChaseOutcome chase_update(Chase *chase, const Input *input, float dt)
{
    if (chase->phase == CHASE_PHASE_DONE)
        return CHASE_REACHED_BUILDING;

    chase->time += dt;

    switch (chase->phase)
    {
    case CHASE_PHASE_DEPARTURE:
        update_departure(chase, input, dt);
        break;
    case CHASE_PHASE_PURSUIT:
        update_pursuit(chase, input, dt);
        break;
    case CHASE_PHASE_FAILED:
        update_failed(chase, dt);
        break;
    case CHASE_PHASE_ARRIVAL:
        update_arrival(chase, dt);
        break;
    case CHASE_PHASE_DONE:
        break;
    }

    /*
     * The framing is part of the staging. The opening beat sits back so the SUV
     * can be watched driving away up the street, the pursuit pulls in for a
     * sense of speed, and the arrival opens up again so the destination is
     * fully in frame when the cutscene takes over.
     */
    float lead = CHASE_CAMERA_LEAD;
    if (chase->phase == CHASE_PHASE_DEPARTURE)
    {
        lead = CHASE_DEPARTURE_CAMERA_LEAD;
    }
    else if (chase->phase == CHASE_PHASE_ARRIVAL ||
             chase->phase == CHASE_PHASE_DONE)
    {
        float ease = clampf(chase->phase_time / 2.5f, 0.0f, 1.0f);
        lead = CHASE_CAMERA_LEAD +
               (CHASE_ARRIVAL_CAMERA_LEAD - CHASE_CAMERA_LEAD) * ease;
    }
    else if (chase->phase == CHASE_PHASE_PURSUIT && chase->attempts == 0)
    {
        float ease = clampf(chase->phase_time / 1.5f, 0.0f, 1.0f);
        lead = CHASE_DEPARTURE_CAMERA_LEAD +
               (CHASE_CAMERA_LEAD - CHASE_DEPARTURE_CAMERA_LEAD) * ease;
    }
    update_camera(chase, lead, dt);

    generate_road_ahead(chase);
    update_junctions(chase, dt);
    update_cars(chase, dt);
    update_engine_sound(chase, dt);

    return chase->phase == CHASE_PHASE_DONE ? CHASE_REACHED_BUILDING
                                            : CHASE_RUNNING;
}
