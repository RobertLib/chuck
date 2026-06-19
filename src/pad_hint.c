#include "pad_hint.h"

#include "game_config.h"

#include <string.h>

const PadHints PAD_HINTS_XBOX = {
    .face = {"A", "B", "X", "Y"},
    .at = {PAD_BUTTON_SOUTH, PAD_BUTTON_EAST,
           PAD_BUTTON_WEST, PAD_BUTTON_NORTH},
    .start = "START",
    .select = "BACK",
    .shoulder_l = "LB",
    .shoulder_r = "RB",
};

/* Unsized, so the assertion measures the list rather than itself; see the note
 * on `LEVEL_THEME_NAMES` in level.c. A missing row here would zero-fill to
 * `PAD_BUTTON_SOUTH` and quietly ask one position twice while never asking
 * another — which reads as a pad that cannot letter itself and falls back to
 * the Xbox set, the one failure shaped exactly like success. */
const int PAD_FACE_POSITIONS[] = {PAD_BUTTON_SOUTH, PAD_BUTTON_EAST,
                                  PAD_BUTTON_WEST, PAD_BUTTON_NORTH};

_Static_assert(sizeof(PAD_FACE_POSITIONS) / sizeof(PAD_FACE_POSITIONS[0]) ==
                   (size_t)PAD_FACE_COUNT,
               "every face needs a button position");

/* A cross, a circle, a square and a triangle in the only alphabet the 8x8
 * debug font has. */
static const char *label_text(PadButtonLabel label)
{
    switch (label)
    {
    case PAD_LABEL_A:
        return "A";
    case PAD_LABEL_B:
        return "B";
    case PAD_LABEL_X:
        return "X";
    case PAD_LABEL_Y:
        return "Y";
    case PAD_LABEL_CROSS:
        return "X";
    case PAD_LABEL_CIRCLE:
        return "O";
    case PAD_LABEL_SQUARE:
        return "[]";
    case PAD_LABEL_TRIANGLE:
        return "/\\";
    case PAD_LABEL_UNKNOWN:
        break;
    }
    return NULL;
}

static PadFace label_face(PadButtonLabel label)
{
    switch (label)
    {
    case PAD_LABEL_A:
    case PAD_LABEL_CROSS:
        return PAD_FACE_CONFIRM;
    case PAD_LABEL_B:
    case PAD_LABEL_CIRCLE:
        return PAD_FACE_CANCEL;
    case PAD_LABEL_X:
    case PAD_LABEL_SQUARE:
        return PAD_FACE_ATTACK;
    case PAD_LABEL_Y:
    case PAD_LABEL_TRIANGLE:
        return PAD_FACE_DOOR;
    case PAD_LABEL_UNKNOWN:
        break;
    }
    return PAD_FACE_NONE;
}

/* START and SELECT keep their position on every pad and only ever change
 * name, so they are read off the type rather than off the button. */
static void read_named_buttons(PadHints *hints, PadType type)
{
    switch (type)
    {
    case PAD_TYPE_NINTENDO_SWITCH_PRO:
    case PAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case PAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case PAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        hints->start = "+";
        hints->select = "-";
        hints->shoulder_l = "L";
        hints->shoulder_r = "R";
        break;
    case PAD_TYPE_PS3:
        hints->start = "START";
        hints->select = "SELECT";
        hints->shoulder_l = "L1";
        hints->shoulder_r = "R1";
        break;
    case PAD_TYPE_PS4:
        hints->start = "OPTIONS";
        hints->select = "SHARE";
        hints->shoulder_l = "L1";
        hints->shoulder_r = "R1";
        break;
    case PAD_TYPE_PS5:
        hints->start = "OPTIONS";
        hints->select = "CREATE";
        hints->shoulder_l = "L1";
        hints->shoulder_r = "R1";
        break;
    case PAD_TYPE_UNKNOWN:
    default:
        /* Xbox, and every pad SDL has no better name for. `default` as well as
         * the named row, because this enum is deliberately a subset of SDL's:
         * an Xbox 360 pad arrives as a number with no row here at all, and it
         * belongs in exactly this branch. */
        break;
    }
}

void pad_hints_apply(PadHints *hints, PadType type,
                     const PadButtonLabel *labels, int count)
{
    *hints = PAD_HINTS_XBOX;

    /*
     * Filed by the letter printed on it rather than by where it sits, which is
     * the whole fix. It is committed only once all four letters have come back
     * and no two of them landed on the same button: a pad SDL cannot letter at
     * all is better served by the Xbox set than by half a translation.
     */
    if (labels != NULL && count >= PAD_FACE_COUNT)
    {
        PadHints read = *hints;
        int found = 0;
        for (int i = 0; i < PAD_FACE_COUNT; ++i)
        {
            PadFace face = label_face(labels[i]);
            if (face == PAD_FACE_NONE || (found & (1 << face)) != 0)
                continue;
            found |= 1 << face;
            read.at[face] = PAD_FACE_POSITIONS[i];
            read.face[face] = label_text(labels[i]);
        }
        if (found == (1 << PAD_FACE_COUNT) - 1)
            *hints = read;
    }

    read_named_buttons(hints, type);
}

PadFace pad_hints_face(const PadHints *hints, int button)
{
    for (int face = 0; face < PAD_FACE_COUNT; ++face)
    {
        if (hints->at[face] == button)
            return (PadFace)face;
    }
    return PAD_FACE_NONE;
}

int pad_hints_button(const PadHints *hints, PadFace face)
{
    if (face < 0 || face >= PAD_FACE_COUNT)
        return PAD_BUTTON_NONE;
    return hints->at[face];
}

/* The token after a `$`, or NULL when it is not one of ours — a stray `$` is
 * copied through rather than swallowed. */
static const char *token_value(const PadHints *hints, const char *src,
                               size_t *consumed)
{
    /* Longest names first, so no token can be eaten by a shorter prefix. */
    const struct
    {
        const char *name;
        const char *value;
    } tokens[] = {
        {"SELECT", hints->select},
        {"START", hints->start},
        {"LB", hints->shoulder_l},
        {"RB", hints->shoulder_r},
        {"A", hints->face[PAD_FACE_CONFIRM]},
        {"B", hints->face[PAD_FACE_CANCEL]},
        {"X", hints->face[PAD_FACE_ATTACK]},
        {"Y", hints->face[PAD_FACE_DOOR]},
    };

    for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i)
    {
        size_t len = strlen(tokens[i].name);
        if (strncmp(src, tokens[i].name, len) == 0)
        {
            *consumed = len;
            return tokens[i].value;
        }
    }
    *consumed = 0;
    return NULL;
}

const char *pad_hint(const PadHints *hints, char *buf, size_t size,
                     const char *pad_form, const char *key_form)
{
    if (buf == NULL || size == 0)
        return key_form;

    /* The keyboard line is copied into the same buffer the spelled one goes
     * into, so the answer is always *in* `buf` whatever is in the player's
     * hands. Handing `key_form` straight back and leaving `buf` untouched put
     * the burden on every caller to use the returned pointer rather than the
     * buffer it had just passed in, and the one caller that forgot drew a line
     * of uninitialised stack across the drive's control prompt — which is the
     * prompt that teaches the only part of the game nobody guesses. */
    if (hints == NULL)
    {
        size_t out = 0;
        while (key_form[out] != '\0' && out + 1 < size)
        {
            buf[out] = key_form[out];
            ++out;
        }
        buf[out] = '\0';
        return buf;
    }

    size_t out = 0;
    const char *src = pad_form;
    while (*src != '\0' && out + 1 < size)
    {
        if (*src == '$')
        {
            size_t consumed = 0;
            const char *value = token_value(hints, src + 1, &consumed);
            if (value != NULL)
            {
                src += 1 + consumed;
                while (*value != '\0' && out + 1 < size)
                    buf[out++] = *value++;
                continue;
            }
        }
        buf[out++] = *src++;
    }
    buf[out] = '\0';
    return buf;
}

int pad_stick_direction(int x, int y)
{
    int px = x < 0 ? -x : x;
    int py = y < 0 ? -y : y;
    if (px < GAMEPAD_AXIS_DEAD_ZONE && py < GAMEPAD_AXIS_DEAD_ZONE)
        return PAD_BUTTON_NONE;
    if (py >= px)
        return y < 0 ? PAD_BUTTON_DPAD_UP : PAD_BUTTON_DPAD_DOWN;
    return x < 0 ? PAD_BUTTON_DPAD_LEFT : PAD_BUTTON_DPAD_RIGHT;
}
