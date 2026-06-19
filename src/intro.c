/*
 * Chuck's title screen is the shot the story starts from: the tower his wife
 * was walked into, seen from the pavement he stopped on.
 *
 * It is drawn the way a BBC Micro title page of 1985 was drawn, because every
 * other screen in the game is: a black sky, the city behind in blue, the tower
 * a blue frame with its windows burning yellow, and a wordmark built out of the
 * machine's own fat pixels in bands of the eight colours. The planes still sit a
 * step in front of one another — dithered blue far away, black masses with a
 * blue rim nearer, the tower in solid colour, the street darkest of all — so
 * the eye is still led from the wordmark down the building to the one lit
 * window that matters and out to the man looking up at it. What a modern frame
 * did with glows and gradients this one does with the three things the machine
 * had instead: a dither for anything that is a material rather than a colour,
 * a flashing colour for anything that has to shout, and the palette for a fade.
 *
 * The whole shot is composed in BBC pixels — four logical pixels across and
 * two down — rather than drawn in logical ones and snapped, because a building
 * whose windows land wherever the rounding puts them is a building whose bays
 * come out eight and nine columns wide at random. Only three things on it are
 * interface: the wordmark, the start plate and the line of chips under it, and
 * their rectangles are the ones the hit tests read, unchanged.
 */
#include "intro.h"

#include <math.h>

#include "bbc.h"
#include "bbc_cast.h"
#include "cutscene.h"

/* ---- The grid ---------------------------------------------------------- */

/* A BBC column or row, as the logical coordinate its top-left corner sits on. */
#define COL(c) ((float)(c) * (float)BBC_PX_W)
#define ROW(r) ((float)(r) * (float)BBC_PX_H)

static void cells(SDL_Renderer *r, BbcColour c, int col, int row, int w, int h)
{
    bbc_block(r, c, COL(col), ROW(row), w, h);
}

static void cells_dither(SDL_Renderer *r, BbcColour base, BbcColour ink,
                         BbcDither pattern, int col, int row, int w, int h)
{
    if (w < 1 || h < 1)
        return;
    bbc_dither(r, base, ink, pattern, COL(col), ROW(row), COL(w), ROW(h));
}

static void cells_stipple(SDL_Renderer *r, BbcColour ink, BbcDither pattern,
                          int col, int row, int w, int h)
{
    if (w < 1 || h < 1)
        return;
    bbc_stipple(r, ink, pattern, COL(col), ROW(row), COL(w), ROW(h));
}

/* One deterministic decision per thing, so a window that is lit on one frame
 * is lit on the next and the wet street can ask the same question again. */
static unsigned hash_u(unsigned value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

/* Nearest whole column to a logical x, which is how anything that drifts —
 * a star, a cloud, a pacing silhouette — moves: a whole BBC pixel at a time. */
static int col_of(float x)
{
    return (int)floorf(x / (float)BBC_PX_W + 0.5f);
}

/* ---- Layout constants the interface keeps ------------------------------ */

/* The tower: eight office floors over a lit lobby, stepping in a column every
 * two floors because the shot looks up at it from street level. */
#define TOWER_FLOORS 8
#define TOWER_PANES 4
#define FLOOR_ROWS 15
#define SPANDREL_ROWS 4
#define PANE_ROWS 10

/* The window the whole composition points at. */
#define TARGET_FLOOR 1
#define TARGET_PANE 2

/* Half the tower's width at each floor, in columns, roof to lobby. Unsized, so
 * the assertion below measures the table rather than itself. */
static const int TOWER_HALF[] = {22, 22, 23, 23, 24, 24, 25, 25, 25};
_Static_assert(sizeof(TOWER_HALF) / sizeof(TOWER_HALF[0]) ==
                   (size_t)TOWER_FLOORS + 1,
               "every floor of the tower, and the lobby, needs a width");

/*
 * The start plate is sized from its own contents rather than by eye: the
 * chevron, a gap, and the longest label the screen can show, plus equal
 * padding on both sides.  Guessing the width is how the label ends up jammed
 * against one edge with the chevron hanging off the other.
 */
#define START_LABEL "PRESS ENTER TO START"
#define START_TRACK 2.0f
#define START_CHEVRON_W 12.0f
#define START_CHEVRON_GAP 3.0f
#define START_PAD 18.0f
#define START_LABEL_W ((float)(sizeof(START_LABEL) - 1) * \
                       ((float)BBC_FONT_CELL + START_TRACK) - \
                       START_TRACK)
#define START_PLATE_W (START_CHEVRON_W + START_CHEVRON_GAP + START_LABEL_W + \
                       START_PAD * 2.0f)

/*
 * The way into the manual is one keycap and a label, at exactly the weight the
 * row of control hints used to have — because it is the same thing, moved.
 *
 * It replaced that row rather than joining it.  A second plate under START, a
 * hint row under that and the street under all three left the bottom eighty
 * pixels of the shot carrying three bands of interface, which reads as a menu
 * stacked on a picture; and the hints themselves were only ever there because
 * there was nowhere else to learn the controls.  Now there is, and it is named
 * on the line where they used to be.
 */
#define MANUAL_LABEL "FIELD MANUAL"
#define MANUAL_TRACK 1.0f
#define MANUAL_KEY_W 20.0f
#define MANUAL_KEY_GAP 9.0f
#define MANUAL_ROW_H 18.0f
/* Padding on the hit rect, which the chip's border is drawn round: the frame a
 * player sees is the rectangle the mouse can hit. */
#define MANUAL_HIT_PAD 10.0f

/*
 * There is deliberately no compile-time width for a chip here.
 *
 * There used to be one per label, sized from `sizeof(LABEL) - 1` against a
 * fixed `MANUAL_KEY_W` — which is the measurement this screen was explicitly
 * rewritten to stop making, because a keycap is as wide as the letter on it
 * and the letter is not known until `pad_hint` has spelled it. `chip_width`
 * below is the live answer, and a second one sitting up here in macros was the
 * stale half of exactly the disagreement the token system exists to prevent.
 */

/* The options sheet shares the manual's line and its hint weight: the pair is
 * still one quiet line of things to know about, not a menu. */
#define OPTIONS_LABEL "OPTIONS"
#define QUIT_LABEL "QUIT"
#define PROMPT_ROW_GAP 18.0f

typedef struct
{
    float w, h;
    int cx;     /* the column everything on the screen is composed on */
    int street; /* the first row of pavement; the tower stands on it   */
    int roof;   /* the cornice's top row                               */
} IntroScene;

/* A rectangle of BBC pixels. */
typedef struct
{
    int col, row, w, h;
} Cells;

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static float smoothstep01(float value)
{
    float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

/*
 * Letterspaced type, for the few lines that have to read as type rather than
 * as a prompt. The system font is the one thing on the screen not on the BBC
 * grid, and tracking it a pixel or two is a type decision rather than a grid
 * one, so it stays — the plate and the chips were measured against it.
 */
static float tracked_width(const char *text, float scale, float track)
{
    size_t len = SDL_strlen(text);
    if (len == 0)
        return 0.0f;
    float step = (float)BBC_FONT_CELL * scale + track;
    return (float)len * step - track;
}

static void draw_tracked(SDL_Renderer *r, float x, float y, float scale,
                         float track, BbcColour colour, const char *text)
{
    float step = (float)BBC_FONT_CELL * scale + track;
    char glyph[2] = {0, 0};
    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        glyph[0] = text[i];
        bbc_text(r, x + step * (float)i, y, scale, colour, glyph);
    }
}

static void draw_tracked_centered(SDL_Renderer *r, float cx, float y,
                                  float scale, float track, BbcColour colour,
                                  const char *text)
{
    draw_tracked(r, cx - tracked_width(text, scale, track) * 0.5f, y, scale,
                 track, colour, text);
}

static IntroScene scene_layout(int win_w, int win_h)
{
    IntroScene s;
    s.w = win_w > 0 ? (float)win_w : 800.0f;
    s.h = win_h > 0 ? (float)win_h : 552.0f;
    /*
     * Dead centre.  The wordmark, the start plate and the manual line all
     * sit on the screen's centre line, so a subject nudged off it does not
     * read as composition — it reads as something that failed to line up.
     * The asymmetry the shot needs is carried on the street instead: the lamp
     * and the man on the left, the car at the kerb on the right.
     */
    s.cx = (int)(s.w / (float)BBC_PX_W) / 2;
    s.street = (int)((s.h - 92.0f) / (float)BBC_PX_H);
    s.roof = 83;
    return s;
}

static int floor_row(const IntroScene *s, int fl)
{
    return s->roof + 5 + FLOOR_ROWS * fl;
}

/*
 * A bay of glazing. Narrow tower, four bays a floor, two-column piers between
 * them and whatever is left over on the corners — eight columns of glass up
 * top and nine at the foot, which is the taper read in the windows as well as
 * in the outline.
 */
static Cells pane_cells(const IntroScene *s, int fl, int pane)
{
    int half = TOWER_HALF[fl];
    int w = half >= 24 ? 9 : 8;
    int edge = half - 2 * w - 3;
    Cells c = {s->cx - half + edge + pane * (w + 2),
               floor_row(s, fl) + SPANDREL_ROWS, w, PANE_ROWS};
    return c;
}

typedef enum
{
    PANE_DARK = 0,
    PANE_WARM,
    PANE_COOL
} PaneLight;

/*
 * One deterministic decision per pane.  The wet pavement reflects exactly the
 * windows that are lit, and it can work that out by asking the same question
 * again rather than by the renderer keeping a list.
 */
static PaneLight pane_light(int fl, int pane, float time, bool *stutter)
{
    unsigned h = hash_u((unsigned)(fl * 37 + pane * 7 + 11) * 2654435761u);
    *stutter = false;
    /* Sparse on purpose: the title is a building asleep, and the one window
     * that matters has to be the brightest thing on the wall. */
    if ((h % 100u) >= 30u)
        return PANE_DARK;
    /* The building wakes up window by window while the title assembles. */
    if (time < 0.55f + (float)(h % 13u) * 0.06f)
        return PANE_DARK;
    /* A tube stutters: the room drops to its own dark colour for a beat. */
    *stutter = (h & 64u) != 0u &&
               fmodf(time * 1.7f + (float)(h % 97u) * 0.06f, 5.4f) < 0.08f;
    return (h & 8u) ? PANE_WARM : PANE_COOL;
}

/*
 * A keycap is as wide as the letter on it.
 *
 * `$Y` is one cell on an Xbox pad and two on a PlayStation, `$SELECT` is six —
 * or seven, spelled OPTIONS. A fixed-width cap was fine while the line only
 * carried single letters and quietly wrong the moment it did not, which is the
 * same rule the manual's control table already keeps: measure *after*
 * spelling, never before.
 */
static float chip_key_width(const char *key)
{
    float text = (float)SDL_strlen(key) * (float)BBC_FONT_CELL;
    float wanted = text + 12.0f;
    return wanted < MANUAL_KEY_W ? MANUAL_KEY_W : wanted;
}

static float chip_width(const char *key, const char *label)
{
    float label_w = (float)SDL_strlen(label) *
                        ((float)BBC_FONT_CELL + MANUAL_TRACK) -
                    MANUAL_TRACK;
    return chip_key_width(key) + MANUAL_KEY_GAP + label_w +
           MANUAL_HIT_PAD * 2.0f;
}

/*
 * Two things, on the screen's centre line: the start plate on the street, and
 * the quiet line of things to know about under it.  Both keep the line the
 * wordmark and the tower are composed on, and the forty pixels between them
 * are what stop the pair reading as a menu rather than as one thing to press
 * and some things to know about.
 *
 * The resume chip joins that same line rather than earning a band of its own:
 * a plate, a second plate and a keycap row is three bands of interface across
 * the bottom of the shot, which is the composition this screen was rebuilt to
 * get rid of. It is the same hint weight as its two neighbours, and like them
 * it is a thing to know about — the campaign is longer than one sitting, and
 * this is where the game says so.
 *
 * The quit chip joins it on the same terms and for the same reason the line
 * exists at all: ESC has always closed the game from here and this screen
 * never said so, and a pad in fullscreen had no ESC to press. Four chips at
 * their widest — a PlayStation pad spelling OPTIONS for SELECT — come to about
 * 660 of the 800 the frame is laid out in, so it is still one line and still
 * one band.
 */
static void place_buttons(Intro *intro, int w, int h, const PadHints *pad)
{
    intro->start_button.x = ((float)w - intro->start_button.w) * 0.5f;
    intro->start_button.y = (float)h - 78.0f;

    char manual_key[8];
    char options_key[8];
    char resume_key[12];
    char quit_key[8];
    intro->manual_button.w =
        chip_width(pad_hint(pad, manual_key, sizeof(manual_key), "$Y", "H"),
                   MANUAL_LABEL);
    intro->options_button.w =
        chip_width(pad_hint(pad, options_key, sizeof(options_key), "$X", "J"),
                   OPTIONS_LABEL);
    intro->resume_button.w =
        intro->resume_offered
            ? chip_width(pad_hint(pad, resume_key, sizeof(resume_key),
                                  "$SELECT", "R"),
                         intro->resume_label)
            : 0.0f;
    intro->quit_button.w =
        intro->quit_offered
            ? chip_width(pad_hint(pad, quit_key, sizeof(quit_key), "$B", "ESC"),
                         QUIT_LABEL)
            : 0.0f;
    intro->manual_button.h = MANUAL_ROW_H;
    intro->options_button.h = MANUAL_ROW_H;
    intro->resume_button.h = intro->resume_offered ? MANUAL_ROW_H : 0.0f;
    intro->quit_button.h = intro->quit_offered ? MANUAL_ROW_H : 0.0f;

    /* Centred as one line, resume first: it is the one anybody coming back to
     * the game is looking for. Quit last, which is where the one thing on this
     * screen that cannot be taken back belongs. */
    float line_w = intro->manual_button.w + PROMPT_ROW_GAP +
                   intro->options_button.w;
    if (intro->quit_offered)
        line_w += PROMPT_ROW_GAP + intro->quit_button.w;
    if (intro->resume_offered)
        line_w += intro->resume_button.w + PROMPT_ROW_GAP;

    float x = ((float)w - line_w) * 0.5f;
    float y = (float)h - 30.0f;
    if (intro->resume_offered)
    {
        intro->resume_button.x = x;
        intro->resume_button.y = y;
        x += intro->resume_button.w + PROMPT_ROW_GAP;
    }
    intro->manual_button.x = x;
    intro->manual_button.y = y;
    x += intro->manual_button.w + PROMPT_ROW_GAP;
    intro->options_button.x = x;
    intro->options_button.y = y;
    x += intro->options_button.w + PROMPT_ROW_GAP;
    intro->quit_button.x = x;
    intro->quit_button.y = y;
}

/* The chip's own words. Nothing is offered for the lobby, because arriving in
 * the lobby is what pressing START already does. */
static void set_resume(Intro *intro, int resume_sector)
{
    intro->resume_offered = resume_sector > 0;
    if (!intro->resume_offered)
    {
        intro->resume_label[0] = '\0';
        return;
    }
    SDL_snprintf(intro->resume_label, sizeof(intro->resume_label),
                 "RESUME SECTOR %d", resume_sector + 1);
}

void intro_init(Intro *intro, int win_w, int win_h, const PadHints *pad,
                int resume_sector)
{
    SDL_zerop(intro);
    int w = win_w > 0 ? win_w : 800;
    int h = win_h > 0 ? win_h : 552;

    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        IntroStar *star = &intro->stars[i];
        star->x = (float)SDL_rand(w);
        /* Everything below the horizon is masonry, so the field is packed
         * into the band of sky that is actually visible. */
        star->y = (float)SDL_rand(i < 118 ? 210 : h / 2);
        star->speed = 2.0f + (float)(i % 5) * 1.7f + (float)SDL_rand(3);
        star->phase = (float)SDL_rand(628) * 0.01f;
    }

    intro->start_button.w = START_PLATE_W;
    intro->start_button.h = 34.0f;
    intro->quit_offered = CHUCK_OFFERS_QUIT;
    set_resume(intro, resume_sector);
    place_buttons(intro, w, h, pad);
}

bool intro_update(Intro *intro, float dt, int win_w, int win_h,
                  float mouse_x, float mouse_y, const PadHints *pad,
                  int resume_sector, bool quit_held)
{
    intro->time += dt;

    int w = win_w > 0 ? win_w : 800;
    int h = win_h > 0 ? win_h : 552;
    set_resume(intro, resume_sector);
    place_buttons(intro, w, h, pad);

    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        IntroStar *star = &intro->stars[i];
        star->x += star->speed * dt;
        if (star->x > (float)w)
            star->x = 0.0f;
    }

    intro->start_hovered = intro_hit_start_button(intro, mouse_x, mouse_y);
    intro->manual_hovered = intro_hit_manual_button(intro, mouse_x, mouse_y);
    intro->options_hovered = intro_hit_options_button(intro, mouse_x, mouse_y);
    intro->resume_hovered = intro_hit_resume_button(intro, mouse_x, mouse_y);
    intro->quit_hovered = intro_hit_quit_button(intro, mouse_x, mouse_y);

    /*
     * The hold. It runs back down at the same rate it ran up, so a thumb that
     * came off the button has visibly undone what it did — a fill that stayed
     * where it was left would mean the next brush of B finished the job.
     *
     * Nothing counts until B has been seen up at least once: this screen is
     * what closing the manual and closing the options sheet both hand back to,
     * and both are closed with B.
     */
    bool done = false;
    if (!intro->quit_offered)
        quit_held = false;
    if (!quit_held)
        intro->quit_armed = true;
    else if (!intro->quit_armed)
        quit_held = false;

    if (quit_held)
    {
        intro->quit_hold += dt;
        if (intro->quit_hold >= TITLE_QUIT_HOLD_TIME)
        {
            intro->quit_hold = TITLE_QUIT_HOLD_TIME;
            done = true;
        }
    }
    else if (intro->quit_hold > 0.0f)
    {
        intro->quit_hold -= dt;
        if (intro->quit_hold < 0.0f)
            intro->quit_hold = 0.0f;
    }
    return done;
}

static bool hit_plate(const SDL_FRect *button, float x, float y)
{
    return x >= button->x && x <= button->x + button->w &&
           y >= button->y && y <= button->y + button->h;
}

bool intro_hit_start_button(const Intro *intro, float x, float y)
{
    return hit_plate(&intro->start_button, x, y);
}

bool intro_hit_manual_button(const Intro *intro, float x, float y)
{
    return hit_plate(&intro->manual_button, x, y);
}

bool intro_hit_options_button(const Intro *intro, float x, float y)
{
    return hit_plate(&intro->options_button, x, y);
}

bool intro_hit_resume_button(const Intro *intro, float x, float y)
{
    return intro->resume_offered && hit_plate(&intro->resume_button, x, y);
}

bool intro_hit_quit_button(const Intro *intro, float x, float y)
{
    return intro->quit_offered && hit_plate(&intro->quit_button, x, y);
}

/* ---- Sky ---------------------------------------------------------------- */

/*
 * The moon: a white disc with its own cyan shadow showing down the lower limb,
 * so it reads as a ball rather than a coin, and three seas in cyan. Up and to
 * the left, where every lit thing in this game takes its light from.
 */
static void draw_moon(SDL_Renderer *r, float cx, float cy, float radius)
{
    bbc_disc(r, BBC_CYAN, cx, cy, radius);
    bbc_disc(r, BBC_WHITE, cx - 4.0f, cy - 2.0f, radius - 2.0f);
    bbc_block(r, BBC_CYAN, cx - 8.0f, cy - 8.0f, 2, 2);
    bbc_block(r, BBC_CYAN, cx + 4.0f, cy + 2.0f, 1, 3);
    bbc_block(r, BBC_CYAN, cx - 4.0f, cy + 4.0f, 1, 1);
}

/*
 * High cloud as the machine drew it: stepped rows of blue checkerboard laid
 * over whatever is behind, so it greys the stars and takes a bite out of the
 * moon as it passes rather than hiding either.
 */
static void draw_cloud(SDL_Renderer *r, int col, int row, int width)
{
    static const float SHAPE[6][2] = {
        {0.30f, 0.40f}, {0.16f, 0.68f}, {0.04f, 0.92f},
        {0.00f, 1.00f}, {0.10f, 0.78f}, {0.26f, 0.44f}};
    for (int i = 0; i < 6; ++i)
    {
        int c0 = col + (int)(SHAPE[i][0] * (float)width);
        int w = (int)(SHAPE[i][1] * (float)width);
        cells_stipple(r, BBC_BLUE, i == 0 ? BBC_DITHER_SPARSE : BBC_DITHER_CHECK,
                      c0, row + i * 2, w, 2);
    }
}

static void render_sky(SDL_Renderer *r, const Intro *intro, const IntroScene *s)
{
    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, s->w, s->h);

    /* Stars, one BBC pixel each, in three distances: blue, cyan and white,
     * and the odd warm one. A twinkle is a step up the ladder for a beat,
     * never a fade. */
    for (int i = 0; i < INTRO_STAR_COUNT; ++i)
    {
        const IntroStar *star = &intro->stars[i];
        int depth = i % 5;
        float twinkle = sinf(intro->time * (0.65f + (float)(i % 7) * 0.13f) +
                             star->phase);
        BbcColour c = depth < 2 ? BBC_BLUE : (depth < 4 ? BBC_CYAN : BBC_WHITE);
        if (i % 7 == 0)
            c = BBC_YELLOW;
        if (twinkle > 0.82f)
            c = depth < 2 ? BBC_CYAN : BBC_WHITE;
        else if (twinkle < -0.9f && depth < 2)
            continue;
        float sx = bbc_snap_x(star->x);
        float sy = bbc_snap_y(star->y);
        bbc_plot(r, c, sx, sy);
        /* The brightest few carry a cross while they flare. */
        if (depth == 4 && i % 3 == 0 && twinkle > 0.88f)
        {
            bbc_plot(r, BBC_BLUE, sx - 4.0f, sy);
            bbc_plot(r, BBC_BLUE, sx + 4.0f, sy);
            bbc_plot(r, BBC_BLUE, sx, sy - 2.0f);
            bbc_plot(r, BBC_BLUE, sx, sy + 2.0f);
        }
    }

    draw_moon(r, s->w * 0.135f, 88.0f, 20.0f);

    /* Three sheets of high cloud drift across, one of them past the moon. */
    static const float CLOUDS[3][3] = {
        {40.0f, 62.0f, 168.0f},
        {430.0f, 122.0f, 232.0f},
        {620.0f, 40.0f, 128.0f}};
    for (int i = 0; i < 3; ++i)
    {
        float span = s->w + 260.0f;
        float x = fmodf(CLOUDS[i][0] + intro->time * (3.4f + (float)i * 2.1f),
                        span) -
                  200.0f;
        draw_cloud(r, col_of(x), (int)(CLOUDS[i][1] / (float)BBC_PX_H),
                   (int)(CLOUDS[i][2] / (float)BBC_PX_W));
    }

    /* One shooting star every few seconds: a white head and a tail stepping
     * down the ladder behind it, a whole BBC pixel a step. */
    float meteor = fmodf(intro->time, 11.5f);
    if (meteor >= 3.1f && meteor < 4.3f)
    {
        float p = (meteor - 3.1f) / 1.2f;
        float head_x = -12.0f + p * (s->w + 36.0f);
        float head_y = 26.0f + p * 58.0f;
        static const BbcColour TAIL[] = {BBC_WHITE, BBC_WHITE, BBC_CYAN,
                                         BBC_CYAN, BBC_BLUE, BBC_BLUE};
        int len = p > 0.8f ? 3 : (int)(sizeof(TAIL) / sizeof(TAIL[0]));
        for (int i = len - 1; i >= 0; --i)
            bbc_plot(r, TAIL[i], head_x - (float)i * 8.0f,
                     head_y - (float)i * 4.0f);
    }
}

/* ---- The city behind the tower ------------------------------------------ */

static void render_skyline(SDL_Renderer *r, const Intro *intro,
                           const IntroScene *s)
{
    /* Far row: blue on a checkerboard, no detail, only a ragged edge with a
     * solid line along it — distance, in the one colour that means it. */
    static const int FAR_HEIGHTS[] = {49, 66, 53, 74, 58, 70, 51, 80, 56};
    const int far_base = s->street - 27;
    int cursor = -6;
    for (int i = 0; cursor < 210; ++i)
    {
        int width = 15 + (i * 17) % 9;
        int height = FAR_HEIGHTS[i % (int)SDL_arraysize(FAR_HEIGHTS)];
        cells_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, cursor,
                     far_base - height, width, height + 27);
        cells(r, BBC_BLUE, cursor, far_base - height, width, 1);
        cursor += width + 1;
    }

    /* Near row: black masses with a blue rim, with the last windows still
     * burning and the odd aviation light on a mast. */
    static const int HEIGHTS[] = {22, 34, 26, 41, 29, 37, 24, 45, 31, 39, 27};
    const int base = s->street - 9;
    cursor = -3;
    for (int i = 0; cursor < 210; ++i)
    {
        int width = 12 + (i * 13) % 8;
        int height = HEIGHTS[i % (int)SDL_arraysize(HEIGHTS)];
        int top = base - height;
        cells(r, BBC_BLACK, cursor, top, width, height + 9);
        cells(r, BBC_BLUE, cursor, top, width, 1);
        cells(r, BBC_BLUE, cursor + width - 1, top, 1, height + 9);

        for (int w = 0; w < width / 3; ++w)
        {
            unsigned h = hash_u((unsigned)(i * 53 + w * 17));
            if ((h % 6u) != 0u)
                continue;
            cells(r, (h & 8u) ? BBC_YELLOW : BBC_CYAN, cursor + 1 + w * 3,
                  top + 5 + (int)(h % 4u) * 6, 1, 1);
        }
        if (i % 4 == 1)
        {
            int ax = cursor + width / 2;
            cells(r, BBC_BLUE, ax, top - 6, 1, 6);
            bool lit = sinf(intro->time * 1.8f + (float)i) > 0.75f;
            cells(r, lit ? BBC_RED : BBC_BLACK, ax, top - 7, 1, 1);
        }
        cursor += width + 2;
    }
}

/*
 * The blocks the tower shares its street with.  They are what the tower gets
 * to overlap, which is the only way a flat drawing says one thing is in front
 * of another: black masses with a blue edge, and only the odd window lit, in
 * red or blue rather than yellow — a bright dot back here competes with the
 * window the shot is about.
 */
static void draw_neighbour_block(SDL_Renderer *r, const IntroScene *s, int col,
                                 int top, int width, unsigned seed)
{
    cells(r, BBC_BLACK, col, top, width, s->street - top);
    cells(r, BBC_BLUE, col, top, width, 1);
    cells(r, BBC_BLUE, col + width - 1, top, 1, s->street - top);

    /* Roof plant, then whatever is left burning inside. */
    cells(r, BBC_BLACK, col + width / 2 - 3, top - 4, 6, 4);
    cells(r, BBC_BLUE, col + width / 2 - 3, top - 4, 6, 1);
    cells(r, BBC_BLUE, col + width / 2 + 2, top - 4, 1, 4);

    int cols = (width - 2) / 4;
    int rows = (s->street - top - 4) / 11;
    for (int row = 0; row < rows; ++row)
    {
        for (int c = 0; c < cols; ++c)
        {
            unsigned h = hash_u(seed + (unsigned)(row * 29 + c * 7));
            if ((h % 8u) != 0u)
                continue;
            if (h & 8u)
                cells_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_SPARSE,
                             col + 2 + c * 4, top + 6 + row * 11, 2, 2);
            else
                cells(r, BBC_BLUE, col + 2 + c * 4, top + 6 + row * 11, 2, 2);
        }
    }
}

static void render_flanking_blocks(SDL_Renderer *r, const IntroScene *s)
{
    /*
     * Mid-ground slabs at four different heights, so the rooflines step
     * unevenly up to the tower on both sides instead of mirroring it.
     */
    draw_neighbour_block(r, s, 29, 171, 30, 41u);
    draw_neighbour_block(r, s, s->cx - 44, 131, 30, 17u);
    draw_neighbour_block(r, s, s->cx + 20, 156, 33, 91u);
    draw_neighbour_block(r, s, s->cx + 49, 179, 26, 63u);

    /* Left: a low block with a water tank on its roof, cut by the frame. */
    int right_col = (int)(s->w / (float)BBC_PX_W);
    cells(r, BBC_BLACK, 0, 151, 35, s->street - 151);
    cells(r, BBC_BLUE, 0, 151, 35, 1);
    cells(r, BBC_BLUE, 34, 151, 1, s->street - 151);
    cells(r, BBC_BLACK, 10, 137, 9, 14);
    cells(r, BBC_BLUE, 10, 137, 9, 1);
    cells(r, BBC_BLUE, 18, 137, 1, 9);
    cells(r, BBC_BLUE, 12, 146, 1, 5);
    cells(r, BBC_BLUE, 16, 146, 1, 5);
    cells_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_SPARSE, 4, 174, 2, 2);
    cells(r, BBC_BLUE, 26, 192, 2, 2);

    /* Right: nearer, taller, also cut by the frame, so the composition is
     * closed on both sides instead of draining out of them. */
    int rx = right_col - 35;
    cells(r, BBC_BLACK, rx, 131, 35, s->street - 131);
    cells(r, BBC_BLUE, rx, 131, 35, 1);
    cells(r, BBC_BLUE, rx, 131, 1, s->street - 131);
    cells(r, BBC_BLUE, rx + 11, 113, 1, 18);
    cells(r, BBC_BLUE, rx + 9, 118, 5, 1);
    cells(r, bbc_flash_pair(BBC_RED, BBC_BLACK), rx + 11, 112, 1, 1);
    cells_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_SPARSE, rx + 4, 157, 2, 2);
    cells_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_SPARSE, rx + 9, 180, 2, 2);
    cells(r, BBC_BLUE, rx + 23, 168, 2, 2);
}

/* ---- The tower ---------------------------------------------------------- */

static void draw_roof_hardware(SDL_Renderer *r, const IntroScene *s)
{
    int half = TOWER_HALF[0];
    int left = s->cx - half;
    int right = s->cx + half;
    int roof = s->roof;

    /*
     * Everything on the roof stays inside a dozen rows: the wordmark's
     * tagline sits just above, and a mast tall enough to look right would
     * grow straight into it.
     */
    int mast = left + 7;
    cells(r, BBC_BLUE, mast, roof - 12, 1, 12);
    cells(r, BBC_BLUE, mast - 2, roof - 8, 5, 1);
    /* The one red light a tower this tall carries for the helicopters, on
     * the machine's own flash. */
    cells(r, bbc_flash_pair(BBC_RED, BBC_BLACK), mast, roof - 13, 1, 1);

    /* Water tank on its legs, and the plant room over the lift shaft. */
    cells(r, BBC_BLUE, left + 14, roof - 11, 7, 7);
    cells(r, BBC_CYAN, left + 14, roof - 11, 7, 1);
    cells(r, BBC_BLUE, left + 15, roof - 4, 1, 4);
    cells(r, BBC_BLUE, left + 19, roof - 4, 1, 4);

    cells(r, BBC_BLUE, right - 16, roof - 7, 10, 7);
    cells(r, BBC_CYAN, right - 16, roof - 7, 10, 1);
    cells(r, BBC_BLACK, right - 14, roof - 4, 2, 2);
    cells(r, BBC_BLACK, right - 10, roof - 4, 2, 2);

    /* Satellite dish on the parapet. */
    cells(r, BBC_BLUE, right - 4, roof - 5, 1, 5);
    cells(r, BBC_WHITE, right - 6, roof - 8, 3, 1);
    cells(r, BBC_CYAN, right - 6, roof - 7, 1, 2);

    /* Cornice: overhangs both faces by two columns, lit along its top
     * arris, with its own shadow cut under it. */
    cells(r, BBC_BLUE, left - 2, roof, right - left + 4, 4);
    cells(r, BBC_WHITE, left - 2, roof, right - left + 4, 1);
    cells(r, BBC_CYAN, left - 2, roof + 1, 1, 3);
    cells(r, BBC_BLACK, left - 2, roof + 4, right - left + 4, 1);
}

static void draw_fire_escape(SDL_Renderer *r, const IntroScene *s)
{
    /* Bolted to the left face: it breaks the box silhouette, and it is the
     * one thing on the wall that says this building gets climbed. */
    for (int i = 0; i < TOWER_FLOORS; ++i)
    {
        int y = floor_row(s, i) + FLOOR_ROWS - 1;
        int x = s->cx - TOWER_HALF[i];
        cells(r, BBC_CYAN, x - 6, y, 6, 1);
        cells(r, BBC_BLUE, x - 6, y - 4, 6, 1);
        cells(r, BBC_BLUE, x - 6, y - 4, 1, 4);
        if (i + 1 < TOWER_FLOORS)
        {
            int ny = floor_row(s, i + 1) + FLOOR_ROWS - 1;
            int nx = s->cx - TOWER_HALF[i + 1];
            bbc_line(r, BBC_BLUE, COL(x - 5), ROW(y + 1), COL(nx - 2),
                     ROW(ny - 1));
        }
    }
}

/* A lit office: a ceiling and a room and a floor, not a slab of colour. The
 * room is a dither — orange is red and yellow on a checkerboard, a cool tube
 * is cyan and blue — and only the tube along its head is solid, which keeps
 * every one of them a step under the window the shot is about. */
static void draw_lit_pane(SDL_Renderer *r, Cells p, PaneLight light,
                          bool stutter, int fl, int pane, float time)
{
    unsigned h = hash_u((unsigned)(fl * 91 + pane * 13));
    bool warm = light == PANE_WARM;
    BbcColour tube = warm ? BBC_YELLOW : BBC_WHITE;
    BbcColour base = warm ? BBC_RED : BBC_BLUE;
    BbcColour ink = warm ? BBC_YELLOW : BBC_CYAN;
    if (stutter)
    {
        cells_dither(r, BBC_BLACK, base, BBC_DITHER_CHECK, p.col, p.row, p.w,
                     p.h);
        return;
    }
    cells_dither(r, base, ink, BBC_DITHER_CHECK, p.col, p.row, p.w, p.h);
    cells(r, tube, p.col, p.row, p.w, 1);
    if ((h & 32u) != 0u) /* half-drawn blind */
        cells(r, base, p.col, p.row + 1, p.w, 3);
    cells(r, BBC_BLACK, p.col, p.row + p.h - 3, p.w, 1);

    /* Someone is still working on a couple of the floors. */
    if ((h % 3u) == 0u)
    {
        int walk = (h & 16u) ? (int)floorf(sinf(time * 0.5f +
                                                (float)(h % 31u) * 0.2f) *
                                               (float)(p.w / 4) +
                                           0.5f)
                             : 0;
        int px = p.col + p.w / 2 - 1 + walk;
        cells(r, BBC_BLACK, px, p.row + p.h - 7, 2, 5);
        cells(r, BBC_BLACK, px, p.row + p.h - 9, 1, 2);
    }
}

static void draw_lobby(SDL_Renderer *r, const IntroScene *s, float time,
                       float wake)
{
    int top = floor_row(s, TOWER_FLOORS);
    int half = TOWER_HALF[TOWER_FLOORS];
    int left = s->cx - half;
    int width = half * 2;
    int bottom = s->street; /* exclusive */

    /*
     * Double-height glazing with the room behind it.  This is the brightest
     * large area in the frame, which is what makes the way in obvious — lit
     * from its own ceiling, so the dither thins toward the floor rather than
     * the whole wall being one wash of orange. It comes on in the two steps
     * a palette fade has rather than smoothly.
     */
    cells(r, BBC_BLACK, left, top, width, bottom - top);
    if (wake > 0.66f)
    {
        cells_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_DENSE, left + 1, top + 2,
                     width - 2, 6);
        cells_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, left + 1,
                     top + 8, width - 2, bottom - top - 9);
    }
    else if (wake > 0.33f)
    {
        cells_dither(r, BBC_BLACK, BBC_RED, BBC_DITHER_CHECK, left + 1, top + 2,
                     width - 2, bottom - top - 3);
    }

    /* Ceiling coves, seen through the glass. */
    for (int i = 0; i < 3 && wake > 0.33f; ++i)
        cells(r, wake > 0.66f ? BBC_WHITE : BBC_YELLOW, left + 6 + i * 17, top + 2,
              6, 1);

    /* Reception counter, with whoever is left behind it in silhouette, and
     * the lift core cold at the back of the room. */
    int counter = left + 24;
    int lean = (int)floorf(sinf(time * 0.8f) * 1.2f + 0.5f);
    bbc_sprite_solid(r, &SPR_RECEPTIONIST_IDLE, COL(counter + 4 + lean),
                     ROW(bottom - 9 - SPR_RECEPTIONIST_IDLE.h + 1), 1,
                     BBC_BLACK);
    cells(r, BBC_BLUE, counter, bottom - 9, 14, 8);
    cells(r, wake > 0.33f ? BBC_YELLOW : BBC_BLUE, counter, bottom - 9, 14, 1);

    cells(r, BBC_BLACK, left + width - 10, top + 3, 7, bottom - top - 4);
    cells(r, wake > 0.33f ? BBC_CYAN : BBC_BLUE, left + width - 9, top + 4, 5, 1);

    /* Glazing bars, so the light is read through a wall of glass. */
    for (int mx = left + 4; mx < left + width - 2; mx += 6)
        cells(r, BBC_BLUE, mx, top + 1, 1, bottom - top - 2);
    cells(r, BBC_BLUE, left, top, width, 2);

    /* The entrance itself: two leaves, a mullion, a lit canopy over them.
     * The doorway burns the same solid yellow as the target window — one
     * interior, one warmth, top of the shot to the bottom. */
    int door = left + 6;
    cells(r, BBC_BLACK, door - 1, top + 3, 16, bottom - top - 4);
    cells(r, BBC_BLUE, door - 2, top + 3, 18, 1);
    cells(r, wake > 0.33f ? BBC_YELLOW : BBC_RED, door - 2, top + 4, 18, 1);
    if (wake > 0.0f)
        cells(r, wake > 0.66f ? BBC_YELLOW : BBC_RED, door, top + 6, 14,
              bottom - top - 7);
    cells(r, BBC_BLACK, door + 7, top + 6, 1, bottom - top - 7);
    cells(r, BBC_WHITE, door + 2, top + 7, 1, 3);

    /* Sill line: the building meets the pavement on a grey concrete edge. */
    cells_dither(r, BBC_BLACK, BBC_WHITE, BBC_DITHER_CHECK, left - 1,
                 bottom - 1, width + 2, 1);
}

static void draw_target_window(SDL_Renderer *r, const IntroScene *s,
                               float time)
{
    Cells p = pane_cells(s, TARGET_FLOOR, TARGET_PANE);
    if (time < 1.35f)
        return;
    /* Struck once as it comes on, then held. */
    if (time >= 1.55f && time < 1.62f)
    {
        cells_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_SPARSE, p.col, p.row,
                     p.w, p.h);
        return;
    }

    /* The one solid light on the wall, with white along its head. */
    cells(r, BBC_YELLOW, p.col, p.row, p.w, p.h);
    cells(r, BBC_WHITE, p.col, p.row, p.w, 1);

    /* Two figures: one held in a chair, one standing over her. */
    int base = p.row + p.h;
    cells(r, BBC_BLACK, p.col + 1, base - 5, 2, 5);
    cells(r, BBC_BLACK, p.col + 1, base - 7, 1, 2);
    cells(r, BBC_BLACK, p.col + 3, base - 6, 1, 6);

    int pace = (int)floorf(sinf(time * 0.55f) * 1.4f + 0.5f);
    cells(r, BBC_BLACK, p.col + p.w - 3 + pace, base - 7, 2, 7);
    cells(r, BBC_BLACK, p.col + p.w - 3 + pace, base - 9, 1, 2);
}

static void render_tower(SDL_Renderer *r, const Intro *intro,
                         const IntroScene *s)
{
    float time = intro->time;
    float wake = smoothstep01((time - 0.35f) / 0.8f);

    draw_roof_hardware(r, s);

    /* The roof is two rows of plain wall between the cornice and the top
     * floor. */
    cells(r, BBC_BLUE, s->cx - TOWER_HALF[0], s->roof + 4, TOWER_HALF[0] * 2, 1);

    /* Body: one band per floor, so the taper steps rather than slopes. */
    for (int i = 0; i < TOWER_FLOORS; ++i)
    {
        int top = floor_row(s, i);
        int half = TOWER_HALF[i];
        int left = s->cx - half;
        int width = half * 2;

        /* Spandrel and piers in the one blue, the arris along each floor
         * catching the moon in cyan, the moon-side corner lit and the far
         * one in its own shade. Every bit of brightness on this wall is
         * meant to be a window. */
        cells(r, BBC_BLUE, left, top, width, FLOOR_ROWS);
        cells(r, BBC_CYAN, left, top, width, 1);
        cells(r, BBC_CYAN, left, top, 1, FLOOR_ROWS);
        cells_dither(r, BBC_BLUE, BBC_BLACK, BBC_DITHER_CHECK, left + width - 1,
                     top + 1, 1, FLOOR_ROWS - 1);

        for (int pane = 0; pane < TOWER_PANES; ++pane)
        {
            Cells p = pane_cells(s, i, pane);
            bool stutter = false;
            PaneLight light = pane_light(i, pane, time, &stutter);

            if (i == TARGET_FLOOR && pane == TARGET_PANE)
            {
                cells(r, BBC_BLACK, p.col, p.row, p.w, p.h);
                continue;
            }
            if (light == PANE_DARK)
            {
                /* Dark glass with the sky caught in one corner; without it
                 * the unlit panes read as holes in the wall. */
                unsigned h = hash_u((unsigned)(i * 19 + pane * 5 + 3));
                cells(r, BBC_BLACK, p.col, p.row, p.w, p.h);
                if ((h & 3u) == 0u)
                {
                    cells(r, BBC_BLUE, p.col + p.w - 3, p.row + 1, 1, 2);
                    cells(r, BBC_BLUE, p.col + p.w - 2, p.row + 1, 1, 1);
                }
            }
            else
            {
                draw_lit_pane(r, p, light, stutter, i, pane, time);
            }
            /* The transom, crossing lit and dark panes alike: it is what
             * gives the wall its scale. */
            cells(r, BBC_BLUE, p.col, p.row + 3, p.w, 1);
        }
    }

    draw_fire_escape(r, s);
    draw_lobby(r, s, time, wake);
    draw_target_window(r, s, time);
}

/* ---- Street ------------------------------------------------------------- */

/*
 * The car they arrived in, still at the kerb with the brake lights on — the
 * shot picks up exactly where the drive ends. Rear to the left, nose to the
 * right: a blue body with its roof edge catching the lobby, dark glass with
 * the lobby's light streaked across it, and '1' the tail light, which goes out
 * a few seconds in.
 */
BBC_SPRITE(SPR_TITLE_SUV,
    "....CCCCCCCCCCCCCCCC....",
    "...BBBBBBBBBBBBBBBBBB...",
    "...BKKWKKKKBBKKKKKKKKB..",
    "...BKWKKKKKBBKKKKKKKKKB.",
    "...BWKKKKKKBBKKKKKKKKKKB",
    "...BKKKKKKKBBKKKKKKKKKKB",
    "1BBBBBBBBBBBBBBBBBBBBBBB",
    "1BBBBBBBBBBBKBBBBBBBBBBW",
    "BCCCCCCCCCCCCCCCCCCCCCCB",
    "BBBBBBBBBBBBKBBBBBBBBBBB",
    "BBBBBBBBBBBBKBBBBBBBBBBB",
    "BBKKKKBBBBBBBBBBBBKKKKBB",
    "BKKWWKKBBBBBBBBBBKKWWKKB",
    ".KKWWKK..........KKWWKK.",
    "..KKKK............KKKK..");

static void draw_suv(SDL_Renderer *r, int col, int base_row, float time)
{
    BbcColour paint[BBC_SPRITE_SLOTS] = {
        time < 4.2f ? BBC_RED : BBC_BLUE, BBC_BLUE, BBC_BLUE, BBC_BLUE};
    bbc_sprite(r, &SPR_TITLE_SUV, COL(col), ROW(base_row - SPR_TITLE_SUV.h),
               1, paint);
}

/*
 * The street lamp, and the light it throws as the machine had light: a cone
 * of white dots that thickens toward the pavement and a pool laid on it in
 * rows. The man stands in it, which is the only reason he can be seen.
 */
static void draw_street_lamp(SDL_Renderer *r, const IntroScene *s, int col)
{
    int base = s->street + 23;
    int head = 159;

    for (int row = head + 5; row < base - 2; ++row)
    {
        float t = (float)(row - head - 5) / (float)(base - head - 7);
        int half = 1 + (int)(t * 15.0f);
        cells_stipple(r, BBC_CYAN, BBC_DITHER_DOTS, col + 1 - half, row,
                      half * 2 + 1, 1);
    }
    cells_stipple(r, BBC_CYAN, BBC_DITHER_ROWS, col - 16, base - 3, 34, 3);
    cells_stipple(r, BBC_WHITE, BBC_DITHER_SPARSE, col - 8, base - 3, 18, 2);

    cells(r, BBC_BLUE, col, head + 5, 1, base - head - 5);
    cells(r, BBC_BLUE, col - 1, base - 1, 3, 1);
    cells(r, BBC_BLUE, col - 1, head, 5, 2);
    cells(r, BBC_BLUE, col + 3, head + 2, 1, 1);
    cells(r, BBC_WHITE, col, head + 2, 4, 2);
}

/*
 * The street's layers — the ground, the wet reflections and the lobby's
 * spill — each its own pass, so what stands on it (the SUV, the lamp, the
 * man) goes down after all three.
 */
static void draw_street_ground(SDL_Renderer *r, const IntroScene *s)
{
    int y = s->street;
    int cols = (int)(s->w / (float)BBC_PX_W);
    int rows = (int)(s->h / (float)BBC_PX_H);

    /*
     * The ground is the darkest plane in the shot.  It has to be: everything
     * it shows — the pool at the entrance, the smears under the lit windows,
     * the lamp — is reflected light, and reflected light on a pale road is
     * invisible.
     */
    cells(r, BBC_BLACK, 0, y, cols, rows - y);
    cells(r, BBC_BLUE, 0, y, cols, 1);
    cells_dither(r, BBC_BLACK, BBC_BLUE, BBC_DITHER_CHECK, 0, y + 1, cols, 2);
    cells(r, BBC_BLUE, 0, y + 3, cols, 1);

    /* Kerb joints, placed clear of the figure so no seam cuts him off at the
     * knee. */
    for (int jx = 6; jx < cols; jx += 24)
        cells(r, BBC_BLACK, jx, y, 1, 3);
    cells_stipple(r, BBC_BLUE, BBC_DITHER_DOTS, 3, y + 15, 29, 4);
    cells_stipple(r, BBC_BLUE, BBC_DITHER_DOTS, 160, y + 7, 32, 4);
}

static void draw_wet_reflections(SDL_Renderer *r, const IntroScene *s,
                                 float time)
{
    int y = s->street;

    /* Wet asphalt: every lit pane smears straight down the frame, one row in
     * two, the way a BBC game drew water. Asking pane_light() again is what
     * keeps the reflection honest. */
    for (int i = 0; i < TOWER_FLOORS; ++i)
    {
        for (int pane = 0; pane < TOWER_PANES; ++pane)
        {
            bool stutter = false;
            bool target = i == TARGET_FLOOR && pane == TARGET_PANE;
            PaneLight light = pane_light(i, pane, time, &stutter);
            if (!target && light == PANE_DARK)
                continue;
            if (target && time < 1.35f)
                continue;
            BbcColour c = target ? BBC_YELLOW
                                 : (light == PANE_WARM ? BBC_RED : BBC_BLUE);

            Cells p = pane_cells(s, i, pane);
            int cx = p.col + p.w / 2;
            int sy = y + 5;
            for (int seg = 0; seg < 6 && sy < y + 34; ++seg)
            {
                unsigned h = hash_u((unsigned)(i * 71 + pane * 9 + seg * 3));
                int jitter = (int)(h % 3u) - 1;
                int width = p.w * (6 - seg) / 9 + (target ? 2 : 0);
                if (width < 1)
                    width = 1;
                cells(r, c, cx - width / 2 + jitter, sy, width, 1);
                sy += 2 + (int)((h >> 3) % 3u) * 2;
            }
        }
    }
}

static void draw_lobby_spill(SDL_Renderer *r, const IntroScene *s, float wake)
{
    if (wake <= 0.33f)
        return;
    int y = s->street;
    int half = TOWER_HALF[TOWER_FLOORS];
    int left = s->cx - half;
    int door = left + 6;

    /*
     * A fully glazed lobby spills along its whole width, not just out of the
     * doors.  That band of light is what the parked car and the kerb are read
     * against — without it both are dark shapes on a dark ground.
     */
    cells_stipple(r, BBC_RED, BBC_DITHER_SPARSE, left - 6, y + 1, half * 2 + 32,
                  4);
    if (wake > 0.66f)
    {
        cells_stipple(r, BBC_YELLOW, BBC_DITHER_SPARSE, door - 3, y + 1, 20, 8);
        cells_stipple(r, BBC_YELLOW, BBC_DITHER_DOTS, door - 6, y + 9, 26, 6);
    }
}

static void render_street(SDL_Renderer *r, const Intro *intro,
                          const IntroScene *s)
{
    float time = intro->time;
    float wake = smoothstep01((time - 0.35f) / 0.8f);
    int y = s->street;

    draw_street_ground(r, s);
    draw_wet_reflections(r, s, time);
    draw_lobby_spill(r, s, wake);

    /* Parked past the right edge of the start plate. */
    draw_suv(r, s->cx + TOWER_HALF[TOWER_FLOORS] + 7, y + 13, time);

    /* A bollard on the near side of the kerb. */
    cells(r, BBC_BLUE, 65, y + 13, 2, 8);
    cells(r, BBC_CYAN, 65, y + 13, 2, 1);

    draw_street_lamp(r, s, 28);

    /*
     * In profile, facing the tower: the Chuck every sector hands over, at
     * twice the size the floors draw him, standing in the lamp's pool because
     * a figure on the darkest plane of the shot is nothing without a light on
     * him. The cast's own sprite and nothing else — a fourth jacket on the
     * title screen for the same coat is how the first frame anybody sees came
     * to show a man the game never puts on a floor.
     */
    const int scale = 2;
    int feet = y + 26;
    bbc_sprite_scaled(r, &SPR_CHUCK_STAND, COL(42),
                      ROW(feet - SPR_CHUCK_STAND.h * scale), 1, NULL, scale);
}

/* ---- Wordmark ----------------------------------------------------------- */

/*
 * The title is a BBC logo, which is to say a bitmap.
 *
 * Five letters designed on squared paper, twelve columns by eighteen squares,
 * each square one BBC pixel across and two down — so on the glass a square is
 * square, the stems are as thick as the bars, and the K's legs are true
 * staircases rather than smoothed lines. They are painted in bands of the
 * machine's colours from the top down, white along every top edge for the
 * light, with a blue shadow dropped one pixel down and to the right for the
 * depth: the whole of what a 1985 loading screen had to make a word stand off
 * the black, and it is enough.
 */
#define MARK_COLS 12
#define MARK_SQUARES 18
#define MARK_ROWS (MARK_SQUARES * 2)
#define MARK_GAP_COLS 3

static const char *const MARK_C[MARK_SQUARES] = {
    "..##########", ".###########", "############", "####......##",
    "###.........", "###.........", "###.........", "###.........",
    "###.........", "###.........", "###.........", "###.........",
    "###.........", "###.........", "####......##", "############",
    ".###########", "..##########"};

static const char *const MARK_H_[MARK_SQUARES] = {
    "###......###", "###......###", "###......###", "###......###",
    "###......###", "###......###", "###......###", "############",
    "############", "############", "###......###", "###......###",
    "###......###", "###......###", "###......###", "###......###",
    "###......###", "###......###"};

static const char *const MARK_U[MARK_SQUARES] = {
    "###......###", "###......###", "###......###", "###......###",
    "###......###", "###......###", "###......###", "###......###",
    "###......###", "###......###", "###......###", "###......###",
    "###......###", "###......###", "####....####", "############",
    ".##########.", "..########.."};

static const char *const MARK_K[MARK_SQUARES] = {
    "###.....####", "###....####.", "###...####..", "###..####...",
    "###.####....", "#######.....", "######......", "#####.......",
    "####........", "#####.......", "######......", "#######.....",
    "###.####....", "###..####...", "###...####..", "###....####.",
    "###.....####", "###......###"};

static const char *const *mark_glyph(char letter)
{
    switch (letter)
    {
    case 'C':
        return MARK_C;
    case 'H':
        return MARK_H_;
    case 'U':
        return MARK_U;
    case 'K':
        return MARK_K;
    default:
        return NULL;
    }
}

static bool mark_ink(const char *const *glyph, int col, int square)
{
    if (glyph == NULL || col < 0 || col >= MARK_COLS || square < 0 ||
        square >= MARK_SQUARES)
        return false;
    return glyph[square][col] == '#';
}

/*
 * The bands, one per BBC row from the top of a letter down: the classic
 * horizon of a period logo, cool and pale above, hot below, split by a white
 * line across the middle of the word.
 */
static const BbcColour MARK_BAND[] = {
    BBC_WHITE,  BBC_CYAN,   BBC_CYAN,   BBC_CYAN,   BBC_CYAN,   BBC_CYAN,
    BBC_CYAN,   BBC_CYAN,   BBC_CYAN,   BBC_CYAN,   BBC_CYAN,   BBC_CYAN,
    BBC_CYAN,   BBC_CYAN,   BBC_BLUE,   BBC_CYAN,   BBC_BLUE,   BBC_BLUE,
    BBC_WHITE,  BBC_YELLOW, BBC_YELLOW, BBC_YELLOW, BBC_YELLOW, BBC_YELLOW,
    BBC_YELLOW, BBC_YELLOW, BBC_YELLOW, BBC_YELLOW, BBC_RED,    BBC_YELLOW,
    BBC_RED,    BBC_RED,    BBC_RED,    BBC_RED,    BBC_RED,    BBC_RED};
_Static_assert(sizeof(MARK_BAND) / sizeof(MARK_BAND[0]) == MARK_ROWS,
               "every row of the wordmark needs a band");

/*
 * One letter with its top-left at (`col`, `row`). `glint` is the column the
 * sweep of light is passing — a diagonal of white that crosses the word every
 * few seconds, the period's own way of saying a logo is polished metal.
 */
static void draw_mark_letter(SDL_Renderer *r, char letter, int col, int row,
                             int glint)
{
    const char *const *glyph = mark_glyph(letter);
    if (glyph == NULL)
        return;

    /* The shadow first, a square down and a column right. */
    for (int sq = 0; sq < MARK_SQUARES; ++sq)
        for (int c = 0; c < MARK_COLS; ++c)
            if (mark_ink(glyph, c, sq))
                cells(r, BBC_BLUE, col + c + 1, row + sq * 2 + 2, 1, 2);

    for (int sq = 0; sq < MARK_SQUARES; ++sq)
    {
        for (int c = 0; c < MARK_COLS; ++c)
        {
            if (!mark_ink(glyph, c, sq))
                continue;
            bool top_edge = !mark_ink(glyph, c, sq - 1);
            bool left_edge = !mark_ink(glyph, c - 1, sq);
            for (int half = 0; half < 2; ++half)
            {
                int band = sq * 2 + half;
                BbcColour colour = MARK_BAND[band];
                if (top_edge && half == 0)
                    colour = BBC_WHITE;
                else if (left_edge && band < MARK_ROWS / 2)
                    colour = BBC_WHITE;
                int d = (col + c) - glint + band / 4;
                if (d >= 0 && d < 3)
                    colour = BBC_WHITE;
                cells(r, colour, col + c, row + band, 1, 1);
            }
        }
    }
}

/* The wordmark centred on `cx` with its letters' tops at `y`: the title
 * screen's middle of the sky, or wherever the key art leaves room. */
static void render_logo_at(SDL_Renderer *r, const Intro *intro, float cx,
                           float y)
{
    static const char *word = "CHUCK";
    const int advance = MARK_COLS + MARK_GAP_COLS;
    const int word_cols = advance * 4 + MARK_COLS;
    const int col0 = col_of(cx) - word_cols / 2;
    const int row0 = (int)(y / (float)BBC_PX_H);

    float sweep = fmodf(intro->time, 9.0f);
    int glint = sweep < 1.8f
                    ? col0 - 12 + (int)((sweep / 1.8f) * (float)(word_cols + 24))
                    : -1000;

    for (int i = 0; i < 5; ++i)
    {
        /* Each letter drops the last few rows into place, one after another,
         * a whole BBC row at a time. */
        float reveal = smoothstep01((intro->time - 0.05f - (float)i * 0.08f) /
                                    0.36f);
        if (reveal <= 0.0f)
            continue;
        int drop = (int)((1.0f - reveal) * 4.0f);
        draw_mark_letter(r, word[i], col0 + i * advance, row0 - drop, glint);
    }

    /* Tagline, set between two red rules so it reads as part of the
     * wordmark. It arrives a beat after the letters and the rules grow out
     * from it a BBC pixel at a time. */
    float tag = smoothstep01((intro->time - 0.72f) / 0.5f);
    if (tag <= 0.0f)
        return;
    /* Forty, because that is the number the rest of the game says: the men on
     * the wall shout it down at Chuck and the manual counts the same floors.
     * A tagline that disagrees with the building it is standing in front of is
     * the one line on this screen nobody can check and everybody reads. */
    const char *line = "FORTY FLOORS. ONE WAY UP.";
    const float track = 3.0f;
    float width = tracked_width(line, 1.0f, track);
    float ty = y + (float)(MARK_ROWS * BBC_PX_H) + 28.0f;
    draw_tracked_centered(r, cx, ty, 1.0f, track, BBC_WHITE, line);

    int rule = (int)(10.0f * tag);
    int rule_row = (int)((ty + 3.0f) / (float)BBC_PX_H);
    cells(r, BBC_RED, col_of(cx - width * 0.5f - 16.0f) - rule, rule_row, rule,
          1);
    cells(r, BBC_RED, col_of(cx + width * 0.5f + 16.0f), rule_row, rule, 1);
}

static void render_logo(SDL_Renderer *r, const Intro *intro,
                        const IntroScene *s)
{
    render_logo_at(r, intro, s->w * 0.5f, 30.0f);
}

/* ---- Interface ---------------------------------------------------------- */

/* The arrow in front of START: three columns, pointing at the words. */
BBC_SPRITE(SPR_TITLE_CHEVRON,
    "1..",
    "11.",
    "111",
    "11.",
    "1..");

/*
 * The start plate: black, with the doubled border a BBC panel that matters
 * gets, and the words on it flashing between two colours at the machine's own
 * flash rate — which is what PRESS SPACE TO START did on every title page of
 * the period, and which holds still under the reduced-motion switch. The
 * pointer over it turns the plate to inverse video, the machine's highlight.
 */
static void render_start_prompt(SDL_Renderer *r, const Intro *intro,
                                const IntroScene *s, const PadHints *pad)
{
    const SDL_FRect *button = &intro->start_button;
    if (intro->time < 1.35f)
        return;

    bool hot = intro->start_hovered;
    float cx = s->w * 0.5f;

    bbc_fill(r, hot ? BBC_YELLOW : BBC_BLACK, button->x, button->y, button->w,
             button->h);
    bbc_frame(r, hot ? BBC_WHITE : BBC_RED, button->x, button->y, button->w,
              button->h);
    bbc_frame(r, hot ? BBC_RED : BBC_YELLOW, button->x + 4.0f, button->y + 2.0f,
              button->w - 8.0f, button->h - 4.0f);

    /* Chevron and label are one group, centred as a group — so whichever
     * label is showing, the padding is equal on both sides. */
    char pad_label[32];
    const char *label =
        pad_hint(pad, pad_label, sizeof(pad_label), "PRESS $A TO START",
                 START_LABEL);
    float width = tracked_width(label, 1.0f, START_TRACK);
    float group = START_CHEVRON_W + START_CHEVRON_GAP + width;
    float group_x = cx - group * 0.5f;
    float text_y = button->y + 13.0f;

    BbcColour chevron[BBC_SPRITE_SLOTS] = {
        hot ? BBC_RED : bbc_flash_pair(BBC_RED, BBC_YELLOW), BBC_BLACK,
        BBC_BLACK, BBC_BLACK};
    bbc_sprite(r, &SPR_TITLE_CHEVRON, group_x, text_y - 1.0f, 1, chevron);
    draw_tracked(r, group_x + START_CHEVRON_W + START_CHEVRON_GAP, text_y, 1.0f,
                 START_TRACK,
                 hot ? BBC_BLACK : bbc_flash_pair(BBC_WHITE, BBC_YELLOW),
                 label);
}

/*
 * One chip on the quiet line: a blue frame round the rectangle the mouse can
 * hit, the key on a bar of cyan in inverse video, and the label in white. Over
 * the pointer the frame and the label go yellow and the cap goes yellow with
 * it. It is set at scale 1 and no smaller, which on this font is the only
 * scale there is for a line of hints.
 */
static void render_prompt_chip(SDL_Renderer *r, const SDL_FRect *button,
                               bool hot, const char *key, const char *label)
{
    float x = button->x + MANUAL_HIT_PAD;
    float y = button->y;
    /* The same measurement the layout used, so the cap the mouse can hit and
     * the cap on screen are one rectangle. */
    float cap_w = chip_key_width(key);

    bbc_frame(r, hot ? BBC_YELLOW : BBC_BLUE, button->x, y, button->w,
              MANUAL_ROW_H);
    bbc_fill(r, hot ? BBC_YELLOW : BBC_CYAN, x, y + 2.0f, cap_w,
             MANUAL_ROW_H - 4.0f);
    /* Centred in the cap rather than set at a fixed inset: a PlayStation pad
     * spells two of its faces with two characters, and a keycap the label
     * hangs out of is worse than no keycap at all. */
    float key_w = (float)SDL_strlen(key) * (float)BBC_FONT_CELL;
    bbc_text(r, x + (cap_w - key_w) * 0.5f, y + 5.0f, 1.0f, BBC_BLACK, key);
    draw_tracked(r, x + cap_w + MANUAL_KEY_GAP, y + 5.0f, 1.0f, MANUAL_TRACK,
                 hot ? BBC_YELLOW : BBC_WHITE, label);
}

static void render_manual_prompt(SDL_Renderer *r, const Intro *intro,
                                 const PadHints *pad)
{
    if (intro->time < 1.75f)
        return;

    char manual_key[8];
    char options_key[8];
    char resume_key[12];
    char quit_key[8];
    /* The resume chip only exists once a sector has been earned. */
    if (intro->resume_offered)
        render_prompt_chip(r, &intro->resume_button, intro->resume_hovered,
                           pad_hint(pad, resume_key, sizeof(resume_key),
                                    "$SELECT", "R"),
                           intro->resume_label);
    render_prompt_chip(r, &intro->manual_button, intro->manual_hovered,
                       pad_hint(pad, manual_key, sizeof(manual_key), "$Y", "H"),
                       MANUAL_LABEL);
    render_prompt_chip(r, &intro->options_button, intro->options_hovered,
                       pad_hint(pad, options_key, sizeof(options_key), "$X", "J"),
                       OPTIONS_LABEL);
    /* And the last chip only exists where there is something to quit to. */
    if (intro->quit_offered)
        render_prompt_chip(r, &intro->quit_button,
                           intro->quit_hovered || intro->quit_hold > 0.0f,
                           pad_hint(pad, quit_key, sizeof(quit_key), "$B",
                                    "ESC"),
                           QUIT_LABEL);

    /*
     * The hold, drawn as the chip filling from its leading edge in the
     * palette's danger red — the colour ABANDON RUN is set in, because this is
     * the same kind of decision and the player should recognise it as one.
     * Nothing is drawn until the button is actually down, so until somebody
     * starts to mean it the line is just four things to know about.
     */
    if (intro->quit_hold > 0.0f)
    {
        float t = intro->quit_hold / TITLE_QUIT_HOLD_TIME;
        float bar_x = intro->quit_button.x + MANUAL_HIT_PAD;
        float bar_w = (intro->quit_button.w - MANUAL_HIT_PAD * 2.0f) * t;
        bbc_fill(r, BBC_RED, bar_x, intro->quit_button.y + MANUAL_ROW_H - 4.0f,
                 bar_w, 2.0f);
    }
}

/* ---- The fade ------------------------------------------------------------
 *
 * The shot comes up out of black the way a BBC picture did: the palette walked
 * three rungs back up its ladder, a step at a time, rather than a black veil
 * thinned over it. Every draw below asks the palette at the moment it lands,
 * so setting it once at the top dims the whole frame, and putting it back at
 * the bottom leaves nothing to whatever is drawn after the title.
 */
static int fade_steps(float time)
{
    float fade = 1.0f - smoothstep01(time / 0.65f);
    return (int)ceilf(fade * 3.0f - 0.01f);
}

typedef struct
{
    BbcColour entry[BBC_COLOUR_COUNT];
} SavedPalette;

static SavedPalette palette_save(void)
{
    SavedPalette saved;
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        saved.entry[i] = bbc_map((BbcColour)i);
    return saved;
}

static void palette_restore(const SavedPalette *saved)
{
    for (int i = 0; i < BBC_COLOUR_COUNT; ++i)
        bbc_palette_set((BbcColour)i, saved->entry[i]);
}

/* ---- The cover -----------------------------------------------------------
 *
 * `--screen cover` is the press kit's key art. It used to be this screen's
 * own night recomposed — the tower closer, the cordon on the street and Chuck
 * a dozen pixels tall on the wall — and at the 315x250 a store lists a game
 * at, it was a quiet building with a dot on it. The picture is the roof at
 * the end of the climb now, drawn by the film that owns every figure in it
 * (`key_art_render` in cutscene.c, which says why); what this file adds is
 * the wordmark, top left over the storm, where the action leaves the sky
 * empty. No interface: a START prompt on a store page is a button nobody can
 * press.
 */
static void render_cover(SDL_Renderer *r, const Intro *intro, int win_w,
                         int win_h)
{
    key_art_render(r, intro->time, win_w, win_h);
    render_logo_at(r, intro, 272.0f, 26.0f);
}

void intro_render(SDL_Renderer *r, const Intro *intro, int win_w, int win_h,
                  const PadHints *pad)
{
    SavedPalette saved = palette_save();
    bbc_palette_dim(fade_steps(intro->time));

    if (intro->key_art)
    {
        render_cover(r, intro, win_w, win_h);
        palette_restore(&saved);
        return;
    }

    IntroScene scene = scene_layout(win_w, win_h);

    render_sky(r, intro, &scene);
    render_skyline(r, intro, &scene);
    render_flanking_blocks(r, &scene);
    render_tower(r, intro, &scene);
    render_street(r, intro, &scene);
    render_logo(r, intro, &scene);
    render_start_prompt(r, intro, &scene, pad);
    render_manual_prompt(r, intro, pad);

    palette_restore(&saved);
}
