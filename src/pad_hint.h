#ifndef CHUCK_PAD_HINT_H
#define CHUCK_PAD_HINT_H

#include "common.h"

/*
 * What the pad in the player's hands actually has printed on it.
 *
 * SDL names a face button by its *position* — `SDL_GAMEPAD_BUTTON_SOUTH` is the
 * bottom one on every pad ever made — and binding straight to those positions
 * is what this used to do. It is right on an Xbox pad, quietly wrong on a
 * Nintendo one, and wrong in the one way a player cannot ignore: a Switch pad
 * prints A where an Xbox pad prints B, so the title screen asked for A and the
 * button printed A was the one that quit the game.
 *
 * So the game binds by *letter* instead. Confirm is whichever button the pad
 * calls A, back is the one it calls B, and the prompt and the thumb agree on
 * every pad — which is the only reason a letter is printed on a button at all.
 * A PlayStation pad needs no swap, only a spelling: its cross, circle, square
 * and triangle already sit where an Xbox pad's A, B, X and Y do. Spelling them
 * is not a free choice either — every prompt in the game goes through
 * `SDL_RenderDebugText`, whose 8x8 font renders ASCII and nothing else, so the
 * shapes are written the way a player types them rather than drawn.
 */
typedef enum
{
    PAD_FACE_NONE = -1,
    PAD_FACE_CONFIRM, /* A / cross:    start, confirm, skip, jump */
    PAD_FACE_CANCEL,  /* B / circle:   back out of whatever is open */
    PAD_FACE_ATTACK,  /* X / square:   attack; the assist sheet */
    PAD_FACE_DOOR,    /* Y / triangle: use door, hold to hack; the manual */
    PAD_FACE_COUNT
} PadFace;

typedef struct
{
    /* How this pad spells each letter, and which button it keeps it on. */
    const char *face[PAD_FACE_COUNT];
    SDL_GamepadButton at[PAD_FACE_COUNT];

    /* The buttons that sit in the same place on every pad but are not called
     * the same thing: + and - on a Switch pad, OPTIONS on a PlayStation. The
     * d-pad and the sticks are named the same everywhere and need no entry. */
    const char *start;
    const char *select;
    const char *shoulder_l;
    const char *shoulder_r;
} PadHints;

/* The Xbox letters in the Xbox places: what the manual's tables are written
 * in, what a pad SDL cannot name falls back to, and what is left behind when
 * a pad is unplugged. */
extern const PadHints PAD_HINTS_XBOX;

/* Reads the four letters off an open pad; a NULL pad gives the Xbox set. */
void pad_hints_read(PadHints *hints, SDL_Gamepad *gamepad);

/* The letter a physical button carries, or `PAD_FACE_NONE` when it carries
 * none — START, BACK, the shoulders and the d-pad mean the same thing wherever
 * they are printed, so they go straight through as themselves. */
PadFace pad_hints_face(const PadHints *hints, SDL_GamepadButton button);

/* And the way back: which button carries a letter. The two inputs that are
 * polled every frame rather than delivered as events need this. */
SDL_GamepadButton pad_hints_button(const PadHints *hints, PadFace face);

/*
 * One prompt line, spelled for whatever is in the player's hands.
 *
 * `pad_form` is a template written in the letters the tables already use —
 * `$A`, `$B`, `$X`, `$Y` for the faces and `$START`, `$SELECT`, `$LB`, `$RB`
 * for the rest — expanded into `buf`. With no pad in hand `key_form` comes
 * back untouched and `buf` is not written. It is one call rather than a
 * ternary at each prompt because the two halves of that ternary drifted apart
 * the moment either one was edited.
 */
const char *pad_hint(const PadHints *hints, char *buf, size_t size,
                     const char *pad_form, const char *key_form);

#endif /* CHUCK_PAD_HINT_H */
