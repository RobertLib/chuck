/*
 * Chuck's skeleton. See [chuck_pose.h](chuck_pose.h) for what it is for and why
 * it links no SDL.
 */

#include "chuck_pose.h"

#include <math.h>

#define CHUCK_TAU 6.28318531f

/*
 * One way of covering ground, as the handful of numbers a gait is actually made
 * of. The stride is not among them on purpose — see `chuck_gait_stride`.
 */
typedef struct
{
    float duty;             /* share of the cycle a foot is planted */
    float reach_front;      /* ankle ahead of the hip at heel strike */
    float reach_back;       /* ankle behind the hip at toe-off */
    float heel_rise;        /* ankle lift as the foot rolls onto the toe */
    float kick;             /* how high the ankle comes up in the swing */
    float kick_at;          /* where in the swing that peak is, 0..1 */
    float strike;           /* toe raised as the heel comes down */
    float hip_drop;         /* hips below standing height, on average */
    float bounce;           /* +: lowest at mid-stance, -: highest there */
    float lean;             /* neck ahead of the hips */
    float arm_swing;        /* radians either side of hanging */
    float arm_bend;         /* elbow flexion through the swing */
    float arm_bend_forward; /* and the extra it gains on the way forward */
    float arm_lag;          /* of a cycle, the arms trailing the legs */
    float tail;             /* headband */
    float roll;             /* share of the heel-to-toe roll the sole shows */
} GaitShape;

/*
 * The gaits, in `ChuckGait` order.
 *
 * The walk keeps both feet down for an eighth of every step and rides highest
 * over the planted leg, which is what a walk is: a body vaulting over a stiff
 * leg. It is a long, driving stride rather than an amble, because the two
 * things that walk — Chuck hauling a body, and Chuck crossing the roof to her —
 * go at sixty to a hundred pixels a second, and a short stride at that pace is
 * eight steps a second: a man scurrying.
 *
 * The run is the other way up — each foot is down for under a fifth of the
 * cycle, the body is lowest while the knee takes the landing and highest in the
 * flight between footfalls, and the arms are carried bent and swung from the
 * shoulder. A run with no flight in it is a fast walk, and a fast walk is what
 * the old cycle was.
 *
 * The long flight is what sets the pace, because the stride is the planted
 * sweep divided by the share of the cycle it is planted for: at a quarter it
 * came to three and a half cycles a second at the sector's running speed —
 * seven steps a second, which read as a man pedalling rather than running, and
 * it read that way the first time anybody watched it. Under a fifth brings it to a
 * little over two. The heel kick, the arm pump and the lean were turned down
 * with it, because a high kick and a hard pump on top of that cadence were the
 * other half of what made it look frantic.
 *
 * The plain run is the film's, and keeps what makes the run a run and nothing a
 * viewer reads as effort. Its timing is the run's — a foot down for a sixth of
 * the cycle and a stride a little longer than the run's, so at any speed the
 * film asks for the cadence is if anything slower — because a two-beat step
 * with a foot planted half the time would have to take ten steps a second to
 * keep up with him on the slowest of the film's runs, which is the pedalling
 * the paragraph above ended. Everything a viewer can see is held under his own
 * walk instead: the swinging foot comes up no higher than a walking step does
 * rather than folding up behind the knee, the soles stay flat because the
 * cast's shoe is a block that never tips, the spine stands as it does at rest,
 * the hips travel less than walking, the arms hang and swing a little from the
 * shoulder with no pump in the elbow, and the headband hangs. The reach is
 * shorter at the back than the run's, because a sole that does not roll cannot
 * push off from as far behind, and the hips sit a little lower so the stance
 * still covers the ground the flight needs.
 *
 * Unsized, and held to the enum below, for the reason AGENTS.md gives about
 * every table in this tree whose length is a claim.
 */
static const GaitShape GAIT_SHAPES[] = {
    /* CHUCK_GAIT_WALK */
    {0.56f, 5.4f, 5.8f, 1.2f, 2.3f, 0.40f, 0.60f, 1.20f, -0.80f, 0.50f,
     0.45f, 0.28f, 0.32f, 0.04f, 0.9f, 1.0f},
    /* CHUCK_GAIT_RUN */
    {0.18f, 3.6f, 6.8f, 2.0f, 4.8f, 0.40f, 0.50f, 0.60f, 1.30f, 1.30f,
     0.75f, 1.05f, 0.30f, 0.03f, 2.2f, 1.0f},
    /* CHUCK_GAIT_PLAIN_RUN */
    {0.16f, 4.2f, 6.0f, 1.6f, 2.2f, 0.50f, 0.00f, 0.80f, 0.60f, 0.25f,
     0.26f, 0.30f, 0.08f, 0.03f, 0.5f, 0.0f},
};
_Static_assert(sizeof(GAIT_SHAPES) / sizeof(GAIT_SHAPES[0]) == CHUCK_GAIT_COUNT,
               "one GaitShape per ChuckGait");

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float wrap01(float v)
{
    return v - floorf(v);
}

static float smooth(float t)
{
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static const GaitShape *gait_shape(ChuckGait gait)
{
    return &GAIT_SHAPES[gait];
}

float chuck_gait_stride(ChuckGait gait)
{
    const GaitShape *g = gait_shape(gait);
    /* The planted foot sweeps reach_front + reach_back under the hips in
       `duty` of a cycle, so the ground has to pass under him that far in the
       same time. Any other stride and the foot is dragged. */
    return (g->reach_front + g->reach_back) / g->duty;
}

float chuck_gait_duty(ChuckGait gait)
{
    return gait_shape(gait)->duty;
}

float chuck_gait_cycle(ChuckGait gait, float distance)
{
    return wrap01(distance / chuck_gait_stride(gait));
}

ChuckPoint chuck_joint(ChuckPoint a, ChuckPoint *b, float bone_a, float bone_b,
                       float bend)
{
    float dx = b->x - a.x;
    float dy = b->y - a.y;
    float d = sqrtf(dx * dx + dy * dy);
    float reach = bone_a + bone_b;
    float shortest = fabsf(bone_a - bone_b) + 0.01f;

    if (d < 0.0001f)
    {
        /* Folded flat on itself: point the end straight down and try again. */
        dx = 0.0f;
        dy = 1.0f;
        d = 1.0f;
        b->x = a.x;
        b->y = a.y + shortest;
    }
    float ux = dx / d;
    float uy = dy / d;
    if (d > reach)
        d = reach;
    else if (d < shortest)
        d = shortest;
    b->x = a.x + ux * d;
    b->y = a.y + uy * d;

    float along = (bone_a * bone_a - bone_b * bone_b + d * d) / (2.0f * d);
    float h = sqrtf(fmaxf(0.0f, bone_a * bone_a - along * along));
    return (ChuckPoint){a.x + ux * along - uy * h * bend,
                        a.y + uy * along + ux * h * bend};
}

/*
 * Where one foot is, `cycle` into its own stride: planted for the first `duty`
 * of it, swinging for the rest.
 *
 * Planted, the ankle tracks straight back under the hips at a constant rate —
 * the rate `chuck_gait_stride` is derived from — and rolls up onto the toe over
 * the back half, which is the heel rise that lets a short leg push off from
 * further behind than it could reach flat-footed. Swinging, it peels off that
 * toe, comes up to the top of the swing and reaches forward to put the heel down
 * with the toe raised. Every quantity is continuous across both seams, so there
 * is no frame in the cycle where a foot jumps.
 */
static void gait_foot(const GaitShape *g, float cycle, float *dx, float *lift,
                      float *pitch)
{
    float c = wrap01(cycle);
    float sweep = g->reach_front + g->reach_back;

    if (c < g->duty)
    {
        float u = c / g->duty;
        float roll = smooth((u - 0.5f) / 0.5f);
        *dx = g->reach_front - sweep * u;
        *lift = g->heel_rise * roll;
        *pitch = roll - g->strike * (1.0f - smooth(u / 0.25f));
        return;
    }

    float u = (c - g->duty) / (1.0f - g->duty);
    /* The lift peaks at `kick_at` of the swing: the parameter is warped so that
       halfway through the sine lands there. */
    float warp = powf(u, logf(0.5f) / logf(g->kick_at));
    float peel = (1.0f - u) * (1.0f - u);
    *dx = -g->reach_back + sweep * smooth(u);
    *lift = g->heel_rise * peel + g->kick * sinf(3.14159265f * warp);
    *pitch = (1.0f - smooth(u / 0.4f)) -
             g->strike * smooth((u - 0.6f) / 0.4f);
}

void chuck_pose_stand(ChuckPose *pose, float breath)
{
    /* A little sat into the hips, feet a shoe's length apart and the far one
       back, the hands hanging with the elbows barely broken: the stance of a
       man waiting to move rather than a man standing to attention. */
    pose->pelvis = (ChuckPoint){CHUCK_ROOT_X - 0.4f,
                                CHUCK_HIP_Y + 0.35f - breath * 0.2f};
    pose->lean = 0.2f;
    pose->ankle[CHUCK_FAR] = (ChuckPoint){CHUCK_ROOT_X - 2.6f, CHUCK_ANKLE_Y};
    pose->ankle[CHUCK_NEAR] = (ChuckPoint){CHUCK_ROOT_X + 1.9f, CHUCK_ANKLE_Y};
    pose->pitch[CHUCK_FAR] = 0.0f;
    pose->pitch[CHUCK_NEAR] = 0.0f;
    pose->arm_swing[CHUCK_FAR] = -0.06f;
    pose->arm_swing[CHUCK_NEAR] = 0.0f;
    pose->arm_bend[CHUCK_FAR] = 0.20f;
    pose->arm_bend[CHUCK_NEAR] = 0.16f;
    pose->tail = 0.3f;
    pose->legs = 1.0f;
}

void chuck_pose_gait(ChuckPose *pose, ChuckGait gait, float cycle)
{
    const GaitShape *g = gait_shape(gait);
    float c = wrap01(cycle);
    float step = wrap01(c * 2.0f);
    float pelvis_x = CHUCK_ROOT_X - g->lean * 0.45f;
    float hip_y = CHUCK_HIP_Y + g->hip_drop +
                  0.5f * g->bounce * cosf(CHUCK_TAU * (step - g->duty));

    /*
     * A planted foot has to be within reach of the hip over it, and nothing here
     * forces that: the hip drop and the bounce are what keep it true, and they
     * were chosen to. The suite is what holds it — a hip held too high for the
     * leg under it pulls the solved ankle up and back off the spot the foot was
     * put on, and `test_a_planted_foot_stays_where_it_was_put` measures exactly
     * that spot. A clamp here used to lower the hips instead, and it never once
     * fired, which made it a rule nobody had checked standing in for one the
     * suite does check.
     */
    for (int side = 0; side < 2; ++side)
    {
        float dx, lift, pitch;
        float hip_x = pelvis_x + (side == CHUCK_NEAR ? 0.5f : -0.5f);

        gait_foot(g, side == CHUCK_NEAR ? c : c + 0.5f, &dx, &lift, &pitch);
        pose->ankle[side] = (ChuckPoint){hip_x + dx, CHUCK_ANKLE_Y - lift};
        pose->pitch[side] = pitch * g->roll;
    }
    pose->pelvis = (ChuckPoint){pelvis_x, hip_y};
    /* The spine pitches a little further forward over the planted leg. */
    pose->lean = g->lean +
                 0.12f * g->lean * cosf(CHUCK_TAU * (step - g->duty * 0.5f));

    /* The arms against the legs: the near arm is furthest back as the near
       heel strikes out in front, and a touch late, because an arm is swung
       from the shoulder and follows through. */
    float arm = -g->arm_swing * cosf(CHUCK_TAU * (c - g->arm_lag));
    pose->arm_swing[CHUCK_NEAR] = arm;
    pose->arm_swing[CHUCK_FAR] = -arm;
    for (int side = 0; side < 2; ++side)
    {
        float forward = fmaxf(0.0f, pose->arm_swing[side]) / g->arm_swing;
        pose->arm_bend[side] = g->arm_bend + g->arm_bend_forward * forward;
    }
    pose->tail = g->tail * (0.85f + 0.15f * sinf(CHUCK_TAU * step));
    pose->legs = 1.0f;
}

/*
 * Three keyframes and the air between them: leaving the floor, the top of the
 * arc and coming down to meet it. Each is a set of offsets from the pelvis, so
 * the figure can be dropped anywhere in its box.
 */
typedef struct
{
    float near_x, near_y, far_x, far_y;
    float near_pitch, far_pitch;
    float near_arm, far_arm, near_bend, far_bend;
    float lean;
    float pelvis_y;
} AirKey;

static const AirKey AIR_TAKEOFF = {2.4f, 5.0f, -2.3f, 8.3f, -0.2f, 1.0f,
                                   1.95f, -0.85f, 0.55f, 0.45f, 0.9f, -0.4f};
static const AirKey AIR_APEX = {1.9f, 5.1f, -0.8f, 6.2f, 0.1f, 0.6f,
                                1.25f, -0.25f, 0.95f, 0.8f, 0.6f, -0.25f};
static const AirKey AIR_DROP = {2.0f, 8.0f, -1.4f, 7.4f, -0.35f, 0.35f,
                                1.45f, 0.95f, 0.45f, 0.55f, 0.25f, 0.0f};

static AirKey air_mix(const AirKey *a, const AirKey *b, float t)
{
    return (AirKey){lerp(a->near_x, b->near_x, t), lerp(a->near_y, b->near_y, t),
                    lerp(a->far_x, b->far_x, t), lerp(a->far_y, b->far_y, t),
                    lerp(a->near_pitch, b->near_pitch, t),
                    lerp(a->far_pitch, b->far_pitch, t),
                    lerp(a->near_arm, b->near_arm, t),
                    lerp(a->far_arm, b->far_arm, t),
                    lerp(a->near_bend, b->near_bend, t),
                    lerp(a->far_bend, b->far_bend, t), lerp(a->lean, b->lean, t),
                    lerp(a->pelvis_y, b->pelvis_y, t)};
}

void chuck_pose_air(ChuckPose *pose, float rise)
{
    rise = rise < -1.0f ? -1.0f : (rise > 1.0f ? 1.0f : rise);
    AirKey k = rise >= 0.0f ? air_mix(&AIR_APEX, &AIR_TAKEOFF, smooth(rise))
                            : air_mix(&AIR_APEX, &AIR_DROP, smooth(-rise));
    float px = CHUCK_ROOT_X - 0.3f;
    float py = CHUCK_HIP_Y + k.pelvis_y;

    pose->pelvis = (ChuckPoint){px, py};
    pose->lean = k.lean;
    pose->ankle[CHUCK_NEAR] = (ChuckPoint){px + k.near_x, py + k.near_y};
    pose->ankle[CHUCK_FAR] = (ChuckPoint){px + k.far_x, py + k.far_y};
    pose->pitch[CHUCK_NEAR] = k.near_pitch;
    pose->pitch[CHUCK_FAR] = k.far_pitch;
    pose->arm_swing[CHUCK_NEAR] = k.near_arm;
    pose->arm_swing[CHUCK_FAR] = k.far_arm;
    pose->arm_bend[CHUCK_NEAR] = k.near_bend;
    pose->arm_bend[CHUCK_FAR] = k.far_bend;
    pose->tail = 1.6f + fabsf(rise) * 1.2f;
    pose->legs = 1.0f;
}

void chuck_pose_sink(ChuckPose *pose, float amount)
{
    pose->pelvis.y += amount;
    /* A body taking a load folds at the hips as well as the knees. */
    pose->lean += amount * 0.28f;
    pose->arm_bend[CHUCK_FAR] += amount * 0.08f;
    pose->arm_bend[CHUCK_NEAR] += amount * 0.08f;
}

void chuck_pose_blend(ChuckPose *a, const ChuckPose *b, float t)
{
    t = clamp01(t);
    a->pelvis.x = lerp(a->pelvis.x, b->pelvis.x, t);
    a->pelvis.y = lerp(a->pelvis.y, b->pelvis.y, t);
    a->lean = lerp(a->lean, b->lean, t);
    for (int side = 0; side < 2; ++side)
    {
        a->ankle[side].x = lerp(a->ankle[side].x, b->ankle[side].x, t);
        a->ankle[side].y = lerp(a->ankle[side].y, b->ankle[side].y, t);
        a->pitch[side] = lerp(a->pitch[side], b->pitch[side], t);
        a->arm_swing[side] = lerp(a->arm_swing[side], b->arm_swing[side], t);
        a->arm_bend[side] = lerp(a->arm_bend[side], b->arm_bend[side], t);
    }
    a->tail = lerp(a->tail, b->tail, t);
    a->legs = lerp(a->legs, b->legs, t);
}

void chuck_pose_fit_legs(ChuckPose *pose, float legs)
{
    /* The pelvis over the floor a planted ankle stands on, and each ankle
       about the pelvis, both scaled: that is every leg scaled about its own
       hip once `chuck_pose_solve` sets the hips `legs` as far apart. A foot on
       the floor stays on it and a lifted one is lifted by the share. */
    ChuckPoint was = pose->pelvis;
    pose->pelvis.y = CHUCK_ANKLE_Y - (CHUCK_ANKLE_Y - was.y) * legs;
    for (int side = 0; side < 2; ++side)
    {
        ChuckPoint *a = &pose->ankle[side];
        a->x = pose->pelvis.x + (a->x - was.x) * legs;
        a->y = pose->pelvis.y + (a->y - was.y) * legs;
    }
    pose->legs *= legs;
}

void chuck_pose_solve(ChuckPose *pose)
{
    /* The spine keeps its length however far he leans: `lean` is how far the
       neck is ahead of the hips, and the height comes out of that. */
    float lean = fmaxf(-CHUCK_SPINE * 0.6f,
                       fminf(pose->lean, CHUCK_SPINE * 0.6f));
    float rise = sqrtf(CHUCK_SPINE * CHUCK_SPINE - lean * lean);
    float sx = lean / CHUCK_SPINE;
    float sy = -rise / CHUCK_SPINE;

    pose->neck = (ChuckPoint){pose->pelvis.x + lean, pose->pelvis.y - rise};

    ChuckPoint shoulder = {pose->neck.x - sx * CHUCK_SHOULDER_DROP,
                           pose->neck.y - sy * CHUCK_SHOULDER_DROP};
    /* Seen side on the two shoulders all but coincide; the far one sits a
       fraction behind so its arm clears the near one at the top of a swing. */
    pose->shoulder[CHUCK_FAR] = (ChuckPoint){shoulder.x - 0.3f, shoulder.y};
    pose->shoulder[CHUCK_NEAR] = (ChuckPoint){shoulder.x + 0.7f, shoulder.y};

    for (int side = 0; side < 2; ++side)
    {
        pose->hip[side] = (ChuckPoint){
            pose->pelvis.x + (side == CHUCK_NEAR ? 0.5f : -0.5f) * pose->legs,
            pose->pelvis.y};
        pose->knee[side] = chuck_joint(pose->hip[side], &pose->ankle[side],
                                       CHUCK_THIGH * pose->legs,
                                       CHUCK_SHIN * pose->legs, -1.0f);

        float a = pose->arm_swing[side];
        float b = a + pose->arm_bend[side];
        ChuckPoint s = pose->shoulder[side];
        pose->elbow[side] = (ChuckPoint){s.x + sinf(a) * CHUCK_UPPER_ARM,
                                         s.y + cosf(a) * CHUCK_UPPER_ARM};
        pose->hand[side] =
            (ChuckPoint){pose->elbow[side].x + sinf(b) * CHUCK_FOREARM,
                         pose->elbow[side].y + cosf(b) * CHUCK_FOREARM};
    }
}

void chuck_pose_reach(ChuckPose *pose, int side, ChuckPoint hand)
{
    pose->elbow[side] = chuck_joint(pose->shoulder[side], &hand,
                                    CHUCK_UPPER_ARM, CHUCK_FOREARM, 1.0f);
    pose->hand[side] = hand;
}
