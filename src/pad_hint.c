#include "pad_hint.h"

const PadHints PAD_HINTS_XBOX = {
    .face = {"A", "B", "X", "Y"},
    .at = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
           SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH},
    .start = "START",
    .select = "BACK",
    .shoulder_l = "LB",
    .shoulder_r = "RB",
};

/* A cross, a circle, a square and a triangle in the only alphabet the 8x8
 * debug font has. */
static const char *label_text(SDL_GamepadButtonLabel label)
{
    switch (label)
    {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
        return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:
        return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:
        return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:
        return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        return "O";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        return "[]";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        return "/\\";
    default:
        return NULL;
    }
}

static PadFace label_face(SDL_GamepadButtonLabel label)
{
    switch (label)
    {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        return PAD_FACE_CONFIRM;
    case SDL_GAMEPAD_BUTTON_LABEL_B:
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        return PAD_FACE_CANCEL;
    case SDL_GAMEPAD_BUTTON_LABEL_X:
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        return PAD_FACE_ATTACK;
    case SDL_GAMEPAD_BUTTON_LABEL_Y:
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        return PAD_FACE_DOOR;
    default:
        return PAD_FACE_NONE;
    }
}

/* START and SELECT keep their position on every pad and only ever change
 * name, so they are read off the type rather than off the button. */
static void read_named_buttons(PadHints *hints, SDL_Gamepad *gamepad)
{
    switch (SDL_GetGamepadType(gamepad))
    {
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        hints->start = "+";
        hints->select = "-";
        hints->shoulder_l = "L";
        hints->shoulder_r = "R";
        break;
    case SDL_GAMEPAD_TYPE_PS3:
        hints->start = "START";
        hints->select = "SELECT";
        hints->shoulder_l = "L1";
        hints->shoulder_r = "R1";
        break;
    case SDL_GAMEPAD_TYPE_PS4:
        hints->start = "OPTIONS";
        hints->select = "SHARE";
        hints->shoulder_l = "L1";
        hints->shoulder_r = "R1";
        break;
    case SDL_GAMEPAD_TYPE_PS5:
        hints->start = "OPTIONS";
        hints->select = "CREATE";
        hints->shoulder_l = "L1";
        hints->shoulder_r = "R1";
        break;
    default:
        /* Xbox, and every pad SDL has no better name for. */
        break;
    }
}

void pad_hints_read(PadHints *hints, SDL_Gamepad *gamepad)
{
    static const SDL_GamepadButton positions[PAD_FACE_COUNT] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH};

    *hints = PAD_HINTS_XBOX;
    if (gamepad == NULL)
        return;

    /*
     * Filed by the letter printed on it rather than by where it sits, which is
     * the whole fix. It is committed only once all four letters have come back
     * and no two of them landed on the same button: a pad SDL cannot letter at
     * all is better served by the Xbox set than by half a translation.
     */
    PadHints read = *hints;
    int found = 0;
    for (int i = 0; i < PAD_FACE_COUNT; ++i)
    {
        SDL_GamepadButtonLabel label =
            SDL_GetGamepadButtonLabel(gamepad, positions[i]);
        PadFace face = label_face(label);
        if (face == PAD_FACE_NONE || (found & (1 << face)) != 0)
            continue;
        found |= 1 << face;
        read.at[face] = positions[i];
        read.face[face] = label_text(label);
    }
    if (found == (1 << PAD_FACE_COUNT) - 1)
        *hints = read;

    read_named_buttons(hints, gamepad);
}

PadFace pad_hints_face(const PadHints *hints, SDL_GamepadButton button)
{
    for (int face = 0; face < PAD_FACE_COUNT; ++face)
    {
        if (hints->at[face] == button)
            return (PadFace)face;
    }
    return PAD_FACE_NONE;
}

SDL_GamepadButton pad_hints_button(const PadHints *hints, PadFace face)
{
    if (face < 0 || face >= PAD_FACE_COUNT)
        return SDL_GAMEPAD_BUTTON_INVALID;
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

    for (size_t i = 0; i < SDL_arraysize(tokens); ++i)
    {
        size_t len = SDL_strlen(tokens[i].name);
        if (SDL_strncmp(src, tokens[i].name, len) == 0)
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
        SDL_strlcpy(buf, key_form, size);
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
