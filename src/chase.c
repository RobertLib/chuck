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

static bool runs_across(const ChaseCar *car)
{
    return car->kind == CHASE_CAR_CROSSING;
}

/* How fast a car is going the way it faces. */
static float car_speed(const ChaseCar *car)
{
    return (runs_across(car) ? car->vx : car->vy) * car->heading;
}

/* ---- Keeping out of each other --------------------------------------- */

/*
 * Everything on the road a car in traffic has to keep out of, addressed by one
 * index: the traffic slots, then the SUV, then Chuck's car.
 *
 * Chuck's car is only solid to traffic while nobody is driving it. During the
 * pursuit a car running into him is a crash and costs him integrity — that is
 * the drive — so traffic neither brakes for him nor stops short of him there.
 * Parked at the kerb before the departure, rolling to a halt after a failure
 * and braking onto its mark at the building it is scenery, and a car driving
 * through it is exactly what this whole section exists to stop.
 */
enum
{
    ROAD_SUV = CHASE_MAX_CARS,
    ROAD_PLAYER,
    ROAD_BOX_COUNT
};

typedef struct
{
    float x, y;   /* centre */
    float hw, hh; /* half extents */
    float vx, vy;
    bool present;
} RoadBox;

static bool player_car_is_scenery(const Chase *chase)
{
    return chase->phase != CHASE_PHASE_PURSUIT;
}

static RoadBox road_box(const Chase *chase, int index, bool with_player)
{
    RoadBox box = {0};
    if (index < CHASE_MAX_CARS)
    {
        const ChaseCar *car = &chase->cars[index];
        box.present = car->active;
        box.x = car->x;
        box.y = car->y;
        box.hw = car_half_x(car);
        box.hh = car_half_y(car);
        box.vx = car->vx;
        box.vy = car->vy;
    }
    else if (index == ROAD_SUV)
    {
        box.present = true;
        box.x = chase->target.x;
        box.y = chase->target.y;
        box.hw = CHASE_SUV_WIDTH * 0.5f;
        box.hh = CHASE_SUV_LENGTH * 0.5f;
        box.vy = chase->target.speed;
    }
    else
    {
        box.present = with_player;
        box.x = chase->player.x;
        box.y = chase->player.y;
        box.hw = CHASE_CAR_WIDTH * 0.5f;
        box.hh = CHASE_CAR_LENGTH * 0.5f;
        box.vy = chase->player.speed;
    }
    return box;
}

/*
 * Whether road box `index` is something the box at `self` has to keep out of.
 * Everything is, bar itself — except that Chuck's car shoves a wreck aside
 * rather than stopping for it (see `shove_wrecks_aside`). Stopping for one
 * could leave his car boxed in on the way onto its mark at the building, with
 * a wreck beside it and a car in front waiting for it to move: measured, one
 * drive in 384 parked him thirteen hundred pixels short, on the oncoming side
 * of a junction, for the whole of the beat.
 */
static bool road_box_blocks(const Chase *chase, int self, int index)
{
    if (index == self)
        return false;
    return !(self == ROAD_PLAYER && index < CHASE_MAX_CARS &&
             chase->cars[index].wreck_time > 0.0f);
}

/*
 * How deep two boxes have to be in each other before they count as already
 * overlapping rather than touching. A car stopped against another one is left
 * exactly touching, and a float's worth of error on that edge must still read
 * as touching — read as an overlap, the pair would be let through each other.
 */
static const float TOUCH_TOLERANCE = 0.05f;

/* The nearest thing in front of a box driving one way along one axis. */
typedef struct
{
    bool found;
    float gap;   /* bumper to bumper */
    float speed; /* how fast it is going the same way */
} Obstacle;

/*
 * `side_lo`..`side_hi` is the strip across the axis the mover sweeps: its own
 * width, widened toward the lane it is pulling into when it is changing lanes,
 * so a car mid-change is looking up both of them.
 *
 * Something the mover already overlaps is not in front of it, and neither this
 * nor `clear_step` has an opinion about it. Traffic never gets into that state
 * with itself; it is what a crash leaves, or the SUV or Chuck arriving in a car
 * sideways — and a pair that is already buried in each other and forbidden to
 * move toward each other's centres is a pair frozen that way. Letting them come
 * apart however they are moving is the only answer that ends the overlap.
 */
static Obstacle obstacle_ahead(const Chase *chase, int self, float x, float y,
                               float hw, float hh, bool across, float heading,
                               float side_lo, float side_hi)
{
    Obstacle best = {false, 0.0f, 0.0f};
    bool with_player = player_car_is_scenery(chase);
    float along = across ? x : y;
    float half_along = across ? hw : hh;
    for (int i = 0; i < ROAD_BOX_COUNT; ++i)
    {
        if (!road_box_blocks(chase, self, i))
            continue;
        RoadBox box = road_box(chase, i, with_player);
        if (!box.present)
            continue;
        float side = across ? box.y : box.x;
        float half_side = across ? box.hh : box.hw;
        if (side + half_side <= side_lo || side - half_side >= side_hi)
            continue;
        float ahead = ((across ? box.x : box.y) - along) * heading;
        if (ahead <= 0.0f)
            continue;
        float gap = ahead - half_along - (across ? box.hw : box.hh);
        if (gap < -TOUCH_TOLERANCE || gap > CHASE_TRAFFIC_LOOKAHEAD)
            continue;
        if (gap < 0.0f)
            gap = 0.0f;
        if (!best.found || gap < best.gap)
        {
            best.found = true;
            best.gap = gap;
            best.speed = (across ? box.vx : box.vy) * heading;
        }
    }
    return best;
}

/*
 * The speed that settles a car in at CHASE_TRAFFIC_GAP behind whatever is in
 * front of it: the leader's own speed plus whatever can still be shed at
 * CHASE_TRAFFIC_EASE in the room left over. Something coming the other way
 * counts as standing still — a car does not reverse out of the way.
 */
static float speed_behind(Obstacle obstacle, float cruise)
{
    if (!obstacle.found)
        return cruise;
    float leader = fmaxf(obstacle.speed, 0.0f);
    float room = obstacle.gap - CHASE_TRAFFIC_GAP;
    float speed = room > 0.0f
                      ? leader + sqrtf(2.0f * CHASE_TRAFFIC_EASE * room)
                      : leader * obstacle.gap / CHASE_TRAFFIC_GAP;
    return fminf(cruise, speed);
}

/*
 * How far a box may move along one axis: `step`, cut back to the point where
 * it would touch something. It is only ever cut toward nought, and it ignores
 * anything the box already overlaps, for the reason given above.
 */
static float clear_step(const Chase *chase, int self, float x, float y,
                        float hw, float hh, bool along_x, float step)
{
    if (step == 0.0f)
        return 0.0f;
    bool with_player = player_car_is_scenery(chase);
    for (int i = 0; i < ROAD_BOX_COUNT; ++i)
    {
        if (!road_box_blocks(chase, self, i))
            continue;
        RoadBox box = road_box(chase, i, with_player);
        if (!box.present)
            continue;
        float side_gap = along_x ? fabsf(box.y - y) - (hh + box.hh)
                                 : fabsf(box.x - x) - (hw + box.hw);
        if (side_gap >= 0.0f)
            continue;
        float centre = along_x ? box.x - x : box.y - y;
        if (centre * step <= 0.0f)
            continue;
        float room = fabsf(centre) - (along_x ? hw + box.hw : hh + box.hh);
        if (room < -TOUCH_TOLERANCE)
            continue;
        if (room < 0.0f)
            room = 0.0f;
        if (fabsf(step) > room)
            step = step > 0.0f ? room : -room;
    }
    return step;
}

/*
 * Traffic is only fair if the player always has somewhere to go, so a lane slot
 * is refused when it would put more than CHASE_MAX_CARS_ABREAST cars across the
 * same stretch of road, or drop a car on top of anything already on it.
 *
 * Anything means anything. A wreck can be anywhere by the time the next block
 * is laid down — the SUV carries what it rams up the road at its own speed, a
 * cross-street car included, well clear of the junction it was on — and so can
 * the SUV itself or Chuck's car, both parked across the kerb lane while the
 * first block is being laid down. Only cars in lanes count toward abreast.
 */
static bool lane_slot_is_free(const Chase *chase, float x, float y)
{
    int abreast = 0;
    for (int i = 0; i < ROAD_BOX_COUNT; ++i)
    {
        RoadBox box = road_box(chase, i, true);
        if (!box.present)
            continue;
        if (fabsf(box.y - y) < box.hh + CHASE_CAR_LENGTH * 0.5f +
                                   CHASE_TRAFFIC_GAP &&
            fabsf(box.x - x) < box.hw + CHASE_CAR_WIDTH * 0.5f)
            return false;
        if (i >= CHASE_MAX_CARS || runs_across(&chase->cars[i]))
            continue;
        if (fabsf(box.y - y) > CHASE_CAR_LENGTH * 1.7f)
            continue;
        if (fabsf(box.x - x) < CHASE_LANE_WIDTH * 0.9f)
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
        car->lane_x = x;
        car->heading = oncoming ? -1.0f : 1.0f;
        car->cruise = oncoming
                          ? rng_between(&chase->rng, CHASE_ONCOMING_SPEED_MIN,
                                        CHASE_ONCOMING_SPEED_MAX)
                          : rng_between(&chase->rng, CHASE_TRAFFIC_SPEED_MIN,
                                        CHASE_TRAFFIC_SPEED_MAX);
        car->vx = 0.0f;
        car->vy = car->heading * car->cruise;
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

/*
 * Narrows [t0, t1] to the times at which two intervals `offset` apart and
 * closing at `rate` lie within `reach` of each other, and says whether any are
 * left. One axis of a swept-box test.
 */
static bool sweep_axis(float offset, float rate, float reach, float *t0,
                       float *t1)
{
    if (fabsf(rate) < 1e-4f)
        return fabsf(offset) < reach;
    float enter = (-reach - offset) / rate;
    float leave = (reach - offset) / rate;
    if (enter > leave)
    {
        float swap = enter;
        enter = leave;
        leave = swap;
    }
    if (enter > *t0)
        *t0 = enter;
    if (leave < *t1)
        *t1 = leave;
    return *t0 < *t1;
}

/* Whether two boxes held at their velocities come within `margin` of each
 * other at any time from now until `horizon`. */
static bool boxes_meet(RoadBox a, RoadBox b, float margin, float horizon)
{
    float t0 = 0.0f;
    float t1 = horizon;
    return sweep_axis(b.x - a.x, b.vx - a.vx, a.hw + b.hw + margin, &t0, &t1) &&
           sweep_axis(b.y - a.y, b.vy - a.vy, a.hh + b.hh + margin, &t0, &t1);
}

/*
 * Whether a car pulling into the junction now would get all the way across
 * without meeting anything on the road.
 *
 * It is asked once, at the kerb, and then never again, and that is the whole
 * design. A car that could stop halfway over for a car in a lane would be
 * standing across the lane behind it, and four of them waiting on each other
 * is a junction locked solid for the rest of the drive. So cross traffic
 * decides before it pulls out, and once it is out the traffic in the lanes is
 * the side that gives way — for a car it can see sweeping across in front of
 * it, which is always about to be gone.
 *
 * The answer can only be as good as the guess about everybody else, so each
 * car in a lane is taken at its word and then twice more: as though it put its
 * foot down to its cruising speed, and as though it slowed to half of what it
 * is doing. A wreck is taken as sliding and as stopped. Without the two extra
 * guesses a car pulling out had to brake for a car in a lane halfway over
 * twelve times as often, because traffic in a lane speeds up and slows down.
 *
 * Neither the SUV nor Chuck is asked about. The crew run the red at twice the
 * pace of anything else on the road and drive through whatever is in their
 * way — that is the ram in `update_target` — and cross traffic cutting across
 * Chuck's path is the hazard the junction is there to be.
 */
static bool crossing_path_is_clear(const Chase *chase, const ChaseCar *car)
{
    RoadBox mover = {car->x, car->y, car_half_x(car), car_half_y(car),
                     car->vx, car->vy, true};
    float horizon = (CHASE_ROAD_WIDTH + CHASE_CAR_LENGTH * 2.0f) / fabsf(car->vx);
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        RoadBox box = road_box(chase, i, false);
        if (!box.present)
            continue;
        if (boxes_meet(mover, box, CHASE_CROSS_CLEARANCE, horizon))
            return false;
        const ChaseCar *other = &chase->cars[i];
        RoadBox guess = box;
        if (other->wreck_time > 0.0f)
        {
            guess.vx = 0.0f;
            guess.vy = 0.0f;
            if (boxes_meet(mover, guess, CHASE_CROSS_CLEARANCE, horizon))
                return false;
        }
        else if (!runs_across(other))
        {
            guess.vy = other->heading * other->cruise;
            if (boxes_meet(mover, guess, CHASE_CROSS_CLEARANCE, horizon))
                return false;
            guess.vy = box.vy * 0.5f;
            if (boxes_meet(mover, guess, CHASE_CROSS_CLEARANCE, horizon))
                return false;
        }
    }
    return true;
}

/* Returns false when nobody pulled out, which is the junction's cue to ask
 * again shortly instead of waiting out a whole gap in the flow. */
static bool spawn_crossing_car(Chase *chase, ChaseIntersection *junction)
{
    ChaseCar *slot = free_car_slot(chase);
    if (slot == NULL)
        return false;

    bool eastbound = rng_range(&chase->rng, 2) == 0;
    float speed = rng_between(&chase->rng, CHASE_CROSS_SPEED_MIN,
                              CHASE_CROSS_SPEED_MAX);
    ChaseCar car;
    memset(&car, 0, sizeof(car));
    car.active = true;
    car.kind = CHASE_CAR_CROSSING;
    car.heading = eastbound ? 1.0f : -1.0f;
    /* Cross traffic keeps right as well, so the two directions never share a
     * line and the player can read which way a car is travelling. */
    car.y = junction->y + (eastbound ? -CHASE_CROSS_LANE_OFFSET
                                     : CHASE_CROSS_LANE_OFFSET);
    car.x = eastbound ? -CHASE_CAR_LENGTH : CHASE_ROAD_WIDTH + CHASE_CAR_LENGTH;
    car.lane_x = car.x;
    car.variant = rng_range(&chase->rng, 4);

    /* Never quicker than the car it is following out, or it would have to
     * stop behind it halfway over — the one thing the check below rules out
     * for everybody else. A leader already slowing is left to that check. */
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        const ChaseCar *other = &chase->cars[i];
        if (!other->active || other->wreck_time > 0.0f || !runs_across(other) ||
            other->heading * car.heading < 0.0f ||
            fabsf(other->y - car.y) > 1.0f)
            continue;
        float leader = car_speed(other);
        if (leader >= CHASE_CROSS_SPEED_MIN && leader < speed)
            speed = leader;
    }
    car.cruise = speed;
    car.vx = car.heading * speed;
    car.vy = 0.0f;

    if (!crossing_path_is_clear(chase, &car))
        return false;
    *slot = car;
    return true;
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
            if (spawn_crossing_car(chase, junction))
                junction->cross_spawn_timer = rng_between(&chase->rng,
                                                          CHASE_CROSS_GAP_MIN,
                                                          CHASE_CROSS_GAP_MAX);
            else
                junction->cross_spawn_timer = CHASE_CROSS_RETRY;
        }
    }
}

/*
 * Whether a car in a lane can pull into the lane at `lane_x` from where it is:
 * nothing alongside or just in front of it there, and nothing coming up that
 * lane from behind that would be on it within CHASE_TRAFFIC_MERGE_LOOK. This
 * is the one question traffic asks about Chuck's car even in the middle of the
 * pursuit — a car that pulls out into his path is a crash the player could not
 * have seen coming, which is a different thing from one he drove into.
 */
static bool lane_has_room(const Chase *chase, int self, float lane_x)
{
    const ChaseCar *car = &chase->cars[self];
    float half_w = CHASE_CAR_WIDTH * 0.5f;
    float half_h = car_half_y(car);
    float own_speed = car_speed(car);
    for (int i = 0; i < ROAD_BOX_COUNT; ++i)
    {
        if (i == self)
            continue;
        RoadBox box = road_box(chase, i, true);
        if (!box.present || fabsf(box.x - lane_x) >= box.hw + half_w)
            continue;
        float ahead = (box.y - car->y) * car->heading;
        float bumper = fabsf(ahead) - box.hh - half_h;
        if (ahead >= 0.0f)
        {
            if (bumper < CHASE_TRAFFIC_GAP * 2.0f)
                return false;
            continue;
        }
        float closing = box.vy * car->heading - own_speed;
        if (bumper < CHASE_TRAFFIC_GAP +
                         fmaxf(closing, 0.0f) * CHASE_TRAFFIC_MERGE_LOOK)
            return false;
    }
    return true;
}

/*
 * A car held up by something that has stopped pulls into the other lane on its
 * own side of the road, if there is room. It never crosses the centre line:
 * the two lanes each way are the only lanes each way, and a car on the wrong
 * side is a head-on the player did not cause.
 */
static void consider_changing_lane(Chase *chase, int index, Obstacle ahead)
{
    ChaseCar *car = &chase->cars[index];
    if (fabsf(car->x - car->lane_x) > 0.0f)
        return;
    int lane = (int)(car->lane_x / CHASE_LANE_WIDTH);
    /* Pulling out of the kerb lane for the two cars pulling into it: once the
     * drive is arriving, both of them are braking onto marks in that lane, and
     * a car still in it ahead of Chuck is a car he parks behind instead. */
    bool clearing_the_kerb = chase->phase == CHASE_PHASE_ARRIVAL &&
                             lane == CHASE_LANE_COUNT - 1 &&
                             car->y > chase->player.y;
    if (!clearing_the_kerb &&
        (!ahead.found || ahead.gap > CHASE_TRAFFIC_MERGE_RANGE ||
         ahead.speed > CHASE_TRAFFIC_MERGE_BELOW))
        return;

    bool forward = lane >= CHASE_FIRST_FORWARD_LANE;
    int first = forward ? CHASE_FIRST_FORWARD_LANE : 0;
    int last = forward ? CHASE_LANE_COUNT - 1 : CHASE_FIRST_FORWARD_LANE - 1;
    for (int other = first; other <= last; ++other)
    {
        float x = chase_lane_center(other);
        if (other != lane && lane_has_room(chase, index, x))
        {
            car->lane_x = x;
            return;
        }
    }
}

static void drive_car(Chase *chase, int index, float dt)
{
    ChaseCar *car = &chase->cars[index];
    bool across = runs_across(car);
    float hw = car_half_x(car);
    float hh = car_half_y(car);
    float side = across ? car->y : car->x;
    float half_side = across ? hh : hw;
    float bound = across ? car->y : car->lane_x;
    Obstacle ahead = obstacle_ahead(chase, index, car->x, car->y, hw, hh,
                                    across, car->heading,
                                    fminf(side, bound) - half_side,
                                    fmaxf(side, bound) + half_side);
    if (!across)
        consider_changing_lane(chase, index, ahead);

    float speed = car_speed(car);
    float wanted = speed_behind(ahead, car->cruise);
    speed = approach(speed, wanted,
                     wanted < speed ? CHASE_TRAFFIC_BRAKE : CHASE_TRAFFIC_ACCEL,
                     dt);
    float step = clear_step(chase, index, car->x, car->y, hw, hh, across,
                            speed * car->heading * dt);
    if (dt > 0.0f && fabsf(step) < speed * dt)
        speed = fabsf(step) / dt;

    if (across)
    {
        car->x += step;
        car->vx = speed * car->heading;
        return;
    }
    car->y += step;
    car->vy = speed * car->heading;
    float drift = approach(car->x, car->lane_x, CHASE_TRAFFIC_MERGE_SPEED, dt) -
                  car->x;
    drift = clear_step(chase, index, car->x, car->y, hw, hh, true, drift);
    car->x += drift;
    car->vx = dt > 0.0f ? drift / dt : 0.0f;
}

/* A wrecked car slews to a halt and stops being a threat — but it is still a
 * car, and it stops against anything it slides into. */
static void slide_wreck(Chase *chase, int index, float dt)
{
    ChaseCar *car = &chase->cars[index];
    car->wreck_time += dt;
    car->vy = approach(car->vy, 0.0f, 260.0f, dt);
    car->vx = approach(car->vx, 0.0f, 190.0f, dt);
    float hw = car_half_x(car);
    float hh = car_half_y(car);
    float dx = clear_step(chase, index, car->x, car->y, hw, hh, true,
                          car->vx * dt);
    if (dt > 0.0f && fabsf(dx) < fabsf(car->vx * dt))
        car->vx = dx / dt;
    car->x += dx;
    float dy = clear_step(chase, index, car->x, car->y, hw, hh, false,
                          car->vy * dt);
    if (dt > 0.0f && fabsf(dy) < fabsf(car->vy * dt))
        car->vy = dy / dt;
    car->y += dy;
}

static void update_cars(Chase *chase, float dt)
{
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        ChaseCar *car = &chase->cars[i];
        if (!car->active)
            continue;

        if (car->wreck_time > 0.0f)
            slide_wreck(chase, i, dt);
        else
            drive_car(chase, i, dt);

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

/*
 * Shoves a car that something has driven into out of that thing's road: pushed
 * clear along whichever way it is buried least — ahead of the bumper if it was
 * in front, aside toward whichever side it was already on otherwise — and
 * carried along at the pusher's speed. It is only ever shoved as far as the
 * room around it allows, so a wreck can be bulldozed out of one car's way but
 * never buried in the next one.
 */
static void shove_car(Chase *chase, int index, float x, float y, float hw,
                      float hh, float speed)
{
    ChaseCar *car = &chase->cars[index];
    float dx = car->x - x;
    float dy = car->y - y;
    float into_x = hw + car_half_x(car) - fabsf(dx);
    float into_y = hh + car_half_y(car) - fabsf(dy);
    if (into_x <= 0.0f || into_y <= 0.0f)
        return;
    float side = dx < 0.0f ? -1.0f : 1.0f;
    car->vx = side * CHASE_WRECK_DRIFT;
    if (dy > 0.0f && into_y <= into_x)
    {
        car->vy = fmaxf(car->vy, speed);
        car->y += clear_step(chase, index, car->x, car->y, car_half_x(car),
                             car_half_y(car), false, into_y);
    }
    else
    {
        car->x += clear_step(chase, index, car->x, car->y, car_half_x(car),
                             car_half_y(car), true, side * into_x);
    }
}

/*
 * A wreck is no threat to Chuck — the collision above skips it on purpose, so
 * that one crash cannot chain into the next — but it is still a car, and his
 * own is shoved through it rather than drawn over it. The crash that made it
 * is the usual case: he is slowed to CHASE_CRASH_SPEED and comes straight back
 * up to pace, and the car he hit is braking to a halt in front of him, so he
 * used to drive the whole length of it a second later.
 */
static void shove_wrecks_aside(Chase *chase)
{
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        if (chase->cars[i].active && chase->cars[i].wreck_time > 0.0f)
            shove_car(chase, i, chase->player.x, chase->player.y,
                      CHASE_CAR_WIDTH * 0.5f, CHASE_CAR_LENGTH * 0.5f,
                      chase->player.speed);
    }
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

    /*
     * The crew drive through anything they cannot get around, which leaves
     * the wreck spinning in the road for Chuck to deal with.
     *
     * Through, not over: a car they hit is shoved out of their road, and a
     * wreck they run into again is shoved again. Left where the crash found
     * it, a car rear-ended at a hundred pixels a second more than its own pace
     * was passed through end to end — a second and more of SUV drawn on top of
     * a car it had supposedly just destroyed.
     */
    const float hw = CHASE_SUV_WIDTH * 0.5f;
    const float hh = CHASE_SUV_LENGTH * 0.5f;
    for (int i = 0; i < CHASE_MAX_CARS; ++i)
    {
        ChaseCar *car = &chase->cars[i];
        if (!car->active)
            continue;
        if (car->wreck_time <= 0.0f)
        {
            if (!boxes_overlap(target->x, target->y, hw, hh, car->x, car->y,
                               car_half_x(car), car_half_y(car)))
                continue;
            wreck_car(car, car->x < target->x ? -1.0f : 1.0f);
            game_events_sound(&chase->events, SFX_CHASE_CRASH);
            game_events_camera_shake(&chase->events, 5.0f, 0.30f);
        }
        shove_car(chase, i, target->x, target->y, hw, hh, target->speed);
    }
}

/*
 * The SUV whenever it is not being chased — pulling away from the kerb, or
 * driving off after Chuck has lost it — drives like everybody else: it eases in
 * behind whatever is in front of it and steers only into room, rather than
 * through a car it has no reason yet to ram.
 */
static void target_drive_in_traffic(Chase *chase, float cruise, float accel,
                                    float steer, float dt)
{
    ChaseTargetCar *target = &chase->target;
    const float hw = CHASE_SUV_WIDTH * 0.5f;
    const float hh = CHASE_SUV_LENGTH * 0.5f;
    Obstacle ahead = obstacle_ahead(chase, ROAD_SUV, target->x, target->y, hw,
                                    hh, false, 1.0f,
                                    fminf(target->x, target->lane_target_x) - hw,
                                    fmaxf(target->x, target->lane_target_x) + hw);
    /* Braking is for something in the way; easing back to its own pace after
     * the pursuit is only ever the ordinary rate. */
    float wanted = speed_behind(ahead, cruise);
    bool braking = ahead.found && wanted < target->speed;
    target->speed = approach(target->speed, wanted,
                             braking ? CHASE_TRAFFIC_BRAKE : accel, dt);
    float step = clear_step(chase, ROAD_SUV, target->x, target->y, hw, hh,
                            false, target->speed * dt);
    if (dt > 0.0f && step < target->speed * dt)
        target->speed = step / dt;
    target->y += step;
    float drift = approach(target->x, target->lane_target_x, steer, dt) -
                  target->x;
    target->x += clear_step(chase, ROAD_SUV, target->x, target->y, hw, hh, true,
                            drift);
}

/*
 * Chuck's car whenever the player is not the one driving it: pulling out from
 * the kerb, rolling to a halt after a failed attempt, and braking onto its
 * mark at the building. Each of those is scripted, and a script knows nothing
 * about the car that happens to be in front of it — so it is driven the way
 * the SUV is off the chase, easing in behind whatever is there and steering
 * only into room. `speed` is what the script asks for; what the car actually
 * did is written back, so the HUD and the engine note follow it.
 */
static void player_drive_in_traffic(Chase *chase, float speed, float lane_x,
                                    float steer, float dt)
{
    ChasePlayerCar *player = &chase->player;
    const float hw = CHASE_CAR_WIDTH * 0.5f;
    const float hh = CHASE_CAR_LENGTH * 0.5f;
    Obstacle ahead = obstacle_ahead(chase, ROAD_PLAYER, player->x, player->y,
                                    hw, hh, false, 1.0f,
                                    fminf(player->x, lane_x) - hw,
                                    fmaxf(player->x, lane_x) + hw);
    float step = clear_step(chase, ROAD_PLAYER, player->x, player->y, hw, hh,
                            false, speed_behind(ahead, speed) * dt);
    player->y += step;
    player->speed = dt > 0.0f ? step / dt : 0.0f;
    float drift = approach(player->x, lane_x, steer, dt) - player->x;
    player->x += clear_step(chase, ROAD_PLAYER, player->x, player->y, hw, hh,
                            true, drift);
    shove_wrecks_aside(chase);
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
        target_drive_in_traffic(chase, CHASE_DEPARTURE_TARGET_SPEED,
                                CHASE_DEPARTURE_TARGET_ACCEL,
                                CHASE_TARGET_STEER_SPEED * 0.5f, dt);

    if (now >= CHASE_DEPARTURE_PULL_OUT)
        player_drive_in_traffic(chase,
                                approach(chase->player.speed,
                                         CHASE_CRUISE_SPEED, CHASE_ACCEL, dt),
                                chase_lane_center(CHASE_LANE_COUNT - 1),
                                CHASE_STEER_SPEED * 0.45f, dt);

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
    shove_wrecks_aside(chase);

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
    player_drive_in_traffic(chase,
                            approach(chase->player.speed, 0.0f, 420.0f, dt),
                            chase->player.x, 0.0f, dt);
    target_drive_in_traffic(chase, CHASE_TARGET_SPEED,
                            CHASE_DEPARTURE_TARGET_ACCEL,
                            CHASE_TARGET_STEER_SPEED, dt);

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
 * so the arrival lands on its marks (and at a dead stop) no matter what speed
 * the pursuit ended at — the SUV always, Chuck unless traffic holds him up,
 * which `update_arrival` explains. Speed is read back from the movement so the
 * HUD and the engine sound still follow the deceleration.
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
    float pull_in = approach(chase->target.x, CHASE_KERB_X,
                             CHASE_TARGET_STEER_SPEED * 0.5f, dt) -
                    chase->target.x;
    chase->target.x += clear_step(chase, ROAD_SUV, chase->target.x,
                                  chase->target.y, CHASE_SUV_WIDTH * 0.5f,
                                  CHASE_SUV_LENGTH * 0.5f, true, pull_in);

    /*
     * Chuck's car follows the same profile, but it is the one of the two with
     * traffic in front of it — the road beyond the SUV was cleared as the beat
     * began, the road between them was on screen and could not be. So the
     * profile is what it asks for, not where it is put: held up behind a car it
     * makes the ground back afterwards no quicker than the profile's own pace
     * or his top speed, whichever is greater, and traffic pulls out of the
     * kerb lane ahead of him (`consider_changing_lane`) to let him in. It used
     * to be put there, through whatever was in the way. Measured over 384
     * drives, 381 now land on the mark; the other three stop behind a car
     * with a car alongside it and nowhere to pull out to.
     */
    float stop_y = chase->building_y - CHASE_ARRIVAL_PLAYER_STOP;
    float on_profile = chase->arrival_player_from_y +
                       (stop_y - chase->arrival_player_from_y) * ease;
    float profile_speed = (stop_y - chase->arrival_player_from_y) * 3.0f *
                          remaining * remaining / CHASE_ARRIVAL_BRAKE_TIME;
    float asked = dt > 0.0f ? (on_profile - chase->player.y) / dt : 0.0f;
    player_drive_in_traffic(chase,
                            clampf(asked, 0.0f,
                                   fmaxf(profile_speed, CHASE_MAX_SPEED)),
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
