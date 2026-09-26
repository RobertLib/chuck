/*
 * Chuck, drawn. See [render_chuck.h](render_chuck.h) for why this is one file
 * serving two renderers, and [chuck_pose.h](chuck_pose.h) for the skeleton it
 * is laid over.
 */

#include "render_chuck.h"

#include <math.h>

#include "fx.h"

/*
 * His materials. The jacket, skin, hair and headband are fx.h's; the rest are
 * the steps between them that fx_ramp cannot derive, named once here rather
 * than spelled at each place they are drawn — they were spelled in two files
 * for as long as there were two drawings of him.
 */
static const SDL_Color FOREARM_NEAR = {209, 154, 105, 255};
static const SDL_Color FOREARM_FAR = {189, 132, 91, 255};
static const SDL_Color TROUSER_NEAR = {29, 55, 80, 255};
static const SDL_Color TROUSER_FAR = {21, 40, 59, 255};
static const SDL_Color BOOT_NEAR = {34, 39, 49, 255};
static const SDL_Color BOOT_FAR = {26, 31, 40, 255};
static const SDL_Color HAIR_LT = {102, 62, 42, 255};
static const SDL_Color STRAP = {21, 54, 76, 255};
static const SDL_Color STRAP_LT = {46, 96, 126, 255};
static const SDL_Color HEM = {18, 46, 66, 255};
static const SDL_Color LAPEL = {30, 76, 106, 255};
static const SDL_Color SLEEVE_NEAR = {42, 118, 153, 255};
static const SDL_Color BELT_LT = {255, 214, 128, 255};
static const SDL_Color BAND_LT = {246, 104, 88, 255};
static const SDL_Color BAND_TAIL = {166, 38, 42, 255};
static const SDL_Color BROW = {181, 127, 87, 255};
static const SDL_Color EYE_WHITE = {166, 176, 164, 255};
static const SDL_Color PUPIL = {38, 50, 60, 255};
static const SDL_Color LID = {110, 58, 40, 255};
static const SDL_Color MOUTH = {126, 66, 50, 255};
/* The inside of an open mouth, and a brow drawn down hard enough to be a line:
   both darker than anything else on the face, because at the film's scale a
   feeling is two or three pixels and they have to carry it. */
static const SDL_Color MOUTH_OPEN = {58, 24, 22, 255};
static const SDL_Color BROW_KNIT = {66, 38, 28, 255};

/*
 * Limb widths in sprite units: three pixels of garment at one pixel to the unit,
 * four and five in the film, which is what every other limb in either place is.
 */
#define THIGH_W 3.2f
#define SHIN_W 3.0f
#define UPPER_ARM_W 2.8f
#define FOREARM_W 2.6f
#define SHOE_H 2.0f

/* ---- Where things land ------------------------------------------------ */

static float view_sx(const ChuckView *v, float lx)
{
    return v->dir >= 0 ? v->x + lx * v->scale
                       : v->x + (CHUCK_BOX_W - lx) * v->scale;
}

static float view_sy(const ChuckView *v, float ly)
{
    return v->y + ly * v->scale;
}

float chuck_view_x(const ChuckView *view, float lx)
{
    return view_sx(view, lx);
}

float chuck_view_y(const ChuckView *view, float ly)
{
    return view_sy(view, ly);
}

static float snap(float v)
{
    return floorf(v + 0.5f);
}

void chuck_view_rect(const ChuckView *view, float lx, float ly, float w,
                     float h, SDL_Color c)
{
    float a = view_sx(view, lx);
    float b = view_sx(view, lx + w);
    float left = snap(fminf(a, b));
    float right = snap(fmaxf(a, b));
    float top = snap(view_sy(view, ly));
    float bottom = snap(view_sy(view, ly + h));

    if (right <= left)
        right = left + 1.0f;
    if (bottom <= top)
        bottom = top + 1.0f;
    fx_rect(view->r, c, left, top, right - left, bottom - top);
}

/*
 * A straight run of pixels `w` wide between two screen points, stepped along
 * whichever axis is longer so a steep limb and a level one come out equally
 * solid. `cap` carries the run past both ends, which is what lets an outline
 * close round the end of a limb instead of stopping flush with it.
 */
static void stroke(SDL_Renderer *r, float x1, float y1, float x2, float y2,
                   float w, float cap, SDL_Color c)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);

    if (cap > 0.0f && len > 0.001f)
    {
        x1 -= dx / len * cap;
        y1 -= dy / len * cap;
        x2 += dx / len * cap;
        y2 += dy / len * cap;
        dx = x2 - x1;
        dy = y2 - y1;
    }

    fx_set(r, c);
    if (fabsf(dy) >= fabsf(dx))
    {
        float top = floorf(fminf(y1, y2));
        float bottom = floorf(fmaxf(y1, y2));
        for (float row = top; row <= bottom; row += 1.0f)
        {
            float t = fabsf(dy) > 0.001f ? (row + 0.5f - y1) / dy : 0.0f;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            fx_fill(r, floorf(x1 + dx * t - w * 0.5f + 0.5f), row, w, 1.0f);
        }
    }
    else
    {
        float left = floorf(fminf(x1, x2));
        float right = floorf(fmaxf(x1, x2));
        for (float col = left; col <= right; col += 1.0f)
        {
            float t = (col + 0.5f - x1) / dx;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            fx_fill(r, col, floorf(y1 + dy * t - w * 0.5f + 0.5f), 1.0f, w);
        }
    }
}

void chuck_view_band(const ChuckView *view, ChuckPoint a, ChuckPoint b,
                     float width, SDL_Color c)
{
    stroke(view->r, view_sx(view, a.x), view_sy(view, a.y),
           view_sx(view, b.x), view_sy(view, b.y),
           fmaxf(1.0f, roundf(width * view->scale)), 0.0f, c);
}

/* ---- Limbs ------------------------------------------------------------ */

static float limb_px(const ChuckView *v, float width)
{
    return fmaxf(2.0f, roundf(width * v->scale));
}

/* Every bone of a limb is inked before any of them is painted, so the joint
 * between two of them never carries a seam. */
static void bone_ink(const ChuckView *v, ChuckPoint a, ChuckPoint b,
                     float width)
{
    stroke(v->r, view_sx(v, a.x), view_sy(v, a.y), view_sx(v, b.x),
           view_sy(v, b.y), limb_px(v, width) + 2.0f, 1.0f, v->ink);
}

/*
 * One bone as a cylinder: the garment, a lit line down the side the ceiling and
 * the facing reach, and a shaded one down the other. A limb standing up takes
 * its rim on the flank he faces, the same flank his jacket carries it on; a limb
 * held level is lit along the top and shaded under it.
 */
static void bone_paint(const ChuckView *v, ChuckPoint a, ChuckPoint b,
                       float width, SDL_Color fill)
{
    FxRamp ramp = fx_ramp(fill);
    float w = limb_px(v, width);
    float edge = (w - 1.0f) * 0.5f;
    float x1 = view_sx(v, a.x);
    float y1 = view_sy(v, a.y);
    float x2 = view_sx(v, b.x);
    float y2 = view_sy(v, b.y);

    stroke(v->r, x1, y1, x2, y2, w, 0.0f, fill);
    if (w < 3.0f)
        return;
    if (fabsf(y2 - y1) >= fabsf(x2 - x1))
    {
        float lead = v->dir >= 0 ? edge : -edge;
        stroke(v->r, x1 + lead, y1, x2 + lead, y2, 1.0f, 0.0f,
               fx_mix(ramp.base, ramp.lit, 0.55f));
        stroke(v->r, x1 - lead, y1, x2 - lead, y2, 1.0f, 0.0f, ramp.dark);
    }
    else
    {
        stroke(v->r, x1, y1 - edge, x2, y2 - edge, 1.0f, 0.0f, ramp.lit);
        stroke(v->r, x1, y1 + edge, x2, y2 + edge, 1.0f, 0.0f, ramp.dark);
    }
}

/*
 * The shoe's sole line, heel to toe, a shoe-depth under the ankle and turned
 * with the foot. Tipped forward it pivots on the toe, which is what keeps the
 * toe on the floor while the heel comes up; tipped back it lifts the toe for the
 * heel to land on.
 */
static void shoe_line(ChuckPoint ankle, float pitch, ChuckPoint *heel,
                      ChuckPoint *toe)
{
    float phi = pitch > 0.0f ? pitch * 0.85f : pitch * 0.40f;
    float c = cosf(phi);
    float s = sinf(phi);
    float depth = SHOE_H * 0.5f;

    heel->x = ankle.x + (-0.9f) * c - depth * s;
    heel->y = ankle.y + (-0.9f) * s + depth * c;
    toe->x = ankle.x + 3.4f * c - depth * s;
    toe->y = ankle.y + 3.4f * s + depth * c;
}

static void shoe_ink(const ChuckView *v, ChuckPoint ankle, float pitch)
{
    ChuckPoint heel, toe;
    shoe_line(ankle, pitch, &heel, &toe);
    stroke(v->r, view_sx(v, heel.x), view_sy(v, heel.y), view_sx(v, toe.x),
           view_sy(v, toe.y), limb_px(v, SHOE_H) + 2.0f, 1.0f, v->ink);
}

/* Heel, sole and the toe cap that catches the light: the toe is what points a
 * figure somewhere, and a pair of near-black boots with no break in them fuse
 * with the shadow into a plinth. */
static void shoe_paint(const ChuckView *v, ChuckPoint ankle, float pitch,
                       SDL_Color boot)
{
    FxRamp ramp = fx_ramp(boot);
    ChuckPoint heel, toe;
    shoe_line(ankle, pitch, &heel, &toe);
    float w = limb_px(v, SHOE_H);
    float hx = view_sx(v, heel.x);
    float hy = view_sy(v, heel.y);
    float tx = view_sx(v, toe.x);
    float ty = view_sy(v, toe.y);

    stroke(v->r, hx, hy, tx, ty, w, 0.0f, boot);
    /* The upper half of the front of the shoe lit, the back of the heel dark. */
    float lift = (w - 1.0f) * 0.5f;
    stroke(v->r, hx + (tx - hx) * 0.55f, hy + (ty - hy) * 0.55f - lift, tx,
           ty - lift, 1.0f, 0.0f, ramp.lit);
    stroke(v->r, hx, hy, hx + (tx - hx) * 0.22f, hy + (ty - hy) * 0.22f, w,
           0.0f, ramp.dark);
}

static void draw_leg(const ChuckView *v, const ChuckPose *p, int side)
{
    SDL_Color trouser = side == CHUCK_NEAR ? TROUSER_NEAR : TROUSER_FAR;
    SDL_Color boot = side == CHUCK_NEAR ? BOOT_NEAR : BOOT_FAR;

    bone_ink(v, p->hip[side], p->knee[side], THIGH_W);
    bone_ink(v, p->knee[side], p->ankle[side], SHIN_W);
    shoe_ink(v, p->ankle[side], p->pitch[side]);
    shoe_paint(v, p->ankle[side], p->pitch[side], boot);
    bone_paint(v, p->hip[side], p->knee[side], THIGH_W, trouser);
    bone_paint(v, p->knee[side], p->ankle[side], SHIN_W, trouser);
}

static void draw_hand(const ChuckView *v, ChuckPoint elbow, ChuckPoint hand,
                      SDL_Color skin, ChuckHand kind)
{
    if (kind == CHUCK_HAND_HIDDEN)
        return;
    float dx = hand.x - elbow.x;
    float dy = hand.y - elbow.y;
    float len = sqrtf(dx * dx + dy * dy);
    float ux = len > 0.001f ? dx / len : 0.0f;
    float uy = len > 0.001f ? dy / len : 1.0f;
    /* The fist sits just past the end of the forearm, not on top of it. */
    float cx = hand.x + ux * 0.6f;
    float cy = hand.y + uy * 0.6f;
    float size = kind == CHUCK_HAND_GRIP ? 2.0f : 1.9f;
    float px = fmaxf(2.0f, roundf(size * v->scale));
    float sx = snap(view_sx(v, cx) - px * 0.5f);
    float sy = snap(view_sy(v, cy) - px * 0.5f);

    fx_rect(v->r, v->ink, sx - 1.0f, sy - 1.0f, px + 2.0f, px + 2.0f);
    fx_rect(v->r, skin, sx, sy, px, px);
    fx_rect(v->r, fx_ramp(skin).lit, sx, sy, px, 1.0f);
    if (kind == CHUCK_HAND_GRIP && px >= 3.0f)
        fx_rect(v->r, fx_ramp(skin).dark, sx, sy + px - 1.0f, px, 1.0f);
}

void chuck_draw_arm(const ChuckView *view, const ChuckPose *pose, int side,
                    ChuckHand hand)
{
    bool near = side == CHUCK_NEAR;
    SDL_Color sleeve = near ? SLEEVE_NEAR : FX_HERO;
    SDL_Color forearm = near ? FOREARM_NEAR : FOREARM_FAR;

    bone_ink(view, pose->shoulder[side], pose->elbow[side], UPPER_ARM_W);
    bone_ink(view, pose->elbow[side], pose->hand[side], FOREARM_W);
    bone_paint(view, pose->shoulder[side], pose->elbow[side], UPPER_ARM_W,
               sleeve);
    bone_paint(view, pose->elbow[side], pose->hand[side], FOREARM_W, forearm);
    /* The sleeve is pushed up to the elbow: a turn of it sits over the joint
       so the jacket ends somewhere instead of fading into the forearm. */
    ChuckPoint cuff_a = pose->elbow[side];
    ChuckPoint cuff_b = {cuff_a.x + (pose->hand[side].x - cuff_a.x) * 0.18f,
                         cuff_a.y + (pose->hand[side].y - cuff_a.y) * 0.18f};
    bone_paint(view, cuff_a, cuff_b, UPPER_ARM_W, fx_ramp(sleeve).dark);
    draw_hand(view, pose->elbow[side], pose->hand[side], forearm, hand);
}

/* ---- Blocks ----------------------------------------------------------- */

/*
 * A body part the way the rest of the cast is built: a block with a pixel or two
 * off its corners (`fx_taper`, top and bottom given separately), its outline
 * running along the same taper a unit further out, and the garment lit from the
 * ceiling — the crown on top, the underside in shade, one rim down the flank he
 * faces. It is `sprite_body` and the film's `cast_body` in sprite units, so it
 * draws the same block at one pixel to the unit and at the film's 1.4.
 *
 * The first drawing of him from the skeleton shaped the jacket and the head as
 * profiles instead — a chest standing forward, a waist, a skull coming to a chin
 * — and it was the better drawing of a man and the wrong drawing of *this* man:
 * every guard, civilian and captor in the game is chamfered blocks, and a figure
 * built any other way reads as pasted in from another game however well it is
 * drawn. What moves is the skeleton; what it wears is the cast's.
 */
typedef enum
{
    BLOCK_INK,
    BLOCK_FLAT,
    BLOCK_LIT
} BlockFill;

static void draw_block(const ChuckView *v, float lx, float ly, float w,
                       float h, int top, int bottom, SDL_Color c,
                       BlockFill fill)
{
    if (fill == BLOCK_INK)
    {
        lx -= 1.0f;
        ly -= 1.0f;
        w += 2.0f;
        h += 2.0f;
        top += 1;
        bottom += 1;
        c = v->ink;
    }

    FxRamp ramp = fx_ramp(c);
    SDL_Color rim = fx_mix(ramp.base, ramp.lit, 0.50f);
    int rows = (int)ceilf(h - 0.001f);
    int crown = h >= 10.0f ? 2 : 1;
    for (int i = 0; i < rows; ++i)
    {
        float inset = fx_taper(i, rows, top, bottom);
        float rw = w - inset * 2.0f;
        if (rw < 0.5f)
            continue;
        float rx = lx + inset;
        float rh = fminf(1.0f, h - (float)i);
        if (fill != BLOCK_LIT)
        {
            chuck_view_rect(v, rx, ly + (float)i, rw, rh, c);
            continue;
        }
        if (i < crown)
        {
            chuck_view_rect(v, rx, ly + (float)i, rw, rh, ramp.lit);
        }
        else if (i == rows - 1)
        {
            chuck_view_rect(v, rx, ly + (float)i, rw, rh, ramp.dark);
        }
        else
        {
            chuck_view_rect(v, rx, ly + (float)i, rw, rh, ramp.base);
            /* Sprite space faces +x, so the flank he faces is the high side;
               the view mirrors it with the rest of him. */
            if (rw >= 6.0f)
                chuck_view_rect(v, rx + rw - 1.0f, ly + (float)i, 1.0f, rh,
                                rim);
        }
    }
}

/* ---- The body --------------------------------------------------------- */

/*
 * The jacket, as the width of each of its ten rows: how far each is set in from
 * the back and from the front of an eleven-unit block.
 *
 * Straight down the sides, the two top corners taken off and nothing more, and
 * the belt and the hem a unit in on both sides. Both ends of that matter. A
 * block chamfered two rows deep at the top has its shoulders sloping away from
 * the neck, and one that runs full width to a bright belt on its bottom row is
 * widest where it stops — narrow on top and flared at the foot is a bell, and
 * a bell on two legs narrower than its rim is exactly the stocky figure this
 * one was drawn to replace. Square shoulders and a hem no wider than the hips
 * under it is a man in a jacket.
 */
#define JACKET_W 11.0f
#define JACKET_BACK 5.2f
static const float JACKET_BACK_IN[] = {1, 0, 0, 0, 0, 0, 0, 1, 1, 1};
static const float JACKET_FRONT_IN[] = {1, 0, 0, 0, 0, 0, 0, 1, 1, 1};
#define JACKET_ROWS ((int)(sizeof(JACKET_BACK_IN) / sizeof(JACKET_BACK_IN[0])))
_Static_assert(sizeof(JACKET_BACK_IN) == sizeof(JACKET_FRONT_IN),
               "one inset from each side for every row of the jacket");
#define JACKET_BELT_ROW 7

void chuck_draw_legs(const ChuckView *view, const ChuckPose *pose)
{
    /* The tops of both thighs go under the jacket's hem, which is drawn over
       them; nothing needs a seat of its own. */
    draw_leg(view, pose, CHUCK_FAR);
    draw_leg(view, pose, CHUCK_NEAR);
}

/*
 * Where the jacket's back edge is. The block does not shear with the spine —
 * a block stepped a pixel back every few rows reads as a belly — it is carried
 * forward whole to the middle of the spine, the way the old drawing leaned into
 * a run by moving the torso a pixel ahead of the legs.
 */
static float jacket_x(const ChuckPose *p)
{
    return (p->neck.x + p->pelvis.x) * 0.5f + 0.1f - JACKET_BACK;
}

/* One row of the jacket, `grow` units wider on each side than the row itself. */
static void jacket_row(const ChuckView *v, float tx, float ty, int row,
                       float grow, SDL_Color c)
{
    float back = JACKET_BACK_IN[row] - grow;
    float front = JACKET_FRONT_IN[row] - grow;
    chuck_view_rect(v, tx + back, ty + (float)row, JACKET_W - back - front,
                    1.0f, c);
}

void chuck_draw_torso(const ChuckView *view, const ChuckPose *pose)
{
    float tx = jacket_x(pose);
    float ty = pose->neck.y;
    FxRamp ramp = fx_ramp(FX_HERO);
    SDL_Color rim = fx_mix(ramp.base, ramp.lit, 0.50f);

    /* The outline a unit outside every row, and the rows above and below the
       ends as wide as the ends themselves, so the corners stay taken off. */
    jacket_row(view, tx, ty - 1.0f, 0, 0.0f, view->ink);
    for (int row = 0; row < JACKET_ROWS; ++row)
        jacket_row(view, tx, ty, row, 1.0f, view->ink);
    jacket_row(view, tx, ty + 1.0f, JACKET_ROWS - 1, 0.0f, view->ink);

    /* The crown the ceiling reaches, the underside it does not, and in between
       the garment with one rim down the flank he faces. */
    for (int row = 0; row < JACKET_ROWS; ++row)
    {
        SDL_Color c = row < 2 ? ramp.lit
                              : (row == JACKET_ROWS - 1 ? ramp.dark : ramp.base);
        jacket_row(view, tx, ty, row, 0.0f, c);
        if (row >= 2 && row < JACKET_ROWS - 1)
            chuck_view_rect(view, tx + JACKET_W - JACKET_FRONT_IN[row] - 1.0f,
                            ty + (float)row, 1.0f, 1.0f, rim);
    }

    /* A shoulder is the top of a torso, not a stripe down the length of it. */
    chuck_view_rect(view, tx + 1.0f, ty + 2.0f, 8.0f, 2.0f, FX_HERO_LT);
    /* The webbing from the near shoulder to the far hip: two pixels standing
       upright are a stripe, on the diagonal they are a strap, and the strap is
       the one line that says the jacket is rigged for a job. */
    ChuckPoint strap_a = {tx + 9.5f, ty + 2.0f};
    ChuckPoint strap_b = {tx + 2.5f, ty + 7.5f};
    chuck_view_band(view, strap_a, strap_b, 2.6f, STRAP);
    chuck_view_band(view, (ChuckPoint){strap_a.x, strap_a.y - 1.0f},
                    (ChuckPoint){strap_b.x, strap_b.y - 1.0f}, 0.7f, STRAP_LT);
    /* The lapel notch, so the jacket has a front to it. */
    chuck_view_rect(view, tx + 8.6f, ty + 3.0f, 1.6f, 3.5f, LAPEL);
    /* The belt, following the jacket's own edge rather than squaring it off,
       and the hem under it where the jacket stops. */
    jacket_row(view, tx, ty, JACKET_BELT_ROW, 0.0f, FX_AMBER);
    jacket_row(view, tx, ty, JACKET_BELT_ROW + 1, 0.0f, FX_AMBER);
    jacket_row(view, tx, ty, JACKET_BELT_ROW, 0.0f, BELT_LT);
    jacket_row(view, tx, ty, JACKET_ROWS - 1, 0.0f, HEM);
}

/* ---- The head --------------------------------------------------------- */

/*
 * The head, built as every head in the building is: an eight-by-seven block of
 * face coming to a chin, a dome of hair laid over the top of it, and the few
 * marks a profile needs — the brow the fringe shades, a nose that breaks the
 * outline rather than sitting inside it, an eye, a mouth, and the headband over
 * all of it with its tail trailing the run.
 *
 * The eye is sized differently at the two scales on purpose. At one pixel to
 * the unit it is three by two with the pupil two wide, because anything smaller
 * disappears; the sector's eye scaled up by half again is a pair of goggles, so
 * the film's is set in finer units and comes out at about the same number of
 * screen pixels.
 */
/* The top-left of the face block. The head sits over the front half of the
   jacket, and a little further forward of it the more he leans into a run. */
static ChuckPoint head_origin(const ChuckPose *pose)
{
    return (ChuckPoint){jacket_x(pose) + 3.0f +
                            (pose->neck.x - pose->pelvis.x) * 0.3f,
                        pose->neck.y - 7.0f};
}

ChuckPoint chuck_head_crown(const ChuckPose *pose)
{
    ChuckPoint face = head_origin(pose);
    /* The hair stands four units over the face block, and the face is eight
       wide. */
    return (ChuckPoint){face.x + 4.0f, face.y - 4.0f};
}

void chuck_draw_head(const ChuckView *view, const ChuckPose *pose, bool blink)
{
    chuck_draw_head_as(view, pose, blink, CHUCK_FACE_EASY);
}

void chuck_draw_head_as(const ChuckView *view, const ChuckPose *pose,
                        bool blink, ChuckFace face)
{
    ChuckPoint face_at = head_origin(pose);
    float fx = face_at.x;
    float fy = face_at.y;
    bool fine = view->scale >= 1.3f;

    /* The face first, coming to a chin, then the hair over it: laid on second
       its fill covers the face's own top outline instead of being cut by it. */
    draw_block(view, fx, fy, 8.0f, 7.0f, 0, 2, FX_SKIN, BLOCK_INK);
    draw_block(view, fx, fy, 8.0f, 7.0f, 0, 2, FX_SKIN, BLOCK_LIT);
    draw_block(view, fx - 1.0f, fy - 4.0f, 10.0f, 5.0f, 3, 0,
               view->ink, BLOCK_FLAT);
    draw_block(view, fx, fy - 3.0f, 8.0f, 4.0f, 2, 0, FX_HAIR,
               BLOCK_FLAT);
    chuck_view_rect(view, fx + 2.0f, fy - 3.0f, 4.0f, 1.0f, HAIR_LT);
    /* The back of the skull stays hair the whole way down to the nape. */
    chuck_view_rect(view, fx, fy + 2.0f, 2.0f, 3.0f, FX_HAIR);
    /* The brow the fringe shades, and the jaw stepping back into shadow. */
    chuck_view_rect(view, fx + 2.0f, fy + 2.0f, 6.0f, 1.0f, BROW);
    draw_block(view, fx, fy + 5.0f, 8.0f, 2.0f, 1, 2, FX_SKIN_DK,
               BLOCK_FLAT);
    /* The nose breaks the outline, or the face is a box with an eye in it. */
    chuck_view_rect(view, fx + 8.0f, fy + 2.0f, fine ? 1.5f : 2.0f, 4.0f,
                    view->ink);
    chuck_view_rect(view, fx + 7.0f, fy + 3.0f, 2.0f, 2.0f, FX_SKIN);

    switch (face)
    {
    case CHUCK_FACE_ALARM:
        /* The white opened above and below a pupil shrunk to a point, and
           the mouth dropped open: the one face nobody has to be told. */
        if (fine)
        {
            chuck_view_rect(view, fx + 4.2f, fy + 2.7f, 2.9f, 2.0f,
                            EYE_WHITE);
            chuck_view_rect(view, fx + 5.9f, fy + 3.2f, 1.1f, 1.1f, PUPIL);
            chuck_view_rect(view, fx + 3.4f, fy + 5.3f, 2.0f, 1.9f,
                            MOUTH_OPEN);
        }
        else
        {
            chuck_view_rect(view, fx + 4.0f, fy + 2.0f, 3.0f, 3.0f,
                            EYE_WHITE);
            chuck_view_rect(view, fx + 5.0f, fy + 3.0f, 1.0f, 1.0f, PUPIL);
            chuck_view_rect(view, fx + 3.0f, fy + 5.0f, 2.0f, 2.0f,
                            MOUTH_OPEN);
        }
        break;
    case CHUCK_FACE_FURY:
        /* The brow comes down toward the nose and closes the eye to a slit
           under it, and the mouth goes wide and flat. It is the brow that
           does it: a narrowed eye alone reads as tired. */
        if (fine)
        {
            chuck_view_band(view, (ChuckPoint){fx + 3.6f, fy + 2.3f},
                            (ChuckPoint){fx + 7.4f, fy + 3.3f}, 0.9f,
                            BROW_KNIT);
            chuck_view_rect(view, fx + 4.4f, fy + 3.6f, 2.6f, 0.9f,
                            EYE_WHITE);
            chuck_view_rect(view, fx + 5.8f, fy + 3.4f, 1.2f, 1.1f, PUPIL);
            chuck_view_rect(view, fx + 2.6f, fy + 6.0f, 3.6f, 0.8f,
                            MOUTH_OPEN);
        }
        else
        {
            chuck_view_rect(view, fx + 4.0f, fy + 2.0f, 4.0f, 1.0f,
                            BROW_KNIT);
            chuck_view_rect(view, fx + 4.0f, fy + 3.0f, 3.0f, 1.0f,
                            EYE_WHITE);
            chuck_view_rect(view, fx + 5.0f, fy + 3.0f, 2.0f, 1.0f, PUPIL);
            chuck_view_rect(view, fx + 2.0f, fy + 6.0f, 4.0f, 1.0f,
                            MOUTH_OPEN);
        }
        break;
    case CHUCK_FACE_EASY:
        /* Mostly pupil, at the front of the white: a dark dot centred in it
           reads as two eyes seen head-on. */
        if (blink)
            chuck_view_rect(view, fx + (fine ? 4.5f : 4.0f), fy + 4.0f,
                            fine ? 2.6f : 3.0f, fine ? 0.6f : 1.0f, LID);
        else if (fine)
        {
            chuck_view_rect(view, fx + 4.4f, fy + 3.2f, 2.6f, 1.3f,
                            EYE_WHITE);
            chuck_view_rect(view, fx + 5.7f, fy + 3.2f, 1.3f, 1.3f, PUPIL);
        }
        else
        {
            chuck_view_rect(view, fx + 4.0f, fy + 3.0f, 3.0f, 2.0f,
                            EYE_WHITE);
            chuck_view_rect(view, fx + 5.0f, fy + 3.0f, 2.0f, 2.0f, PUPIL);
        }
        chuck_view_rect(view, fx + 3.0f, fy + 6.0f, 3.0f, 1.0f, MOUTH);
        break;
    }

    /* The headband goes on last, across the hairline and the brow both. */
    chuck_view_rect(view, fx - 2.0f, fy, 12.0f, 2.0f, FX_RED);
    chuck_view_rect(view, fx - 2.0f, fy, 12.0f, 1.0f, BAND_LT);
    /* The loose end is tied into the band, however hard it trails. */
    chuck_view_rect(view, fx - 5.0f - pose->tail * 0.6f, fy + 1.0f,
                    3.5f + pose->tail * 0.6f, 2.0f, BAND_TAIL);
}

void chuck_draw(const ChuckView *view, const ChuckPose *pose, bool blink)
{
    chuck_draw_arm(view, pose, CHUCK_FAR, CHUCK_HAND_OPEN);
    chuck_draw_legs(view, pose);
    chuck_draw_torso(view, pose);
    chuck_draw_head(view, pose, blink);
    chuck_draw_arm(view, pose, CHUCK_NEAR, CHUCK_HAND_OPEN);
}
