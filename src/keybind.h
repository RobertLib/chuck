#ifndef CHUCK_KEYBIND_H
#define CHUCK_KEYBIND_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Which key does what, as a model with no SDL in it.
 *
 * The keyboard used to be written into `game_read_input` and the event handler
 * as scancodes, which made the control scheme a property of the source rather
 * than of the player. That was the last thing the options sheet could not
 * reach: a sheet with three assist switches on it, in a game whose every other
 * comfort is a row, still answered "can I move the jump key" with no.
 *
 * This file is the model and [keybind.c](keybind.c) is the table. It links no
 * SDL for the same reason [settings.c](settings.c) does not — it is state the
 * player owns and a file it round-trips through, so the suite can hold both —
 * and the numbers below *are* SDL scancodes, which is a copy of somebody
 * else's constants and therefore has to be checked rather than trusted.
 * `game_input.c` does that check at compile time, one `_Static_assert` per row
 * generated from this same list, so a scancode that ever moves is a build
 * failure and not a key that quietly stops working.
 *
 * ## What is bindable and what is not
 *
 * The nine below are the controls of a sector — the things a hand is on while
 * playing. ESC, ENTER and BACKSPACE are deliberately absent from the key table
 * further down, and that is the rule rather than an oversight: they are pause,
 * confirm and back, which is the whole of how a player who has just bound
 * something unusable gets back to this sheet to undo it. A settings screen that
 * can lock you out of the settings screen is worse than no settings screen.
 */

/*
 * The actions, their labels on the sheet, and the words they are filed under.
 *
 * The list parameter is `ROW` rather than the usual `X`, and that is not a
 * style choice: the key table below has an entry for the key *called* X, whose
 * identifier would shadow a parameter named X and expand into the macro
 * itself. It is a compile error rather than a wrong answer, but only once
 * somebody has written it.
 *
 * `ROW(ident, label, key)` — `label` is what the options sheet prints and
 * `key` is what the settings file writes. Neither may be derived from the
 * other: the label is allowed to be retitled and the file key is not, because
 * a renamed file key is a binding silently reset on the next launch.
 */
#define CHUCK_BIND_LIST(ROW)                                                \
    ROW(LEFT, "MOVE LEFT", "bind_left")                                     \
    ROW(RIGHT, "MOVE RIGHT", "bind_right")                                  \
    ROW(UP, "UP / CLIMB", "bind_up")                                        \
    ROW(DOWN, "DOWN / CRAWL", "bind_down")                                  \
    ROW(JUMP, "JUMP", "bind_jump")                                          \
    ROW(SHOOT, "ATTACK", "bind_attack")                                     \
    ROW(USE, "USE DOOR / HACK", "bind_use")                                 \
    ROW(WEAPON_NEXT, "NEXT WEAPON", "bind_weapon_next")                     \
    ROW(WEAPON_PREV, "LAST WEAPON", "bind_weapon_prev")

typedef enum
{
#define CHUCK_BIND_ENUM(ident, label, key) BIND_##ident,
    CHUCK_BIND_LIST(CHUCK_BIND_ENUM)
#undef CHUCK_BIND_ENUM
        BIND_COUNT
} BindAction;

/*
 * Two keys an action may answer to, which is what keeps the arrows and WASD
 * both working without either being second class. A slot holding
 * `KEYBIND_NONE` is empty and answers nothing.
 */
#define BIND_SLOTS 2
#define KEYBIND_NONE 0

/*
 * The keys a player may bind, and the exact names printed for them.
 *
 * `ROW(ident, scancode, name)`. Every `name` is at most `KEYBIND_NAME_MAX`
 * characters, which is not a coincidence and is checked by the suite: the
 * options sheet draws two of them side by side on one row, so a key that spells
 * itself longer than the column would push the row off the plate — the same
 * failure the manual's control sheet has already had once.
 *
 * The set is deliberately not "every key SDL can name". It is the ones a hand
 * plays with, plus the numeric keypad, which is the usual left-handed answer to
 * a keyboard whose letters are in the wrong place.
 */
#define KEYBIND_NAME_MAX 6

#define CHUCK_KEY_LIST(ROW)                                                 \
    ROW(A, 4, "A") ROW(B, 5, "B") ROW(C, 6, "C") ROW(D, 7, "D")             \
    ROW(E, 8, "E") ROW(F, 9, "F") ROW(G, 10, "G") ROW(H, 11, "H")           \
    ROW(I, 12, "I") ROW(J, 13, "J") ROW(K, 14, "K") ROW(L, 15, "L")         \
    ROW(M, 16, "M") ROW(N, 17, "N") ROW(O, 18, "O") ROW(P, 19, "P")         \
    ROW(Q, 20, "Q") ROW(R, 21, "R") ROW(S, 22, "S") ROW(T, 23, "T")         \
    ROW(U, 24, "U") ROW(V, 25, "V") ROW(W, 26, "W") ROW(X, 27, "X")         \
    ROW(Y, 28, "Y") ROW(Z, 29, "Z")                                         \
    ROW(1, 30, "1") ROW(2, 31, "2") ROW(3, 32, "3") ROW(4, 33, "4")         \
    ROW(5, 34, "5") ROW(6, 35, "6") ROW(7, 36, "7") ROW(8, 37, "8")         \
    ROW(9, 38, "9") ROW(0, 39, "0")                                         \
    ROW(TAB, 43, "TAB") ROW(SPACE, 44, "SPACE")                             \
    ROW(MINUS, 45, "-") ROW(EQUALS, 46, "=")                                \
    ROW(LEFTBRACKET, 47, "[") ROW(RIGHTBRACKET, 48, "]")                    \
    ROW(BACKSLASH, 49, "\\") ROW(SEMICOLON, 51, ";")                        \
    ROW(APOSTROPHE, 52, "'") ROW(GRAVE, 53, "`")                            \
    ROW(COMMA, 54, ",") ROW(PERIOD, 55, ".") ROW(SLASH, 56, "/")            \
    ROW(RIGHT, 79, "RIGHT") ROW(LEFT, 80, "LEFT")                           \
    ROW(DOWN, 81, "DOWN") ROW(UP, 82, "UP")                                 \
    ROW(KP_DIVIDE, 84, "KP /") ROW(KP_MULTIPLY, 85, "KP *")                 \
    ROW(KP_MINUS, 86, "KP -") ROW(KP_PLUS, 87, "KP +")                      \
    ROW(KP_1, 89, "KP 1") ROW(KP_2, 90, "KP 2") ROW(KP_3, 91, "KP 3")       \
    ROW(KP_4, 92, "KP 4") ROW(KP_5, 93, "KP 5") ROW(KP_6, 94, "KP 6")       \
    ROW(KP_7, 95, "KP 7") ROW(KP_8, 96, "KP 8") ROW(KP_9, 97, "KP 9")       \
    ROW(KP_0, 98, "KP 0") ROW(KP_PERIOD, 99, "KP .")                        \
    ROW(LCTRL, 224, "LCTRL") ROW(LSHIFT, 225, "LSHIFT")                     \
    ROW(LALT, 226, "LALT") ROW(RCTRL, 228, "RCTRL")                         \
    ROW(RSHIFT, 229, "RSHIFT") ROW(RALT, 230, "RALT")

/*
 * And the same for whatever is plugged in.
 *
 * The keyboard has had nine rows and two slots each for a long time while the
 * pad had a layout welded into the source, which is the exact asymmetry this
 * file's own opening paragraph objects to: "a sheet with three assist switches
 * on it still answered *can I move the jump key* with no" was true of the pad
 * for as long as the sheet has existed.
 *
 * **Stored by position, printed by letter**, which is the one thing that must
 * not be got backwards here. [pad_hint.h](pad_hint.h) exists because SDL names
 * a face button by where it sits while a player reads the letter printed on
 * it, and a Switch pad prints A where an Xbox pad prints B. So a binding is
 * kept as the position — a settings file carried between the two pads keeps
 * meaning the same physical button under the same thumb — and the display
 * column is a `pad_hint` template that the sheet expands for whatever is in
 * the player's hands. `PADBIND_NAME_MAX` is measured against the widest
 * spelling any pad can produce, exactly as `KEYBIND_NAME_MAX` is.
 *
 * `ROW(ident, button, file_name, shown)`. `file_name` is positional and
 * permanent; `shown` is a template for `pad_hint`.
 *
 * START, BACK and GUIDE are absent for the reason ESC, ENTER and BACKSPACE
 * are: START is pause and the other two are how a pad gets back out of a sheet
 * it has just made unusable. The paddles, the touchpad and MISC1 are absent
 * for a different reason — most pads do not have them, and a sheet that offers
 * a button the hardware lacks is a sheet offering a way to unbind an action by
 * accident.
 */
#define PADBIND_NONE (-1)
#define PADBIND_NAME_MAX 5

#define CHUCK_PAD_LIST(ROW)                                                 \
    ROW(SOUTH, 0, "SOUTH", "$A")                                            \
    ROW(EAST, 1, "EAST", "$B")                                              \
    ROW(WEST, 2, "WEST", "$X")                                              \
    ROW(NORTH, 3, "NORTH", "$Y")                                            \
    ROW(LEFT_STICK, 7, "LSTICK", "LS")                                      \
    ROW(RIGHT_STICK, 8, "RSTICK", "RS")                                     \
    ROW(LEFT_SHOULDER, 9, "LB", "$LB")                                      \
    ROW(RIGHT_SHOULDER, 10, "RB", "$RB")                                    \
    ROW(DPAD_UP, 11, "DPAD_UP", "DP UP")                                    \
    ROW(DPAD_DOWN, 12, "DPAD_DOWN", "DP DN")                                \
    ROW(DPAD_LEFT, 13, "DPAD_LEFT", "DP LF")                                \
    ROW(DPAD_RIGHT, 14, "DPAD_RIGHT", "DP RT")

/*
 * The whole binding table: `keys[action][slot]` as scancodes or
 * `KEYBIND_NONE`, and `pad[action][slot]` as buttons or `PADBIND_NONE`.
 *
 * Two pad slots rather than one, and it is the shipped layout that decides
 * that rather than symmetry for its own sake: attack has always answered both
 * the button lettered X and the one lettered B, because those are the two a
 * trigger finger finds and leaving B inert mid-sector read as a dead button.
 * One slot would have had to either drop that or hard-code it beside the
 * table, and a control that is half in the table is the arrangement this file
 * exists to end.
 */
typedef struct
{
    int keys[BIND_COUNT][BIND_SLOTS];
    int pad[BIND_COUNT][BIND_SLOTS];
} KeyBindings;

/*
 * Where the pad's chips sit in the row's run of caps, and how many there are.
 *
 * The sheet walks one cursor across all four, so the pad is simply the slots
 * after the last key rather than a mode of its own — which is what keeps the
 * arming, the clearing and the reset row from each needing a second branch.
 */
#define BIND_PAD_SLOT BIND_SLOTS
#define BIND_TOTAL_SLOTS (BIND_SLOTS * 2)

void keybind_defaults(KeyBindings *bindings);

/* What the sheet prints for an action, and what the file files it under —
 * once for the keyboard row and once for the pad row. */
const char *keybind_action_label(BindAction action);
const char *keybind_action_file_key(BindAction action);
const char *keybind_action_pad_file_key(BindAction action);

/* The printed name of a key, or "" for `KEYBIND_NONE` and for anything not on
 * the bindable list. Never NULL, so a caller may print it directly. */
const char *keybind_key_name(int scancode);

/* And the way back, for the file. `KEYBIND_NONE` when the name is not one this
 * build knows, which a settings file from a newer one may well contain. */
int keybind_key_from_name(const char *name, size_t length);

/* Whether a key may be bound at all. This is what refuses ESC, ENTER and
 * BACKSPACE — see the note at the top. */
bool keybind_is_bindable(int scancode);

/*
 * The pad's three, mirroring the three above.
 *
 * `keybind_pad_name` hands back the `pad_hint` template rather than a finished
 * word, because what a face button is *called* depends on the pad and this
 * file links no SDL. The sheet runs the answer through `pad_hint`, which is
 * the same call every other prompt in the game already makes.
 * `keybind_pad_file_name` is the positional spelling the settings file keeps.
 * Both are "" for `PADBIND_NONE` and for anything off the list, never NULL.
 */
const char *keybind_pad_name(int button);
const char *keybind_pad_file_name(int button);
int keybind_pad_from_file_name(const char *name, size_t length);
bool keybind_pad_is_bindable(int button);

/* Put `button` in `action`'s pad `slot`, taking it off whoever had it. Same
 * one-button-one-job rule the keyboard keeps, and for the same reason. Returns
 * false and changes nothing when the button is not bindable or the slot is out
 * of range. */
bool keybind_set_pad(KeyBindings *bindings, BindAction action, int slot,
                     int button);

/* True when `button` is one of the buttons `action` answers to. */
bool keybind_action_has_pad(const KeyBindings *bindings, BindAction action,
                            int button);

/*
 * The four face positions, in `PadFace` order, which is the bridge between a
 * binding and the pad in the player's hands.
 *
 * **A face binding is stored as a letter, spelled as a letter, and only turned
 * into a position at the moment it is read**, because a Switch pad prints A
 * where an Xbox pad prints B — the whole reason [pad_hint.h](pad_hint.h)
 * exists. Storing the raw position a Switch player pressed would put jump on
 * the button printed B for everybody who later opened the same file on an Xbox
 * pad, which is the bug this game already fixed once.
 *
 * So the canonical (Xbox) positions below stand for the letters, `game_input.c`
 * resolves them through `pad_hints_button` when the pad is read, and a capture
 * goes the other way through `pad_hints_face`. Everything that is not a face —
 * the d-pad, the bumpers, the stick clicks — sits in the same place on every
 * pad and is stored as itself.
 *
 * Returns -1 for a button that carries no letter.
 */
int keybind_pad_face_index(int button);
int keybind_pad_face_button(int face_index);

/* True when `scancode` is one of the keys `action` answers to. */
bool keybind_action_has(const KeyBindings *bindings, BindAction action,
                        int scancode);

/*
 * Put `scancode` in `action`'s `slot`, and take it away from wherever else it
 * was.
 *
 * One key does one job: a binding that quietly leaves the old owner in place is
 * a key that fires two actions, which on a keyboard is indistinguishable from
 * the game being broken. Clearing the old owner can leave an action with no
 * keys at all, and that is allowed — it is a thing the player did, it is
 * visible on the sheet as an empty chip, and the reset row is beside it.
 *
 * Returns false, changing nothing, when the key is not bindable or the slot is
 * out of range.
 */
bool keybind_set(KeyBindings *bindings, BindAction action, int slot,
                 int scancode);

#endif /* CHUCK_KEYBIND_H */
