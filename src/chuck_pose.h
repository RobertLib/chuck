#ifndef CHUCK_CHUCK_POSE_H
#define CHUCK_CHUCK_POSE_H

#include <stdbool.h>

/*
 * Chuck's skeleton: where every joint of him is, in one frame of one move.
 *
 * The sector draws him at thirty-two pixels and the film at one and a half times
 * that, and for as long as each renderer posed him for itself the two came out
 * wrong in the same way: the walk was a pair of feet pedalling three and a half
 * pixels either side of the hip while the man travelled twenty-two between
 * footfalls. A foot that moves a sixth as far as the body over it is a foot
 * sliding on the floor, and every cutscene between sectors was that slide at
 * one and a half times the size.
 *
 * So the body is a skeleton here rather than a set of rectangles in two files:
 * bone lengths that no pose can change, knees and elbows that are solved rather
 * than placed, and a gait whose planted foot travels back under the hips at
 * exactly the rate the hips travel forward. That last one is a property rather
 * than a look, which is why this file links no SDL: the suite holds it, and the
 * two renderers — `draw_player` in render_figures.c and the film's Chuck in
 * cutscene.c, both through render_chuck.c — draw what it answers.
 *
 * Everything is in the sector's sprite box: twenty-six units wide, thirty-two
 * tall, the man facing +x, the soles on y = 32. A renderer mirrors and scales
 * it; nothing in here knows which way he is really facing or how big he is.
 */

#define CHUCK_BOX_W 26.0f
#define CHUCK_ROOT_X 13.0f
#define CHUCK_GROUND_Y 32.0f
/* The ankle of a foot flat on the floor: the shoe under it is two units deep. */
#define CHUCK_ANKLE_Y 30.0f

/*
 * The proportions, and the reason for each of them.
 *
 * He is built to the same template as everybody else in the building — the
 * guards, the janitor, the civilians and the film's crew all stand a
 * thirty-two unit figure on a head of eleven rows and a jacket of ten to twelve
 * — because a player reads the cast as one world by its proportions before any
 * other detail. The first version of this skeleton gave him a smaller head and
 * a lot more leg, and he stopped looking like a man from the same game as the
 * people shooting at him. What he keeps of it is a little more leg than the old
 * drawing had: the hip joint sits under the belt at 20.7 rather than at 21.5,
 * so the stride has room to be a stride. Standing, the leg is a hair short of
 * straight, because a knee locked dead straight is the one pose no living body
 * holds.
 */
#define CHUCK_THIGH 4.8f
#define CHUCK_SHIN 4.8f
#define CHUCK_HIP_Y 20.7f
#define CHUCK_UPPER_ARM 4.9f
#define CHUCK_FOREARM 4.8f
/* Hips to the base of the neck. */
#define CHUCK_SPINE 10.0f
/* The shoulder joint, measured down the spine from the base of the neck. */
#define CHUCK_SHOULDER_DROP 2.0f

/* The two sides of him in the order they are drawn: the far limb passes behind
 * the body, the near one in front of it. */
enum
{
    CHUCK_FAR = 0,
    CHUCK_NEAR = 1
};

typedef struct
{
    float x;
    float y;
} ChuckPoint;

/*
 * One frame of him.
 *
 * The first block is what a move decides, and is what gets blended when one
 * move hands over to another; the second is derived from it by
 * `chuck_pose_solve` and is never set by hand, because a knee that is placed
 * rather than solved is a knee that can make a shin longer than the other one.
 */
typedef struct
{
    ChuckPoint pelvis;    /* bottom of the spine, between the two hips */
    float lean;           /* how far the base of the neck is ahead of the hips */
    ChuckPoint ankle[2];  /* where each foot is asked to be */
    float pitch[2];       /* the foot: +1 up on the toe, -1 toe raised */
    float arm_swing[2];   /* upper arm from hanging straight, + is forward */
    float arm_bend[2];    /* elbow flexion in radians, 0 is a straight arm */
    float tail;           /* how far the headband's loose end trails */
    float legs;           /* leg bones as a share of the sector's; see
                             `chuck_pose_fit_legs` */

    ChuckPoint hip[2];
    ChuckPoint knee[2];
    ChuckPoint neck;
    ChuckPoint shoulder[2];
    ChuckPoint elbow[2];
    ChuckPoint hand[2];
} ChuckPose;

/*
 * The ways he covers ground. A run has a flight between footfalls and a walk
 * has both feet down at once; everything else about them — how far a stride
 * is, how hard the arms pump, how much he leans — follows from that.
 *
 * The plain run is the film's, and it is the run with the acting taken out: the
 * same flight and a stride as long, so a planted foot stays planted at the
 * film's speeds exactly as it does in the sector, but upright, on flat soles,
 * with the swinging foot kept low and the arms hanging rather than pumping.
 * Beside the crew, who walk on a two-beat stride with their soles flat and
 * their bodies still, the sector's run — the heel kicked up behind him, the
 * lean, the elbows driving and the headband streaming — read as a man drawn for
 * a different film. The sector keeps it, because there he is the only thing
 * moving that way and a player is steering it.
 */
typedef enum
{
    CHUCK_GAIT_WALK,
    CHUCK_GAIT_RUN,
    CHUCK_GAIT_PLAIN_RUN,
    CHUCK_GAIT_COUNT
} ChuckGait;

/*
 * How far he travels in one full cycle of a gait — two steps — in sprite units.
 *
 * It is not a tuning knob: it is the length a planted foot sweeps under the
 * hips, divided by the share of the cycle the foot spends planted, and it has to
 * be exactly that or the foot slides. Callers turn distance into a place in the
 * cycle with `chuck_gait_cycle` rather than turning *time* into one, because
 * time is what made the film's cast skate: a figure eased into a run and out of
 * it on a smoothstep moves at every speed from nought up, and a cycle driven by
 * the clock pedals at one of them.
 */
float chuck_gait_stride(ChuckGait gait);
/* The share of a cycle each foot spends on the floor. */
float chuck_gait_duty(ChuckGait gait);
/* `distance` travelled forward, as a place in the cycle, 0..1. */
float chuck_gait_cycle(ChuckGait gait, float distance);

/* Standing easy. `breath` is -1..1 and lifts the chest a fraction. */
void chuck_pose_stand(ChuckPose *pose, float breath);

/* One frame of a gait, `cycle` the near leg's place in it: 0 is the near heel
 * striking the floor ahead of him. The far leg runs half a cycle behind. */
void chuck_pose_gait(ChuckPose *pose, ChuckGait gait, float cycle);

/*
 * In the air. `rise` is +1 leaving the floor at full jump speed, 0 at the top
 * of the arc and -1 dropping fast: the near knee comes up and the arms go up
 * with the take-off, and on the way down the legs open out to meet the floor.
 */
void chuck_pose_air(ChuckPose *pose, float rise);

/* Sink the hips by `amount` units with the feet left where they are, so the
 * knees take it — a landing, a brace. */
void chuck_pose_sink(ChuckPose *pose, float amount);

/*
 * The same move on legs `legs` times as long as the sector's, with the feet
 * left on the floor: every leg of the pose is scaled about its own hip, the
 * pelvis comes down to keep a planted foot planted, and the body above it is
 * carried down with it. Call after the move is decided and before solving.
 *
 * It exists for the film. The sector's man is drawn against a tile grid and his
 * leg is what a jump and a stride are measured on, while the film stands him
 * beside the crew, whose legs are shorter under a longer body, and at one and a
 * half times the size his read as a different build. A fitted gait covers
 * `legs` times the ground per cycle, so a caller turns distance into a cycle
 * with `chuck_gait_cycle(gait, distance / legs)` or the planted foot slides.
 */
void chuck_pose_fit_legs(ChuckPose *pose, float legs);

/* `a` moved `t` of the way toward `b`, field by field, for the decided half of
 * the pose. Solve afterwards. */
void chuck_pose_blend(ChuckPose *a, const ChuckPose *b, float t);

/* Derive the joints from what was decided. */
void chuck_pose_solve(ChuckPose *pose);

/*
 * Put one hand on a point — a grip, a trigger, a rung — with the elbow solved
 * behind the line from shoulder to hand. A point further than the arm reaches
 * is pulled back onto the end of it, so no pose can lengthen an arm to make a
 * gun look held. Call after `chuck_pose_solve`.
 */
void chuck_pose_reach(ChuckPose *pose, int side, ChuckPoint hand);

/*
 * The middle joint of a two-bone limb from `a` to `*b`. `bend` picks the side:
 * +1 breaks behind the line (an elbow), -1 in front of it (a knee). An end
 * beyond reach is moved back onto it, and `*b` says where it landed.
 */
ChuckPoint chuck_joint(ChuckPoint a, ChuckPoint *b, float bone_a, float bone_b,
                       float bend);

#endif
