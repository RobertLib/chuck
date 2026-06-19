/*
 * The credits, and the one name on them.
 *
 * A roll for a game one person wrote is a short list said six times, and that
 * repetition is the credit rather than padding: the six jobs are six genuinely
 * different disciplines, and the answer being the same every time is the whole
 * of what this screen has to say. The building's own docket had twelve names
 * on it and the manual prints all twelve; this one has one, and the roll says
 * so out loud rather than leaving the player to notice.
 *
 * Nothing here draws anything. The table is the roll, the heights are the
 * layout, and `game_render.c` walks both — the same split the options sheet
 * uses, and the same reason: a line that does not fit inside the frame is
 * caught by `make test` instead of by whoever happens to sit through the
 * ending.
 */

#include "credits.h"

#include <stddef.h>

#include "version.h"

static const CreditLine LINES[] = {
    {CREDIT_GAP, NULL},
    {CREDIT_TITLE, "CHUCK"},
    {CREDIT_NOTE, "FORTY FLOORS. SEVENTEEN SECTORS. ONE NIGHT."},
    {CREDIT_GAP, NULL},
    {CREDIT_RULE, NULL},
    {CREDIT_GAP, NULL},

    {CREDIT_ROLE, "DESIGN"},
    {CREDIT_NAME, "ROBLIB"},
    {CREDIT_GAP, NULL},
    {CREDIT_ROLE, "PROGRAMMING"},
    {CREDIT_NAME, "ROBLIB"},
    {CREDIT_GAP, NULL},
    {CREDIT_ROLE, "ART AND ANIMATION"},
    {CREDIT_NAME, "ROBLIB"},
    {CREDIT_GAP, NULL},
    {CREDIT_ROLE, "LEVEL DESIGN"},
    {CREDIT_NAME, "ROBLIB"},
    {CREDIT_GAP, NULL},
    {CREDIT_ROLE, "MUSIC AND SOUND"},
    {CREDIT_NAME, "ROBLIB"},
    {CREDIT_GAP, NULL},
    {CREDIT_ROLE, "STORY AND SCRIPT"},
    {CREDIT_NAME, "ROBLIB"},
    {CREDIT_GAP, NULL},

    {CREDIT_RULE, NULL},
    {CREDIT_GAP, NULL},
    {CREDIT_ROLE, "A GAME BY"},
    {CREDIT_NAME, "ROBLIB"},
    {CREDIT_GAP, NULL},
    {CREDIT_NOTE, "TWELVE NAMES ON THEIR DOCKET."},
    {CREDIT_NOTE, "ONE ON THIS ONE."},
    {CREDIT_GAP, NULL},

    {CREDIT_RULE, NULL},
    {CREDIT_GAP, NULL},
    {CREDIT_ROLE, "BUILT WITH"},
    {CREDIT_NAME, "SDL3"},
    {CREDIT_GAP, NULL},
    {CREDIT_NOTE, "NO ART FILES AND NO SOUND FILES."},
    {CREDIT_NOTE, "EVERY PIXEL IS DRAWN AND EVERY SOUND"},
    {CREDIT_NOTE, "SYNTHESISED WHEN THE GAME STARTS."},
    {CREDIT_GAP, NULL},

    {CREDIT_RULE, NULL},
    {CREDIT_GAP, NULL},
    {CREDIT_NOTE, "WITH THANKS TO EVERY FILM ABOUT ONE MAN,"},
    {CREDIT_NOTE, "ONE BUILDING AND ONE VERY LONG NIGHT."},
    {CREDIT_GAP, NULL},
    {CREDIT_NOTE, "AND TO EVERYONE WHO CAME UP FORTY FLOORS."},
    {CREDIT_GAP, NULL},
    {CREDIT_GAP, NULL},

    /* The card the roll comes to rest on. */
    {CREDIT_TITLE, "CHUCK"},
    {CREDIT_NOTE, "VERSION " CHUCK_VERSION},
    {CREDIT_NOTE, "(C) 2026 ROBERT LIBSANSKY"},
};

#define LINE_COUNT ((int)(sizeof(LINES) / sizeof(LINES[0])))

const CreditLine *credits_lines(int *out_count)
{
    if (out_count != NULL)
        *out_count = LINE_COUNT;
    return LINES;
}

float credits_line_height(CreditLineKind kind)
{
    switch (kind)
    {
    case CREDIT_TITLE:
        return 42.0f;
    case CREDIT_ROLE:
        return 18.0f;
    case CREDIT_NAME:
        return 34.0f;
    case CREDIT_NOTE:
        return 18.0f;
    case CREDIT_RULE:
        return 26.0f;
    case CREDIT_GAP:
        return 22.0f;
    }
    return 0.0f;
}

float credits_line_scale(CreditLineKind kind)
{
    /* One or two, and nothing in between: the 8x8 font is a bitmap, and any
     * other scale resamples it into the one thing a screen of nothing but
     * type cannot afford. */
    switch (kind)
    {
    case CREDIT_TITLE:
    case CREDIT_NAME:
        return 2.0f;
    case CREDIT_ROLE:
    case CREDIT_NOTE:
        return 1.0f;
    case CREDIT_RULE:
    case CREDIT_GAP:
        return 0.0f;
    }
    return 0.0f;
}

/* Where the last line's own row begins, measured from the top of the roll. */
static float last_line_offset(void)
{
    float offset = 0.0f;
    for (int i = 0; i < LINE_COUNT - 1; ++i)
        offset += credits_line_height(LINES[i].kind);
    return offset;
}

void credits_init(CreditsRoll *roll, float view_h)
{
    roll->time = 0.0f;
    /* The roll starts one frame below the bottom edge and climbs until the
     * last line is standing on its mark, so the closing card is composed by
     * the same arithmetic that scrolls everything above it. */
    roll->travel = view_h + last_line_offset() - CREDITS_REST_Y;
    if (roll->travel < 0.0f)
        roll->travel = 0.0f;
    roll->duration = roll->travel / CREDITS_SCROLL_SPEED + CREDITS_HOLD_TIME;
}

bool credits_update(CreditsRoll *roll, float dt)
{
    roll->time += dt;
    if (roll->time >= roll->duration)
    {
        roll->time = roll->duration;
        return true;
    }
    return false;
}

void credits_skip_to_rest(CreditsRoll *roll)
{
    float rest_at = roll->duration - CREDITS_HOLD_TIME;
    if (roll->time < rest_at)
        roll->time = rest_at;
}

bool credits_at_rest(const CreditsRoll *roll)
{
    return roll->time >= roll->duration - CREDITS_HOLD_TIME;
}

float credits_scroll(const CreditsRoll *roll)
{
    float scroll = roll->time * CREDITS_SCROLL_SPEED;
    return scroll < roll->travel ? scroll : roll->travel;
}
