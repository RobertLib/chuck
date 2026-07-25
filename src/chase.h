#ifndef CHUCK_CHASE_H
#define CHUCK_CHASE_H

#include "game_config.h"
#include "game_event.h"
#include "player.h" /* Input */
#include "rng.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * The prologue pursuit: a top-down, forward-only drive through night traffic
 * that ends where the platformer begins. Like every other gameplay module this
 * one knows nothing about SDL or about `Game`; it advances a self-contained
 * simulation from an explicit seed and reports feedback through its own event
 * buffer, so the shell owns all audio and presentation.
 *
 * Road space: x runs across the road (0 .. CHASE_ROAD_WIDTH), y runs along the
 * driving direction and grows forward. Screen-up is forward.
 */

typedef enum
{
    /* The SUV pulls away from the kerb while Chuck runs to his car. */
    CHASE_PHASE_DEPARTURE,
    /* The player drives and has to stay on the SUV's tail. */
    CHASE_PHASE_PURSUIT,
    /* Wrecked or lost the trail; the pursuit restarts after a short beat. */
    CHASE_PHASE_FAILED,
    /* The SUV brakes at the building and both cars come to a stop. */
    CHASE_PHASE_ARRIVAL,
    CHASE_PHASE_DONE
} ChasePhase;

typedef enum
{
    CHASE_FAILURE_NONE,
    CHASE_FAILURE_WRECKED,
    CHASE_FAILURE_LOST
} ChaseFailure;

typedef enum
{
    CHASE_RUNNING,
    CHASE_REACHED_BUILDING
} ChaseOutcome;

typedef enum
{
    CHASE_CAR_TRAFFIC,  /* same direction as the pursuit, but slower */
    CHASE_CAR_ONCOMING, /* head-on, in the two left-hand lanes */
    CHASE_CAR_CROSSING  /* traverses an intersection sideways */
} ChaseCarKind;

typedef struct
{
    float x, y; /* centre of the car in road space */
    float vx, vy;
    ChaseCarKind kind;
    int variant;      /* seeded colour/shape choice, for the renderer */
    float wreck_time; /* seconds since it was hit; 0 while intact */
    bool active;
} ChaseCar;

typedef struct
{
    float y;                 /* centre of the cross street */
    float signal_offset;     /* phase offset of this junction's signal cycle */
    float cross_spawn_timer; /* countdown to the next car entering the junction */
    bool active;
} ChaseIntersection;

typedef struct
{
    float x, y;
    float speed;
    int integrity;       /* collisions left before the car is wrecked */
    float invuln_timer;  /* also the post-crash flash used by the renderer */
    float scrape_timer;  /* > 0 while grinding along a kerb */
    bool engine_running; /* false until Chuck starts the car */
} ChasePlayerCar;

typedef struct
{
    float x, y;
    float speed;
    float lane_target_x;
    float lane_timer;
    float boost_timer; /* they noticed the tail and pulled away */
} ChaseTargetCar;

typedef struct
{
    Rng rng;
    GameEventBuffer events;

    ChasePhase phase;
    ChaseFailure failure;
    float time;         /* seconds since the chase started; drives signals */
    float phase_time;   /* seconds spent in the current phase */
    float pursuit_time; /* seconds of driving completed in this attempt */
    int attempts;       /* how many times the pursuit has been restarted */

    ChasePlayerCar player;
    ChaseTargetCar target;

    ChaseCar cars[CHASE_MAX_CARS];
    ChaseIntersection intersections[CHASE_MAX_INTERSECTIONS];
    float generated_y; /* road content exists up to this y */
    float building_y;  /* > 0 once the destination has been placed */
    /* Where each car was when it started braking for the building. */
    float arrival_player_from_y;
    float arrival_target_from_y;

    float camera_y; /* road y at the bottom edge of the view */
    float engine_timer;
    float horn_timer;
} Chase;

void chase_init(Chase *chase, uint64_t seed);

/*
 * Advances one frame and returns CHASE_REACHED_BUILDING once the SUV has
 * parked at the building, which is the caller's cue to hand over to the
 * opening cutscene. Failing only costs the current attempt: the pursuit
 * restarts itself, so the prologue can always be completed.
 */
ChaseOutcome chase_update(Chase *chase, const Input *input, float dt);

/* Centre x of a lane, counted from the oncoming side of the road. */
float chase_lane_center(int lane);

/* Distance from the player's car to the SUV; negative while it is behind. */
float chase_gap(const Chase *chase);

/*
 * Signal state of a junction, shared by the simulation and the renderer so the
 * lights the player reads are the ones the cross traffic obeys.
 */
bool chase_cross_has_green(const ChaseIntersection *junction, float time);

/* Progress along the route, 0..1, used by the HUD. */
float chase_route_progress(const Chase *chase);

#endif /* CHUCK_CHASE_H */
