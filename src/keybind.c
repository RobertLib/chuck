#include "keybind.h"

#include <string.h>

/* ---- The actions ------------------------------------------------------ */

static const struct
{
    const char *label;
    const char *file_key;
} ACTIONS[BIND_COUNT] = {
#define CHUCK_BIND_ROW(ident, label, key) {label, key},
    CHUCK_BIND_LIST(CHUCK_BIND_ROW)
#undef CHUCK_BIND_ROW
};

const char *keybind_action_label(BindAction action)
{
    if (action < 0 || action >= BIND_COUNT)
        return "";
    return ACTIONS[action].label;
}

const char *keybind_action_file_key(BindAction action)
{
    if (action < 0 || action >= BIND_COUNT)
        return "";
    return ACTIONS[action].file_key;
}

/*
 * And the same row's pad line, spelled out of the same literal.
 *
 * `"pad_" key` rather than a fourth column on `CHUCK_BIND_LIST`, so the two
 * keys for one action cannot be renamed apart: a file key that drifts is a
 * binding silently reset on the next launch, which is the exact failure the
 * note on the list warns about, and having it happen to the pad half only
 * would be the sort of bug nobody without a controller could reproduce.
 */
static const char *const PAD_FILE_KEYS[BIND_COUNT] = {
#define CHUCK_BIND_PAD_KEY(ident, label, key) "pad_" key,
    CHUCK_BIND_LIST(CHUCK_BIND_PAD_KEY)
#undef CHUCK_BIND_PAD_KEY
};

const char *keybind_action_pad_file_key(BindAction action)
{
    if (action < 0 || action >= BIND_COUNT)
        return "";
    return PAD_FILE_KEYS[action];
}

/* ---- The keys --------------------------------------------------------- */

static const struct
{
    int scancode;
    const char *name;
} KEYS[] = {
#define CHUCK_KEY_ROW(ident, code, name) {code, name},
    CHUCK_KEY_LIST(CHUCK_KEY_ROW)
#undef CHUCK_KEY_ROW
};

#define KEY_COUNT ((int)(sizeof(KEYS) / sizeof(KEYS[0])))

const char *keybind_key_name(int scancode)
{
    for (int i = 0; i < KEY_COUNT; ++i)
    {
        if (KEYS[i].scancode == scancode)
            return KEYS[i].name;
    }
    return "";
}

int keybind_key_from_name(const char *name, size_t length)
{
    if (name == NULL || length == 0)
        return KEYBIND_NONE;
    for (int i = 0; i < KEY_COUNT; ++i)
    {
        if (strlen(KEYS[i].name) == length &&
            strncmp(KEYS[i].name, name, length) == 0)
        {
            return KEYS[i].scancode;
        }
    }
    return KEYBIND_NONE;
}

bool keybind_is_bindable(int scancode)
{
    return scancode != KEYBIND_NONE && keybind_key_name(scancode)[0] != '\0';
}

/* ---- The pad ---------------------------------------------------------- */

static const struct
{
    int button;
    const char *file_name;
    const char *shown;
} PADS[] = {
#define CHUCK_PAD_ROW(ident, button, file_name, shown)                       \
    {button, file_name, shown},
    CHUCK_PAD_LIST(CHUCK_PAD_ROW)
#undef CHUCK_PAD_ROW
};

#define PAD_COUNT ((int)(sizeof(PADS) / sizeof(PADS[0])))

const char *keybind_pad_name(int button)
{
    for (int i = 0; i < PAD_COUNT; ++i)
    {
        if (PADS[i].button == button)
            return PADS[i].shown;
    }
    return "";
}

const char *keybind_pad_file_name(int button)
{
    for (int i = 0; i < PAD_COUNT; ++i)
    {
        if (PADS[i].button == button)
            return PADS[i].file_name;
    }
    return "";
}

int keybind_pad_from_file_name(const char *name, size_t length)
{
    if (name == NULL || length == 0)
        return PADBIND_NONE;
    for (int i = 0; i < PAD_COUNT; ++i)
    {
        if (strlen(PADS[i].file_name) == length &&
            strncmp(PADS[i].file_name, name, length) == 0)
        {
            return PADS[i].button;
        }
    }
    return PADBIND_NONE;
}

bool keybind_pad_is_bindable(int button)
{
    return button != PADBIND_NONE && keybind_pad_file_name(button)[0] != '\0';
}

/* The canonical positions of the four letters, in `PadFace` order: A, B, X, Y
 * where an Xbox pad puts them. See the note in the header. */
static const int FACE_BUTTONS[4] = {0, 1, 2, 3};

int keybind_pad_face_index(int button)
{
    for (int i = 0; i < 4; ++i)
        if (FACE_BUTTONS[i] == button)
            return i;
    return -1;
}

int keybind_pad_face_button(int face_index)
{
    if (face_index < 0 || face_index >= 4)
        return PADBIND_NONE;
    return FACE_BUTTONS[face_index];
}

bool keybind_action_has_pad(const KeyBindings *bindings, BindAction action,
                            int button)
{
    if (bindings == NULL || action < 0 || action >= BIND_COUNT ||
        button == PADBIND_NONE)
    {
        return false;
    }
    for (int slot = 0; slot < BIND_SLOTS; ++slot)
        if (bindings->pad[action][slot] == button)
            return true;
    return false;
}

bool keybind_set_pad(KeyBindings *bindings, BindAction action, int slot,
                     int button)
{
    if (bindings == NULL || action < 0 || action >= BIND_COUNT)
        return false;
    if (slot < 0 || slot >= BIND_SLOTS)
        return false;
    if (!keybind_pad_is_bindable(button))
        return false;

    /* Taken from whoever had it, exactly as `keybind_set` does: a button that
     * fires two actions is indistinguishable from a broken pad. */
    for (int other = 0; other < BIND_COUNT; ++other)
        for (int s = 0; s < BIND_SLOTS; ++s)
            if (bindings->pad[other][s] == button)
                bindings->pad[other][s] = PADBIND_NONE;

    bindings->pad[action][slot] = button;
    return true;
}

/* ---- The table -------------------------------------------------------- */

/*
 * What the game has always been played with, and it is written here rather
 * than left in the input layer because "the defaults" is now a thing the reset
 * row has to be able to hand back.
 *
 * The arrows and WASD are both first class, which is why there are two slots
 * rather than a primary and a fallback. The four with one key are the four
 * that only ever had one.
 */
void keybind_defaults(KeyBindings *bindings)
{
    if (bindings == NULL)
        return;
    for (int action = 0; action < BIND_COUNT; ++action)
        for (int slot = 0; slot < BIND_SLOTS; ++slot)
            bindings->keys[action][slot] = KEYBIND_NONE;

    bindings->keys[BIND_LEFT][0] = 80;  /* LEFT */
    bindings->keys[BIND_LEFT][1] = 4;   /* A */
    bindings->keys[BIND_RIGHT][0] = 79; /* RIGHT */
    bindings->keys[BIND_RIGHT][1] = 7;  /* D */
    bindings->keys[BIND_UP][0] = 82;    /* UP */
    bindings->keys[BIND_UP][1] = 26;    /* W */
    bindings->keys[BIND_DOWN][0] = 81;  /* DOWN */
    bindings->keys[BIND_DOWN][1] = 22;  /* S */
    /* The dedicated jump key, and the reason it exists: over a ladder UP is
     * the climb, so the jump has to be reachable without it. */
    bindings->keys[BIND_JUMP][0] = 225; /* LSHIFT */
    bindings->keys[BIND_SHOOT][0] = 44; /* SPACE */
    bindings->keys[BIND_USE][0] = 8;    /* E */
    bindings->keys[BIND_WEAPON_NEXT][0] = 20; /* Q */
    bindings->keys[BIND_WEAPON_NEXT][1] = 43; /* TAB */
    bindings->keys[BIND_WEAPON_PREV][0] = 29; /* Z */

    /*
     * And the pad, which is the layout the game shipped with rather than a new
     * one: this table was welded into `game_read_input` and
     * `handle_gamepad_button` until it became a thing the player could change,
     * so what it says here is exactly what those two used to do.
     *
     * The four faces are stored as the canonical letter positions — A, B, X, Y
     * — and resolved against whatever pad is plugged in when they are read.
     * See `keybind_pad_face_index`.
     *
     * The left stick is not on this table and cannot be: it is an axis, it is
     * read every frame as movement beside the d-pad, and there is nothing
     * about "push the stick left" that a player could usefully move somewhere
     * else. The triggers stay off it for the same reason — they are the
     * drive's pedals, and the drive is not a sector.
     */
    for (int action = 0; action < BIND_COUNT; ++action)
        for (int slot = 0; slot < BIND_SLOTS; ++slot)
            bindings->pad[action][slot] = PADBIND_NONE;

    bindings->pad[BIND_LEFT][0] = 13;  /* DPAD LEFT  */
    bindings->pad[BIND_RIGHT][0] = 14; /* DPAD RIGHT */
    bindings->pad[BIND_UP][0] = 11;    /* DPAD UP    */
    bindings->pad[BIND_DOWN][0] = 12;  /* DPAD DOWN  */
    bindings->pad[BIND_JUMP][0] = 0;   /* A          */
    /* Both of the two a trigger finger finds, which is what the sector has
     * always answered — see the note on `KeyBindings`. */
    bindings->pad[BIND_SHOOT][0] = 2;  /* X          */
    bindings->pad[BIND_SHOOT][1] = 1;  /* B          */
    bindings->pad[BIND_USE][0] = 3;    /* Y          */
    bindings->pad[BIND_WEAPON_NEXT][0] = 10; /* RB   */
    bindings->pad[BIND_WEAPON_PREV][0] = 9;  /* LB   */
}

bool keybind_action_has(const KeyBindings *bindings, BindAction action,
                        int scancode)
{
    if (bindings == NULL || action < 0 || action >= BIND_COUNT ||
        scancode == KEYBIND_NONE)
    {
        return false;
    }
    for (int slot = 0; slot < BIND_SLOTS; ++slot)
    {
        if (bindings->keys[action][slot] == scancode)
            return true;
    }
    return false;
}

bool keybind_set(KeyBindings *bindings, BindAction action, int slot,
                 int scancode)
{
    if (bindings == NULL || action < 0 || action >= BIND_COUNT)
        return false;
    if (slot < 0 || slot >= BIND_SLOTS)
        return false;
    if (!keybind_is_bindable(scancode))
        return false;

    /* Taken from whoever had it, this action included: binding a key to the
     * slot beside the one already holding it would otherwise leave the action
     * answering the same key twice and one slot short. */
    for (int other = 0; other < BIND_COUNT; ++other)
        for (int s = 0; s < BIND_SLOTS; ++s)
            if (bindings->keys[other][s] == scancode)
                bindings->keys[other][s] = KEYBIND_NONE;

    bindings->keys[action][slot] = scancode;
    return true;
}
