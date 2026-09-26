#ifndef CHUCK_RENDER_CHUCK_H
#define CHUCK_RENDER_CHUCK_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "chuck_pose.h"

/*
 * Chuck, drawn from his skeleton, at whatever size he is on screen.
 *
 * The sector draws him at one pixel to the unit and the film at 1.4, and those
 * used to be two drawings of him — a set of rectangles in render_figures.c and a
 * second set, scaled, in cutscene.c — that agreed about the colours and about
 * nothing else. One drawing now, laid over a [chuck_pose.h](chuck_pose.h)
 * frame, so the man in the lobby and the man in the corridor between sectors
 * walk the same way because they are the same code. The one difference is the
 * film's own and deliberate: it fits him with shorter legs
 * (`chuck_pose_fit_legs`), so he stands at the crew's height among the crew.
 *
 * It sits on the shell side of the SDL boundary with the rest of the cast, and
 * it is built from the same rules [render_sprite.h](render_sprite.h) states for
 * every figure: an ink silhouette a pixel outside the form, the garment as a
 * ramp lit from the ceiling, one rim down the flank he is facing, and trousers a
 * long way under the jacket in value so the jacket is the mass the eye lands on.
 * Its parts are the cast's chamfered blocks on purpose: see `draw_block` for
 * the version of him that was shaped instead, and why it did not last.
 */

typedef struct
{
    SDL_Renderer *r;
    float x;       /* left edge of the 26-unit sprite box, on screen */
    float y;       /* top of the box */
    int dir;       /* +1 facing right; -1 mirrors about the box */
    float scale;   /* screen pixels per sprite unit */
    SDL_Color ink; /* the outline of whichever cast he is standing among */
} ChuckView;

typedef enum
{
    CHUCK_HAND_OPEN,   /* a hand at rest or swinging */
    CHUCK_HAND_GRIP,   /* closed on something the caller draws */
    CHUCK_HAND_HIDDEN  /* behind whatever the caller draws over it */
} ChuckHand;

/*
 * What his face is doing.
 *
 * The sector only ever draws him at ease — at one pixel to the unit a face is an
 * eye and a mouth, and a player is watching what he does rather than what he
 * feels. The film is where he has to be seen to react, and the kerb above all:
 * it is the one beat of the campaign in which he is given nothing to do but
 * watch, so what the moment does to him has to be on him.
 */
typedef enum
{
    CHUCK_FACE_EASY,
    CHUCK_FACE_ALARM, /* eyes wide, mouth open */
    CHUCK_FACE_FURY   /* the brow down over a narrowed eye, the mouth set */
} ChuckFace;

/* Where a sprite-space point lands on screen through a view. */
float chuck_view_x(const ChuckView *view, float lx);
float chuck_view_y(const ChuckView *view, float ly);

/* A rectangle in sprite space, mirrored and scaled with the figure, for the
 * props a caller hangs on him. */
void chuck_view_rect(const ChuckView *view, float lx, float ly, float w,
                     float h, SDL_Color c);
/* A straight band of `width` units between two sprite-space points. */
void chuck_view_band(const ChuckView *view, ChuckPoint a, ChuckPoint b,
                     float width, SDL_Color c);

/*
 * The parts, in the order they have to go down. A caller that puts a prop in
 * his hands draws everything up to the head, then the prop, then the near arm
 * over it; one that does not can call `chuck_draw` and be done.
 */
void chuck_draw_arm(const ChuckView *view, const ChuckPose *pose, int side,
                    ChuckHand hand);
void chuck_draw_legs(const ChuckView *view, const ChuckPose *pose);
void chuck_draw_torso(const ChuckView *view, const ChuckPose *pose);
void chuck_draw_head(const ChuckView *view, const ChuckPose *pose, bool blink);
/* The same head wearing `face`. A face that is not at ease does not blink. */
void chuck_draw_head_as(const ChuckView *view, const ChuckPose *pose,
                        bool blink, ChuckFace face);

void chuck_draw(const ChuckView *view, const ChuckPose *pose, bool blink);

/* The middle of the top of his hair in sprite units, for a caller that hangs
 * something over his head. */
ChuckPoint chuck_head_crown(const ChuckPose *pose);

#endif
