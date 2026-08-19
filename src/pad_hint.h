#ifndef CHUCK_PAD_HINT_H
#define CHUCK_PAD_HINT_H

#include "keybind.h"

#include <stdbool.h>
#include <stddef.h>

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
 *
 * ## Why this file links no SDL
 *
 * It used to, and that made it the last corner of the tree no test could
 * reach: `make coverage` found four functions here that the suite never
 * executed, and they were the four that decide which letter goes on which
 * button — the whole of the fix described above. The reason was not that the
 * decision needs a gamepad. It needs *what a gamepad said*, which is nine
 * label numbers and a type number, and those are as inert as the scancodes in
 * [keybind.h](keybind.h).
 *
 * So this file gets the treatment that one already had. The numbers below are
 * a copy of somebody else's constants and are therefore checked rather than
 * trusted: [game_input.c](game_input.c) asserts every one of them against
 * `SDL_GAMEPAD_BUTTON_LABEL_*` and `SDL_GAMEPAD_TYPE_*` at compile time, one
 * assertion per row generated from the same list the tables are, so a value
 * that ever moves is a build failure and not a pad that quietly spells itself
 * wrong. Opening the pad and asking it those two questions is the only part
 * that needs SDL, and it stays on that side of the line in `game_input.c`.
 */

/*
 * The button positions, taken from the one list in the tree that already holds
 * them.
 *
 * Written as `CHUCK_PAD_LIST`'s own numbers rather than as four more literals,
 * because a second copy of SOUTH-is-0 would be a second copy of a number this
 * codebase has a rule about — and that list is already asserted against SDL
 * button for button. One copy, checked once, named twice.
 */
typedef enum
{
#define CHUCK_PAD_BUTTON_ENUM(ident, button, file_name, shown)                 \
    PAD_BUTTON_##ident = (button),
    CHUCK_PAD_LIST(CHUCK_PAD_BUTTON_ENUM)
#undef CHUCK_PAD_BUTTON_ENUM
    /* The absent button, spelled as the constant `keybind.h` already holds to
     * `SDL_GAMEPAD_BUTTON_INVALID`, so there is one of it rather than two. */
    PAD_BUTTON_NONE = PADBIND_NONE
} PadButton;

/*
 * The letter a pad says is printed on a button.
 *
 * `ROW(ident, value)`. These are `SDL_GamepadButtonLabel`, and the point of
 * spelling them out here is that the switch below them is a *decision* — which
 * letter means confirm — that the suite has to be able to drive without a
 * gamepad plugged in.
 */
#define CHUCK_PAD_LABEL_LIST(ROW)                                             \
    ROW(UNKNOWN, 0)                                                           \
    ROW(A, 1)                                                                 \
    ROW(B, 2)                                                                 \
    ROW(X, 3)                                                                 \
    ROW(Y, 4)                                                                 \
    ROW(CROSS, 5)                                                             \
    ROW(CIRCLE, 6)                                                            \
    ROW(SQUARE, 7)                                                            \
    ROW(TRIANGLE, 8)

typedef enum
{
#define CHUCK_PAD_LABEL_ENUM(ident, value) PAD_LABEL_##ident = (value),
    CHUCK_PAD_LABEL_LIST(CHUCK_PAD_LABEL_ENUM)
#undef CHUCK_PAD_LABEL_ENUM
} PadButtonLabel;

/*
 * The pads whose START and SELECT are called something other than START and
 * SELECT, and nothing else.
 *
 * `ROW(ident, value)`. Only the types this file actually branches on are
 * listed: an Xbox pad, a generic pad and everything SDL has no better name for
 * all take the same fall-through, so adding them would be adding rows nothing
 * reads. `PAD_TYPE_UNKNOWN` is the fall-through's own name and is what a
 * caller with no pad passes.
 */
#define CHUCK_PAD_TYPE_LIST(ROW)                                              \
    ROW(UNKNOWN, 0)                                                           \
    ROW(PS3, 4)                                                               \
    ROW(PS4, 5)                                                               \
    ROW(PS5, 6)                                                               \
    ROW(NINTENDO_SWITCH_PRO, 7)                                               \
    ROW(NINTENDO_SWITCH_JOYCON_LEFT, 8)                                       \
    ROW(NINTENDO_SWITCH_JOYCON_RIGHT, 9)                                      \
    ROW(NINTENDO_SWITCH_JOYCON_PAIR, 10)

typedef enum
{
#define CHUCK_PAD_TYPE_ENUM(ident, value) PAD_TYPE_##ident = (value),
    CHUCK_PAD_TYPE_LIST(CHUCK_PAD_TYPE_ENUM)
#undef CHUCK_PAD_TYPE_ENUM
} PadType;

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
    int at[PAD_FACE_COUNT];

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

/*
 * The four positions a face letter can be printed on, in `PadFace` order.
 *
 * Exported because asking a pad what it prints is the caller's job and this is
 * the list of questions. Unsized, so the assertion beside it measures the list
 * rather than itself — see the note on `LEVEL_THEME_NAMES` in level.c.
 */
extern const int PAD_FACE_POSITIONS[];

/*
 * Decide the whole spelling from what a pad said, with no pad in the room.
 *
 * `labels[i]` is the letter the pad prints on `PAD_FACE_POSITIONS[i]`, so the
 * array is indexed by *position* and the filing this function does is what
 * turns that into an index by *letter*. `count` is how many of them the caller
 * managed to read; anything short of `PAD_FACE_COUNT` leaves the Xbox set
 * alone, for the same reason a half-lettered pad does.
 *
 * Passing `PAD_TYPE_UNKNOWN` and a `count` of nought is the no-pad case and
 * yields exactly `PAD_HINTS_XBOX`.
 */
void pad_hints_apply(PadHints *hints, PadType type,
                     const PadButtonLabel *labels, int count);

/* The letter a physical button carries, or `PAD_FACE_NONE` when it carries
 * none — START, BACK, the shoulders and the d-pad mean the same thing wherever
 * they are printed, so they go straight through as themselves. */
PadFace pad_hints_face(const PadHints *hints, int button);

/* And the way back: which button carries a letter. The two inputs that are
 * polled every frame rather than delivered as events need this. */
int pad_hints_button(const PadHints *hints, PadFace face);

/*
 * One prompt line, spelled for whatever is in the player's hands.
 *
 * `pad_form` is a template written in the letters the tables already use —
 * `$A`, `$B`, `$X`, `$Y` for the faces and `$START`, `$SELECT`, `$LB`, `$RB`
 * for the rest — expanded into `buf`. With no pad in hand `key_form` is copied
 * into `buf` instead, so **the answer is always in `buf`** and a caller that
 * ignores the return value still draws the right line rather than the stack.
 * It is one call rather than a ternary at each prompt because the two halves
 * of that ternary drifted apart the moment either one was edited.
 */
const char *pad_hint(const PadHints *hints, char *buf, size_t size,
                     const char *pad_form, const char *key_form);

/*
 * Which way a stick is pushed, said as the d-pad button that means the same
 * thing, or `PAD_BUTTON_NONE` while it is inside `GAMEPAD_AXIS_DEAD_ZONE`.
 *
 * Here rather than in [game_input.c](game_input.c) for the reason the letters
 * are: what the menus need off a stick is a *decision* — how far is a push, and
 * which way is a diagonal — and a decision made behind `SDL_GetGamepadAxis` is
 * a decision no test can drive. Reading the two axes is what needs a pad and
 * stays over there; this takes the two numbers it read.
 *
 * The dominant axis wins and a tie goes to the vertical, because every cursor in
 * the game runs down a column. Both axes are asked together rather than one at a
 * time because a diagonal arrives as two separate events and which of the two a
 * menu should answer is only decidable with both in hand.
 */
int pad_stick_direction(int x, int y);

#endif /* CHUCK_PAD_HINT_H */
